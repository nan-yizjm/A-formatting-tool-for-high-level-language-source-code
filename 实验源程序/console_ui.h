// 终端界面公共组件：UTF-8代码页、ANSI颜色、菜单和安全输入。
#pragma once

#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
// 手工声明少量Win32接口，避免windows.h中的宏污染Token名称。
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
extern "C" __declspec(dllimport) int __stdcall SetConsoleCP(unsigned int);
extern "C" __declspec(dllimport) void* __stdcall GetStdHandle(unsigned long);
extern "C" __declspec(dllimport) int __stdcall GetConsoleMode(void*, unsigned long*);
extern "C" __declspec(dllimport) int __stdcall SetConsoleMode(void*, unsigned long);
#endif

namespace ui
{
inline bool& colorsEnabled()
{
    // 函数内静态变量兼容项目使用的旧版MinGW编译器。
    static bool enabled = true;
    return enabled;
}
inline const char* reset() { return colorsEnabled() ? "\033[0m" : ""; }
inline const char* red() { return colorsEnabled() ? "\033[31m" : ""; }
inline const char* green() { return colorsEnabled() ? "\033[32m" : ""; }
inline const char* yellow() { return colorsEnabled() ? "\033[33m" : ""; }
inline const char* cyan() { return colorsEnabled() ? "\033[36m" : ""; }
inline const char* bold() { return colorsEnabled() ? "\033[1m" : ""; }

inline void initialize()
{
#ifdef _WIN32
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    void* output = GetStdHandle(static_cast<unsigned long>(-11));
    unsigned long mode = 0;
    if (!output || !GetConsoleMode(output, &mode) ||
        !SetConsoleMode(output, mode | 0x0004UL))
    {
        colorsEnabled() = false;
    }
#endif
}

inline void clearScreen()
{
    if (colorsEnabled()) std::cout << "\033[2J\033[H";
    else std::cout << std::string(40, '\n');
}

inline void divider()
{
    std::cout << "+------------------------------------------------------------------+\n";
}

inline void printTitle(const std::string& currentFile)
{
    std::cout << cyan() << bold();
    divider();
    std::cout << "|              C语言源程序分析与格式化工具                         |\n";
    divider();
    std::cout << reset();
    std::cout << "  当前文件: " << (currentFile.empty() ? "未选择" : currentFile) << '\n';
    divider();
}

inline void printMenu(const std::string& currentFile)
{
    printTitle(currentFile);
    std::cout
        << "  [1] 词法分析        查看Token、位置与词法错误\n"
        << "  [2] 语法分析        查看抽象语法树(AST)\n"
        << "  [3] 格式化输出      从AST生成新的C源文件\n"
        << "  [4] 格式化预览      在终端查看生成结果\n"
        << "  [5] 重新选择文件\n"
        << "  [0] 退出程序\n";
    divider();
    std::cout << "  请选择: ";
}

inline void success(const std::string& text)
{
    std::cout << green() << "[成功] " << reset() << text << '\n';
}

inline void warning(const std::string& text)
{
    std::cout << yellow() << "[警告] " << reset() << text << '\n';
}

inline void error(const std::string& text)
{
    std::cout << red() << "[错误] " << reset() << text << '\n';
}

inline std::string readLine(const std::string& prompt)
{
    std::cout << prompt;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

inline bool readChoice(int& choice)
{
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    std::istringstream input(line);
    char extra = '\0';
    if (!(input >> choice)) return false;
    return !(input >> extra);
}

inline void pause()
{
    std::cout << "\n按 Enter 返回主菜单...";
    std::string ignored;
    std::getline(std::cin, ignored);
}
}
