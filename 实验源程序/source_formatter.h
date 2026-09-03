// AST格式化器接口：只依赖AST生成统一风格的C源代码。
#pragma once

#include "ast.h"

#include <ostream>
#include <string>

class SourceFormatter
{
public:
    explicit SourceFormatter(std::ostream& output, int indentWidth = 4);
    void format(const ASTNode& root);

private:
    std::ostream& out_;
    int indentLevel_ = 0;
    int indentWidth_ = 4;

    void indent();
    void formatTopLevel(const ASTNode& node);
    void formatStatement(const ASTNode& node);
    void formatControlledBody(const ASTNode& node);
    void formatDeclaration(const ASTNode& node, bool withSemicolon);
    void formatDeclarator(const ASTNode& node);
    void formatParameters(const ASTNode& node);
    // parentPrecedence用于判断当前表达式是否需要补圆括号。
    void formatExpression(const ASTNode& node, int parentPrecedence = 0);
    void formatComment(const std::string& text);
    int precedence(const ASTNode& node) const;
};

std::string formatSource(const ASTNode& root, int indentWidth = 4);
