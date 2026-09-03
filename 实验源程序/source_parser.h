// 递归下降语法分析器：校验Token序列并构造AST。
#pragma once

#include "ast.h"
#include "source_lexer.h"

#include <string>
#include <vector>

struct Diagnostic
{
    std::string stage;
    std::string message;
    int line;
    int column;
    std::string tokenText;
};

class SourceParser
{
public:
    explicit SourceParser(const std::vector<Token>& tokens);
    ASTPtr parse();
    const std::vector<Diagnostic>& diagnostics() const;

private:
    const std::vector<Token>& tokens_;
    std::size_t current_ = 0;
    std::vector<Diagnostic> diagnostics_;
    // 夹在语法元素之间的注释先暂存，随后挂到最近的安全语句块。
    std::vector<ASTPtr> deferredComments_;

    const Token& peek(std::size_t offset = 0) const;
    const Token& previous() const;
    bool atEnd() const;
    bool check(TokenKind kind);
    bool match(TokenKind kind);
    const Token& advance();
    const Token& consume(TokenKind kind, const std::string& message);
    [[noreturn]] void fail(const Token& token, const std::string& message);
    bool rawComment() const;
    ASTPtr takeRawComment();
    void skipComments();
    void flushDeferred(ASTNode& parent);
    ASTPtr parseControlledStatement();

    bool isDeclarationStart();
    std::string parseType();
    ASTPtr parseExternal();
    ASTPtr parseVariableDeclaration(const std::string& type,
                                    const Token& firstName,
                                    bool consumeSemicolon);
    ASTPtr parseDeclarator(const Token& name);
    ASTPtr parseParameterList();
    ASTPtr parseCompoundStatement();
    ASTPtr parseStatement();
    ASTPtr parseIfStatement();
    ASTPtr parseWhileStatement();
    ASTPtr parseForStatement();
    ASTPtr parseReturnStatement();

    // 以下函数从低到高实现表达式优先级。
    ASTPtr parseExpression();
    ASTPtr parseAssignment();
    ASTPtr parseLogicalOr();
    ASTPtr parseLogicalAnd();
    ASTPtr parseEquality();
    ASTPtr parseRelational();
    ASTPtr parseAdditive();
    ASTPtr parseMultiplicative();
    ASTPtr parseUnary();
    ASTPtr parsePostfix();
    ASTPtr parsePrimary();
    ASTPtr makeBinary(const Token& op, ASTPtr left, ASTPtr right);
};

void printAst(const ASTNode& root);
std::size_t countAstNodes(const ASTNode& root);
