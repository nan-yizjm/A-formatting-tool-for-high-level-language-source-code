// SourceFormatter实现。所有输出均来自AST，不再读取原始源文件。
#include "source_formatter.h"

#include <sstream>
#include <stdexcept>

SourceFormatter::SourceFormatter(std::ostream& output, int indentWidth)
    : out_(output), indentWidth_(indentWidth) {}

void SourceFormatter::indent()
{
    out_ << std::string(indentLevel_ * indentWidth_, ' ');
}

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

std::string formatSource(const ASTNode& root, int indentWidth)
{
    std::ostringstream output;
    SourceFormatter formatter(output, indentWidth);
    formatter.format(root);
    return output.str();
}
