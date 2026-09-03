// 程序入口：组织文件读写、分析流水线和交互式终端菜单。
#include "console_ui.h"
#include "source_formatter.h"
#include "source_lexer.h"
#include "source_parser.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned int, unsigned long, const char*, int, wchar_t*, int);
extern "C" FILE* __cdecl _wfopen(const wchar_t*, const wchar_t*);
#endif

struct AnalysisResult
{
    // 一次分析的全部产物，便于各菜单项共享统一处理结果。
    std::vector<Token> tokens;
    ASTPtr ast;
    std::vector<Diagnostic> diagnostics;
};

static bool readFile(const std::string& path, std::string& content)
{
#ifdef _WIN32
    // MinGW窄字符文件流不能可靠打开中文路径，先转为UTF-16再读取。
    const int length = MultiByteToWideChar(65001, 0, path.c_str(),
                                           static_cast<int>(path.size()),
                                           nullptr, 0);
    if (length <= 0) return false;
    std::wstring widePath(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(65001, 0, path.c_str(), static_cast<int>(path.size()),
                        &widePath[0], length);

    FILE* input = _wfopen(widePath.c_str(), L"rb");
    if (!input) return false;
    content.clear();
    char buffer[8192];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), input)) != 0)
        content.append(buffer, count);
    const bool success = std::ferror(input) == 0;
    std::fclose(input);
#else
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    content = buffer.str();
    const bool success = input.good() || input.eof();
#endif

    // UTF-8 BOM is file metadata, not a C source token.
    if (success && content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF)
    {
        content.erase(0, 3);
    }
    return success;
}

static bool writeFile(const std::string& path, const std::string& content)
{
#ifdef _WIN32
    // 输出路径沿用输入文件目录，因此同样必须使用宽字符Windows接口。
    const int length = MultiByteToWideChar(65001, 0, path.c_str(),
                                           static_cast<int>(path.size()),
                                           nullptr, 0);
    if (length <= 0) return false;
    std::wstring widePath(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(65001, 0, path.c_str(), static_cast<int>(path.size()),
                        &widePath[0], length);

    FILE* output = _wfopen(widePath.c_str(), L"wb");
    if (!output) return false;
    const std::size_t written = std::fwrite(content.data(), 1, content.size(), output);
    const bool success = written == content.size() && std::fclose(output) == 0;
    return success;
#else
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
#endif
}

static std::string normalizePath(std::string path)
{
    if (path.size() >= 2 &&
        ((path.front() == '"' && path.back() == '"') ||
         (path.front() == '\'' && path.back() == '\'')))
    {
        path = path.substr(1, path.size() - 2);
    }
    return path;
}

static std::string outputPathFor(const std::string& input)
{
    const std::size_t slash = input.find_last_of("/\\");
    const std::size_t dot = input.find_last_of('.');
    if (dot == std::string::npos ||
        (slash != std::string::npos && dot < slash))
    {
        return input + "_formatted.c";
    }
    return input.substr(0, dot) + "_formatted.c";
}

static int sourceLineCount(const std::string& source)
{
    if (source.empty()) return 0;
    const int newlines = static_cast<int>(std::count(source.begin(), source.end(), '\n'));
    return newlines + (source.back() == '\n' ? 0 : 1);
}

static AnalysisResult analyze(const std::string& source)
{
    // 词法错误存在时不启动解析器，防止非法Token引发误导性的语法错误。
    AnalysisResult result;
    SourceLexer lexer(source);
    result.tokens = lexer.scan();

    for (const Token& token : result.tokens)
    {
        if (token.kind == TokenKind::Invalid)
        {
            result.diagnostics.push_back({"词法分析", token.message,
                                          token.line, token.column, token.text});
        }
    }

    if (!result.diagnostics.empty()) return result;

    SourceParser parser(result.tokens);
    result.ast = parser.parse();
    result.diagnostics = parser.diagnostics();
    return result;
}

static int visualWidth(const std::string& text)
{
    // 终端中常用汉字通常占两列，ASCII占一列。
    int width = 0;
    for (std::size_t i = 0; i < text.size();)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80)
        {
            ++width;
            ++i;
        }
        else
        {
            width += 2;
            if ((c & 0xE0) == 0xC0) i += 2;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else if ((c & 0xF8) == 0xF0) i += 4;
            else ++i;
        }
    }
    return width;
}

static std::string oneLine(const std::string& text)
{
    std::string escaped;
    for (char c : text)
    {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else escaped += c;
    }

    if (visualWidth(escaped) <= 42) return escaped;
    // 按UTF-8字符边界截断，不能直接substr字节，否则可能产生乱码。
    std::string result;
    int width = 0;
    for (std::size_t i = 0; i < escaped.size();)
    {
        const unsigned char c = static_cast<unsigned char>(escaped[i]);
        std::size_t bytes = 1;
        int characterWidth = 1;
        if ((c & 0xE0) == 0xC0) { bytes = 2; characterWidth = 2; }
        else if ((c & 0xF0) == 0xE0) { bytes = 3; characterWidth = 2; }
        else if ((c & 0xF8) == 0xF0) { bytes = 4; characterWidth = 2; }
        if (width + characterWidth > 39) break;
        result.append(escaped, i, bytes);
        width += characterWidth;
        i += bytes;
    }
    return result + "...";
}

static std::string padded(const std::string& text, int width)
{
    const int padding = std::max(0, width - visualWidth(text));
    return text + std::string(static_cast<std::size_t>(padding), ' ');
}

