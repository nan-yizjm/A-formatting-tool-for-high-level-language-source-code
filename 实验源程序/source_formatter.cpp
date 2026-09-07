// SourceFormatter实现。所有输出均来自AST，不再读取原始源文件。
#include "source_formatter.h"

#include <sstream>
#include <stdexcept>

// 绑定输出流并设置每一级缩进所使用的空格数。
// 输出流由调用方管理，格式化器只负责向其中写入文本，不负责打开或关闭文件。
SourceFormatter::SourceFormatter(std::ostream& output, int indentWidth)
    : out_(output), indentWidth_(indentWidth) {}

// 输出当前缩进级别对应的空格。
// 缩进宽度默认是4，因此indentLevel_为2时会输出8个空格。
void SourceFormatter::indent()
{
    out_ << std::string(indentLevel_ * indentWidth_, ' ');
}

// 返回表达式节点的优先级数值，数值越大优先级越高。
// 该顺序必须与递归下降解析器一致，否则格式化时可能错误省略圆括号。
int SourceFormatter::precedence(const ASTNode& node) const
{
    // 数值越大表示结合越紧，与解析器的表达式层次保持一致。
    if (node.kind == NodeKind::CallExpression ||
        node.kind == NodeKind::SubscriptExpression) return 9;
    if (node.kind == NodeKind::UnaryExpression) return 8;
    if (node.kind != NodeKind::BinaryExpression) return 10;
    if (node.value == "=") return 1;
    if (node.value == "||") return 2;
    if (node.value == "&&") return 3;
    if (node.value == "==" || node.value == "!=") return 4;
    if (node.value == ">" || node.value == "<" ||
        node.value == ">=" || node.value == "<=") return 5;
    if (node.value == "+" || node.value == "-") return 6;
    if (node.value == "*" || node.value == "/" || node.value == "%") return 7;
    return 0;
}

// 递归输出表达式，并在必要时补充圆括号保持原有语义。
// parentPrecedence来自父表达式；当前节点优先级更低时必须包裹圆括号。
void SourceFormatter::formatExpression(const ASTNode& node, int parentPrecedence)
{
    switch (node.kind)
    {
    case NodeKind::Identifier:
    case NodeKind::IntegerLiteral:
    case NodeKind::FloatingLiteral:
    case NodeKind::CharacterLiteral:
    case NodeKind::StringLiteral:
        out_ << node.value;
        return;

    case NodeKind::EmptyExpression:
        return;

    case NodeKind::UnaryExpression:
    {
        const int current = precedence(node);
        const bool parentheses = current < parentPrecedence;
        if (parentheses) out_ << '(';
        out_ << node.value;
        formatExpression(*node.children.at(0), current);
        if (parentheses) out_ << ')';
        return;
    }

    case NodeKind::CallExpression:
    {
        const int current = precedence(node);
        const bool parentheses = current < parentPrecedence;
        if (parentheses) out_ << '(';
        formatExpression(*node.children.at(0), current);
        out_ << '(';
        const ASTNode& arguments = *node.children.at(1);
        for (std::size_t i = 0; i < arguments.children.size(); ++i)
        {
            if (i != 0) out_ << ", ";
            formatExpression(*arguments.children[i]);
        }
        out_ << ')';
        if (parentheses) out_ << ')';
        return;
    }

    case NodeKind::SubscriptExpression:
    {
        const int current = precedence(node);
        const bool parentheses = current < parentPrecedence;
        if (parentheses) out_ << '(';
        formatExpression(*node.children.at(0), current);
        out_ << '[';
        formatExpression(*node.children.at(1));
        out_ << ']';
        if (parentheses) out_ << ')';
        return;
    }

    case NodeKind::BinaryExpression:
    {
        const int current = precedence(node);
        const bool parentheses = current < parentPrecedence;
        if (parentheses) out_ << '(';
        formatExpression(*node.children.at(0), current);
        out_ << ' ' << node.value << ' ';
        // 非赋值运算的右子树使用更高的父优先级，保留a-(b-c)等语义。
        formatExpression(*node.children.at(1),
                         node.value == "=" ? current : current + 1);
        if (parentheses) out_ << ')';
        return;
    }

    default:
        throw std::runtime_error("格式化器收到非法表达式节点");
    }
}

