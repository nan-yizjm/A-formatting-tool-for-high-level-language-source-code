// 抽象语法树的数据模型。
// AST使用unique_ptr独占子节点，整棵树会随根节点自动释放。
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class NodeKind
{
    // 节点按“程序结构、声明、语句、表达式、叶子”分组。
    Program, Preprocessor, Comment,
    VariableDeclaration, Declarator, ArrayDimension, Initializer,
    FunctionDeclaration, FunctionDefinition, ParameterList, Parameter,
    CompoundStatement, ExpressionStatement, IfStatement, WhileStatement,
    ForStatement, ReturnStatement, BreakStatement, ContinueStatement,
    EmptyStatement, BinaryExpression, UnaryExpression, CallExpression,
    ArgumentList, SubscriptExpression, Identifier, IntegerLiteral,
    FloatingLiteral, CharacterLiteral, StringLiteral, EmptyExpression
};

struct ASTNode
{
    NodeKind kind;
    std::string value;
    int line;
    int column;
    std::vector<std::unique_ptr<ASTNode>> children;

    // 使用节点类别、内容和源位置创建一个AST节点。
    // value通常保存类型名、标识符、常量或运算符，children保存语法结构关系。
    ASTNode(NodeKind nodeKind, std::string nodeValue = {},
            int sourceLine = 0, int sourceColumn = 0)
        : kind(nodeKind), value(std::move(nodeValue)),
          line(sourceLine), column(sourceColumn) {}

    // 接管子节点所有权，并返回不拥有对象的观察指针。
    // 调用方不得释放返回指针；子节点生命周期完全由父节点控制。
    ASTNode* add(std::unique_ptr<ASTNode> child)
    {
        // 返回观察指针便于调用者继续访问；所有权仍保存在children中。
        ASTNode* result = child.get();
        children.push_back(std::move(child));
        return result;
    }
};

using ASTPtr = std::unique_ptr<ASTNode>;

// 简化AST节点创建，统一返回独占所有权指针。
// 解析器通过本函数统一构建节点，可避免漏填源位置或误用裸指针。
inline ASTPtr makeNode(NodeKind kind, const std::string& value = {},
                       int line = 0, int column = 0)
{
    return std::make_unique<ASTNode>(kind, value, line, column);
}

const char* nodeKindName(NodeKind kind);
