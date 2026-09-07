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
// 返回终端颜色开关，供所有界面输出函数共享。
// 使用函数内静态变量避免头文件被多个.cpp包含时产生重复定义。
inline bool& colorsEnabled()
{
    // 函数内静态变量兼容项目使用的旧版MinGW编译器。
    static bool enabled = true;
    return enabled;
}
// 返回重置ANSI终端样式的控制字符串；关闭颜色时返回空字符串。
inline const char* reset() { return colorsEnabled() ? "\033[0m" : ""; }
// 返回红色ANSI终端样式的控制字符串；用于错误信息。
inline const char* red() { return colorsEnabled() ? "\033[31m" : ""; }
// 返回绿色ANSI终端样式的控制字符串；用于成功信息。
inline const char* green() { return colorsEnabled() ? "\033[32m" : ""; }
// 返回黄色ANSI终端样式的控制字符串；用于警告信息。
inline const char* yellow() { return colorsEnabled() ? "\033[33m" : ""; }
// 返回青色ANSI终端样式的控制字符串；用于标题和结果标题。
inline const char* cyan() { return colorsEnabled() ? "\033[36m" : ""; }
// 返回加粗ANSI终端样式的控制字符串；与颜色组合强调标题。
inline const char* bold() { return colorsEnabled() ? "\033[1m" : ""; }

// 设置UTF-8代码页并尝试启用Windows终端的ANSI颜色支持。
// 若终端不支持ANSI控制序列则自动关闭颜色，避免把转义字符直接打印出来。
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

// 清屏并将光标移动到终端左上角。
// 不支持ANSI的旧终端用输出空行的方式退化实现。
inline void clearScreen()
{
    if (colorsEnabled()) std::cout << "\033[2J\033[H";
    else std::cout << std::string(40, '\n');
}

// 输出统一宽度的界面分隔线。
// 标题、菜单和分析摘要复用该函数，保证终端版式一致。
inline void divider()
{
    std::cout << "+------------------------------------------------------------------+\n";
}

// 输出程序标题以及当前选中的源文件路径。
// 未选择文件时显示“未选择”，避免用户误以为正在分析上一个文件。
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

// 输出主菜单和各功能对应的编号。
// 菜单编号由main.cpp解释，这里仅负责统一展示，不处理业务逻辑。
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

// 以成功样式输出提示信息。
inline void success(const std::string& text)
{
    std::cout << green() << "[成功] " << reset() << text << '\n';
}

// 以警告样式输出提示信息。
inline void warning(const std::string& text)
{
    std::cout << yellow() << "[警告] " << reset() << text << '\n';
}

// 以错误样式输出提示信息。
inline void error(const std::string& text)
{
    std::cout << red() << "[错误] " << reset() << text << '\n';
}

// 输出提示文字并读取一整行用户输入。
// getline允许文件路径包含空格，不会像operator>>那样在空格处截断。
inline std::string readLine(const std::string& prompt)
{
    std::cout << prompt;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

// 读取菜单编号，并拒绝包含额外字符的输入。
// 例如“1abc”和“1.5”都会失败，防止菜单逻辑收到含糊的选择值。
inline bool readChoice(int& choice)
{
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    std::istringstream input(line);
    char extra = '\0';
    if (!(input >> choice)) return false;
    return !(input >> extra);
}

// 等待用户按下Enter，避免菜单结果被立即清屏。
// 读取整行而非单个字符，避免残留换行符影响下一次菜单输入。
inline void pause()
{
    std::cout << "\n按 Enter 返回主菜单...";
    std::string ignored;
    std::getline(std::cin, ignored);
}
}