// 输出变量名、数组维度和可选初始化器组成的声明符。
// 数组维度先输出，初始化器最后输出，确保a[10] = 1等结构顺序正确。
void SourceFormatter::formatDeclarator(const ASTNode& node)
{
    // AST中维度和初始化器都是声明符子节点，分两遍保证初始化始终在末尾。
    out_ << node.value;
    for (const auto& child : node.children)
    {
        if (child->kind == NodeKind::ArrayDimension)
        {
            out_ << '[';
            if (!child->children.empty()) formatExpression(*child->children[0]);
            out_ << ']';
        }
    }
    for (const auto& child : node.children)
    {
        if (child->kind == NodeKind::Initializer)
        {
            out_ << " = ";
            if (!child->children.empty()) formatExpression(*child->children[0]);
        }
    }
}

// 输出一个变量声明，可按需要决定是否追加分号。
// for语句初始化子句复用该函数，但此时不能额外输出分号。
void SourceFormatter::formatDeclaration(const ASTNode& node, bool withSemicolon)
{
    out_ << node.value << ' ';
    for (std::size_t i = 0; i < node.children.size(); ++i)
    {
        if (i != 0) out_ << ", ";
        formatDeclarator(*node.children[i]);
    }
    if (withSemicolon) out_ << ';';
}

// 输出函数形参列表；空列表统一格式化为void。
// 统一写为void能够明确表示函数不接受参数，而不是旧式C的未知参数列表。
void SourceFormatter::formatParameters(const ASTNode& node)
{
    if (node.children.empty())
    {
        out_ << "void";
        return;
    }
    for (std::size_t i = 0; i < node.children.size(); ++i)
    {
        if (i != 0) out_ << ", ";
        const ASTNode& parameter = *node.children[i];
        out_ << parameter.value << ' ';
        formatDeclarator(*parameter.children.at(0));
    }
}

// 逐行输出注释原文，并为多行注释补齐当前缩进。
// 注释内容不作重写，避免破坏用户在注释中使用的示例、对齐或中文文本。
void SourceFormatter::formatComment(const std::string& text)
{
    std::istringstream lines(text);
    std::string line;
    bool wrote = false;
    while (std::getline(lines, line))
    {
        if (wrote) indent();
        out_ << line << '\n';
        wrote = true;
    }
    if (!wrote) out_ << '\n';
}

// 输出控制语句的执行体，必要时为单语句补上花括号。
// 即使输入是if (x) y = 1;，输出也会带花括号以减少后续修改产生的悬挂语句风险。
void SourceFormatter::formatControlledBody(const ASTNode& node)
{
    // 为单语句控制体补花括号，使输出结构一致且便于后续修改。
    if (node.kind == NodeKind::CompoundStatement)
    {
        formatStatement(node);
        return;
    }
    indent();
    out_ << "{\n";
    ++indentLevel_;
    formatStatement(node);
    --indentLevel_;
    indent();
    out_ << "}\n";
}