static void printTokenTable(const std::vector<Token>& tokens)
{
    std::cout << "+------+----------+------------------+--------------------------------------------+\n"
              << "| " << padded("序号", 4)
              << " | " << padded("位置", 8)
              << " | " << padded("Token类别", 16)
              << " | " << padded("Token原文", 42) << " |\n"
              << "+------+----------+------------------+--------------------------------------------+\n";
    int number = 1;
    for (const Token& token : tokens)
    {
        if (token.kind == TokenKind::EndOfFile) continue;
        std::ostringstream position;
        position << token.line << ':' << token.column;
        std::cout << "| " << std::setw(4) << std::left << number++
                  << " | " << padded(position.str(), 8)
                  << " | " << padded(tokenKindName(token.kind), 16)
                  << " | " << padded(oneLine(token.text), 42) << " |\n";
    }
    std::cout << "+------+----------+------------------+--------------------------------------------+\n";
}

static std::string lineAt(const std::string& source, int requestedLine)
{
    std::istringstream input(source);
    std::string line;
    for (int number = 1; std::getline(input, line); ++number)
    {
        if (number == requestedLine)
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
    }
    return {};
}

static void printDiagnostics(const std::vector<Diagnostic>& diagnostics,
                             const std::string& source)
{
    // 同时显示结构化位置和源代码指示线，便于直接定位输入错误。
    for (const Diagnostic& diagnostic : diagnostics)
    {
        std::cout << '\n' << ui::red() << "[" << diagnostic.stage << "错误] "
                  << ui::reset() << "第" << diagnostic.line << "行，第"
                  << diagnostic.column << "列: " << diagnostic.message << '\n';
        const std::string line = lineAt(source, diagnostic.line);
        if (!line.empty())
        {
            std::cout << std::setw(5) << diagnostic.line << " | " << line << '\n'
                      << "      | "
                      << std::string(static_cast<std::size_t>(
                             std::max(0, diagnostic.column - 1)), ' ')
                      << ui::red() << '^' << ui::reset() << '\n';
        }
    }
}

static void printSummary(const std::string& source,
                         const AnalysisResult& result)
{
    std::size_t tokenCount = 0;
    for (const Token& token : result.tokens)
        if (token.kind != TokenKind::EndOfFile) ++tokenCount;

    std::cout << "\n分析摘要\n";
    ui::divider();
    std::cout << "  代码行数: " << sourceLineCount(source) << '\n'
              << "  Token数 : " << tokenCount << '\n'
              << "  AST节点 : " << (result.ast ? countAstNodes(*result.ast) : 0) << '\n'
              << "  错误数  : " << result.diagnostics.size() << '\n';
    ui::divider();
}

static bool selectFile(std::string& filename, std::string& source)
{
    while (true)
    {
        ui::clearScreen();
        ui::printTitle(filename);
        const std::string input = ui::readLine("请输入C源文件路径（输入0退出）: ");
        if (!std::cin || input == "0") return false;
        filename = normalizePath(input);
        if (readFile(filename, source))
        {
            ui::success("文件加载成功，共" +
                        std::to_string(sourceLineCount(source)) + "行");
            return true;
        }
        ui::error("无法打开文件，请检查路径或访问权限");
        ui::pause();
    }
}

int main()
{
    ui::initialize();
    std::string filename;
    std::string source;
    if (!selectFile(filename, source)) return 0;

    while (true)
    {
        ui::clearScreen();
        ui::printMenu(filename);
        int choice = -1;
        if (!ui::readChoice(choice))
        {
            if (!std::cin) return 0;
            ui::error("请输入0到5之间的整数");
            ui::pause();
            continue;
        }
        if (choice == 0) return 0;
        if (choice == 5)
        {
            if (!selectFile(filename, source)) return 0;
            continue;
        }
        if (choice < 1 || choice > 5)
        {
            ui::error("请输入0到5之间的整数");
            ui::pause();
            continue;
        }

        // 每次操作重新读取文件，确保能看到用户刚保存的修改。
        if (!readFile(filename, source))
        {
            ui::error("源文件已无法读取，请重新选择文件");
            ui::pause();
            continue;
        }

        ui::clearScreen();
        AnalysisResult result = analyze(source);

        if (choice == 1)
        {
            std::cout << ui::cyan() << "词法分析结果\n" << ui::reset();
            printTokenTable(result.tokens);
            if (result.diagnostics.empty()) ui::success("词法分析通过");
            else printDiagnostics(result.diagnostics, source);
            printSummary(source, result);
        }
        else if (choice == 2)
        {
            if (!result.ast)
            {
                printDiagnostics(result.diagnostics, source);
                ui::error("存在错误，无法建立AST");
            }
            else
            {
                std::cout << ui::cyan() << "抽象语法树（前序遍历）\n"
                          << ui::reset();
                printAst(*result.ast);
                ui::success("语法分析通过");
                printSummary(source, result);
            }
        }
        else if (choice == 3 || choice == 4)
        {
            if (!result.ast)
            {
                printDiagnostics(result.diagnostics, source);
                ui::error("存在错误，不能生成格式化代码");
            }
            else
            {
                const std::string formatted = formatSource(*result.ast, 4);
                if (choice == 3)
                {
                    const std::string outputPath = outputPathFor(filename);
                    if (writeFile(outputPath, formatted))
                    {
                        ui::success("格式化文件已生成");
                        std::cout << "  输出路径: " << outputPath << '\n'
                                  << "  缩进规则: 每层4个空格\n";
                    }
                    else ui::error("无法写入格式化文件");
                }
                else
                {
                    std::cout << ui::cyan() << "格式化预览\n" << ui::reset();
                    ui::divider();
                    std::cout << formatted;
                    ui::divider();
                }
            }
        }
        ui::pause();
    }
}