// 根据语句节点类别输出对应的格式化代码。
// 所有语句末尾的换行都在这里统一处理，表达式格式化函数只负责表达式本身。
void SourceFormatter::formatStatement(const ASTNode& node)
{
    switch (node.kind)
    {
    case NodeKind::CompoundStatement:
        indent();
        out_ << "{\n";
        ++indentLevel_;
        for (const auto& child : node.children) formatStatement(*child);
        --indentLevel_;
        indent();
        out_ << "}\n";
        return;

    case NodeKind::VariableDeclaration:
        indent();
        formatDeclaration(node, true);
        out_ << '\n';
        return;

    case NodeKind::ExpressionStatement:
        indent();
        formatExpression(*node.children.at(0));
        out_ << ";\n";
        return;

    case NodeKind::EmptyStatement:
        indent();
        out_ << ";\n";
        return;

    case NodeKind::ReturnStatement:
        indent();
        out_ << "return";
        if (!node.children.empty())
        {
            out_ << ' ';
            formatExpression(*node.children[0]);
        }
        out_ << ";\n";
        return;

    case NodeKind::BreakStatement:
        indent(); out_ << "break;\n"; return;
    case NodeKind::ContinueStatement:
        indent(); out_ << "continue;\n"; return;

    case NodeKind::Comment:
        indent();
        formatComment(node.value);
        return;

    case NodeKind::Preprocessor:
        out_ << node.value << '\n';
        return;

    case NodeKind::IfStatement:
        indent();
        out_ << "if (";
        formatExpression(*node.children.at(0));
        out_ << ")\n";
        formatControlledBody(*node.children.at(1));
        if (node.children.size() > 2)
        {
            indent();
            out_ << "else\n";
            formatControlledBody(*node.children.at(2));
        }
        return;

    case NodeKind::WhileStatement:
        indent();
        out_ << "while (";
        formatExpression(*node.children.at(0));
        out_ << ")\n";
        formatControlledBody(*node.children.at(1));
        return;

    case NodeKind::ForStatement:
        indent();
        out_ << "for (";
        if (node.children[0]->kind == NodeKind::VariableDeclaration)
            formatDeclaration(*node.children[0], false);
        else
            formatExpression(*node.children[0]);
        out_ << "; ";
        formatExpression(*node.children[1]);
        out_ << "; ";
        formatExpression(*node.children[2]);
        out_ << ")\n";
        formatControlledBody(*node.children[3]);
        return;

    default:
        throw std::runtime_error("格式化器收到非法语句节点");
    }
}

// 输出程序根节点下的一个顶层元素。
// 顶层允许预处理指令、注释、全局变量、函数声明和函数定义。
void SourceFormatter::formatTopLevel(const ASTNode& node)
{
    switch (node.kind)
    {
    case NodeKind::Preprocessor:
        out_ << node.value << '\n';
        return;
    case NodeKind::Comment:
        formatComment(node.value);
        return;
    case NodeKind::VariableDeclaration:
        formatDeclaration(node, true);
        out_ << '\n';
        return;
    case NodeKind::FunctionDeclaration:
        out_ << node.value << ' ' << node.children[0]->value << '(';
        formatParameters(*node.children[1]);
        out_ << ");\n";
        return;
    case NodeKind::FunctionDefinition:
        out_ << node.value << ' ' << node.children[0]->value << '(';
        formatParameters(*node.children[1]);
        out_ << ")\n";
        formatStatement(*node.children[2]);
        return;
    default:
        throw std::runtime_error("程序根节点包含非法顶层节点");
    }
}

// 遍历程序根节点并生成完整格式化源代码。
// 连续预处理指令不插空行，其他顶层元素间留一个空行以提高可读性。
void SourceFormatter::format(const ASTNode& root)
{
    if (root.kind != NodeKind::Program)
        throw std::runtime_error("格式化入口必须是程序节点");
    indentLevel_ = 0;
    for (std::size_t i = 0; i < root.children.size(); ++i)
    {
        formatTopLevel(*root.children[i]);
        if (i + 1 < root.children.size())
        {
            const NodeKind current = root.children[i]->kind;
            const NodeKind next = root.children[i + 1]->kind;
            // 连续预处理行紧邻输出，其余顶层结构之间保留一个空行。
            const bool directivePair = current == NodeKind::Preprocessor &&
                                       next == NodeKind::Preprocessor;
            if (!directivePair) out_ << '\n';
        }
    }
}

// 创建字符串输出流并返回格式化后的完整源代码文本。
// main.cpp既可将返回文本写入文件，也可直接用于终端预览。
std::string formatSource(const ASTNode& root, int indentWidth)
{
    std::ostringstream output;
    SourceFormatter formatter(output, indentWidth);
    formatter.format(root);
    return output.str();
}
