// SourceParser实现。每个parseXxx函数对应一条语法产生式。
#include "source_parser.h"

#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
// 首个语法错误即中止本次建树，避免在不完整AST上继续产生级联错误。
struct ParseAbort {};

bool typeModifier(TokenKind kind)
{
    return isTypeKeyword(kind) || kind == TokenKind::KwConst ||
           kind == TokenKind::KwStatic || kind == TokenKind::KwExtern ||
           kind == TokenKind::KwRegister;
}

void printAstNode(const ASTNode& node, const std::string& prefix, bool last)
{
    std::cout << prefix << (last ? "`-- " : "|-- ") << nodeKindName(node.kind);
    if (!node.value.empty()) std::cout << ": " << node.value;
    if (node.line > 0) std::cout << "  [" << node.line << ':' << node.column << ']';
    std::cout << '\n';

    const std::string childPrefix = prefix + (last ? "    " : "|   ");
    for (std::size_t i = 0; i < node.children.size(); ++i)
        printAstNode(*node.children[i], childPrefix, i + 1 == node.children.size());
}
}

SourceParser::SourceParser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& SourceParser::peek(std::size_t offset) const
{
    const std::size_t index = current_ + offset;
    return tokens_[index < tokens_.size() ? index : tokens_.size() - 1];
}

const Token& SourceParser::previous() const { return tokens_[current_ - 1]; }
bool SourceParser::atEnd() const { return peek().kind == TokenKind::EndOfFile; }

bool SourceParser::rawComment() const
{
    return peek().kind == TokenKind::LineComment ||
           peek().kind == TokenKind::BlockComment;
}

ASTPtr SourceParser::takeRawComment()
{
    const Token token = advance();
    return makeNode(NodeKind::Comment, token.text, token.line, token.column);
}

void SourceParser::skipComments()
{
    // C标准把注释视为空白，但格式化工具还需要保存其原文。
    while (rawComment()) deferredComments_.push_back(takeRawComment());
}

void SourceParser::flushDeferred(ASTNode& parent)
{
    for (auto& comment : deferredComments_) parent.add(std::move(comment));
    deferredComments_.clear();
}

bool SourceParser::check(TokenKind kind)
{
    skipComments();
    return peek().kind == kind;
}

bool SourceParser::match(TokenKind kind)
{
    if (!check(kind)) return false;
    advance();
    return true;
}

const Token& SourceParser::advance()
{
    if (!atEnd()) ++current_;
    return previous();
}

const Token& SourceParser::consume(TokenKind kind, const std::string& message)
{
    if (check(kind)) return advance();
    fail(peek(), message);
}

[[noreturn]] void SourceParser::fail(const Token& token, const std::string& message)
{
    diagnostics_.push_back({"语法分析", message, token.line, token.column, token.text});
    throw ParseAbort{};
}

const std::vector<Diagnostic>& SourceParser::diagnostics() const
{
    return diagnostics_;
}

ASTPtr SourceParser::parse()
{
    auto program = makeNode(NodeKind::Program, "", 1, 1);
    try
    {
        while (!atEnd())
        {
            program->add(parseExternal());
            flushDeferred(*program);
        }
    }
    catch (const ParseAbort&)
    {
        return nullptr;
    }
    return program;
}

bool SourceParser::isDeclarationStart()
{
    skipComments();
    return typeModifier(peek().kind);
}

std::string SourceParser::parseType()
{
    if (!isDeclarationStart()) fail(peek(), "此处需要类型说明符");
    std::string type;
    bool hasBaseType = false;
    while (true)
    {
        skipComments();
        if (!typeModifier(peek().kind)) break;
        const Token token = advance();
        if (isTypeKeyword(token.kind)) hasBaseType = true;
        if (!type.empty()) type += ' ';
        type += token.text;
    }
    if (!hasBaseType) fail(peek(), "声明中缺少基本类型");
    return type;
}

ASTPtr SourceParser::parseExternal()
{
    // 文件作用域允许预处理、注释、变量声明、函数声明和函数定义。
    if (rawComment()) return takeRawComment();
    if (match(TokenKind::Preprocessor))
        return makeNode(NodeKind::Preprocessor, previous().text,
                        previous().line, previous().column);
    if (!isDeclarationStart()) fail(peek(), "文件作用域只允许声明、函数、注释或预处理指令");
    const Token start = peek();
    const std::string type = parseType();
    const Token& name = consume(TokenKind::Identifier, "类型后需要变量名或函数名");

    if (!match(TokenKind::LeftParen))
        return parseVariableDeclaration(type, name, true);

    auto parameters = parseParameterList();
    consume(TokenKind::RightParen, "函数参数列表后缺少右圆括号')'");

    if (match(TokenKind::Semicolon))
    {
        auto function = makeNode(NodeKind::FunctionDeclaration, type, start.line, start.column);
        function->add(makeNode(NodeKind::Identifier, name.text, name.line, name.column));
        function->add(std::move(parameters));
        return function;
    }

    if (!check(TokenKind::LeftBrace)) fail(peek(), "函数头后需要分号或函数体");
    std::vector<ASTPtr> headerComments = std::move(deferredComments_);
    deferredComments_.clear();
    auto function = makeNode(NodeKind::FunctionDefinition, type, start.line, start.column);
    function->add(makeNode(NodeKind::Identifier, name.text, name.line, name.column));
    function->add(std::move(parameters));
    auto body = parseCompoundStatement();
    if (!headerComments.empty())
    {
        body->children.insert(body->children.begin(),
                              std::make_move_iterator(headerComments.begin()),
                              std::make_move_iterator(headerComments.end()));
    }
    function->add(std::move(body));
    return function;
}

ASTPtr SourceParser::parseDeclarator(const Token& name)
{
    // 声明符先收集全部数组维度，再处理可选初始化表达式。
    auto declarator = makeNode(NodeKind::Declarator, name.text, name.line, name.column);
    while (match(TokenKind::LeftBracket))
    {
        const Token bracket = previous();
        auto dimension = makeNode(NodeKind::ArrayDimension, "", bracket.line, bracket.column);
        if (!check(TokenKind::RightBracket)) dimension->add(parseExpression());
        consume(TokenKind::RightBracket, "数组下标后缺少右中括号']'");
        declarator->add(std::move(dimension));
    }
    if (match(TokenKind::Assign))
    {
        const Token equals = previous();
        auto initializer = makeNode(NodeKind::Initializer, "", equals.line, equals.column);
        initializer->add(parseAssignment());
        declarator->add(std::move(initializer));
    }
    return declarator;
}

ASTPtr SourceParser::parseVariableDeclaration(const std::string& type,
                                              const Token& firstName,
                                              bool consumeSemicolon)
{
    auto declaration = makeNode(NodeKind::VariableDeclaration, type,
                                firstName.line, firstName.column);
    declaration->add(parseDeclarator(firstName));
    while (match(TokenKind::Comma))
    {
        const Token& name = consume(TokenKind::Identifier, "逗号后需要变量名");
        declaration->add(parseDeclarator(name));
    }
    if (consumeSemicolon)
        consume(TokenKind::Semicolon, "变量声明后缺少分号';'");
    return declaration;
}

ASTPtr SourceParser::parseParameterList()
{
    auto list = makeNode(NodeKind::ParameterList, "", peek().line, peek().column);
    if (check(TokenKind::RightParen)) return list;
    if (check(TokenKind::KwVoid))
    {
        std::size_t lookahead = 1;
        while (peek(lookahead).kind == TokenKind::LineComment ||
               peek(lookahead).kind == TokenKind::BlockComment) ++lookahead;
        if (peek(lookahead).kind == TokenKind::RightParen)
        {
            advance();
            skipComments();
            return list;
        }
    }
    do
    {
        const Token start = peek();
        const std::string type = parseType();
        const Token& name = consume(TokenKind::Identifier, "形参类型后需要参数名");
        auto parameter = makeNode(NodeKind::Parameter, type, start.line, start.column);
        parameter->add(parseDeclarator(name));
        list->add(std::move(parameter));
    } while (match(TokenKind::Comma));
    return list;
}

ASTPtr SourceParser::parseCompoundStatement()
{
    const Token& brace = consume(TokenKind::LeftBrace, "此处需要左花括号'{'");
    auto block = makeNode(NodeKind::CompoundStatement, "", brace.line, brace.column);
    while (peek().kind != TokenKind::RightBrace && !atEnd())
    {
        if (rawComment())
        {
            block->add(takeRawComment());
            continue;
        }
        if (match(TokenKind::Preprocessor))
        {
            block->add(makeNode(NodeKind::Preprocessor, previous().text,
                                previous().line, previous().column));
            continue;
        }
        if (isDeclarationStart())
        {
            const std::string type = parseType();
            const Token& name = consume(TokenKind::Identifier, "局部变量类型后需要变量名");
            block->add(parseVariableDeclaration(type, name, true));
            flushDeferred(*block);
        }
        else
        {
            block->add(parseStatement());
            flushDeferred(*block);
        }
    }
    consume(TokenKind::RightBrace, "复合语句缺少右花括号'}'");
    return block;
}

ASTPtr SourceParser::parseStatement()
{
    if (check(TokenKind::LeftBrace)) return parseCompoundStatement();
    if (match(TokenKind::KwIf)) return parseIfStatement();
    if (match(TokenKind::KwWhile)) return parseWhileStatement();
    if (match(TokenKind::KwFor)) return parseForStatement();
    if (match(TokenKind::KwReturn)) return parseReturnStatement();
    if (match(TokenKind::KwBreak))
    {
        const Token token = previous();
        consume(TokenKind::Semicolon, "break后缺少分号';'");
        return makeNode(NodeKind::BreakStatement, "", token.line, token.column);
    }
    if (match(TokenKind::KwContinue))
    {
        const Token token = previous();
        consume(TokenKind::Semicolon, "continue后缺少分号';'");
        return makeNode(NodeKind::ContinueStatement, "", token.line, token.column);
    }
    if (match(TokenKind::Semicolon))
        return makeNode(NodeKind::EmptyStatement, "", previous().line, previous().column);
    const Token start = peek();
    auto statement = makeNode(NodeKind::ExpressionStatement, "", start.line, start.column);
    statement->add(parseExpression());
    consume(TokenKind::Semicolon, "表达式语句后缺少分号';'");
    return statement;
}

ASTPtr SourceParser::parseIfStatement()
{
    const Token token = previous();
    consume(TokenKind::LeftParen, "if后缺少左圆括号'('");
    auto condition = parseExpression();
    consume(TokenKind::RightParen, "if条件后缺少右圆括号')'");
    auto node = makeNode(NodeKind::IfStatement, "", token.line, token.column);
    node->add(std::move(condition));
    node->add(parseControlledStatement());
    if (match(TokenKind::KwElse)) node->add(parseControlledStatement());
    return node;
}

ASTPtr SourceParser::parseWhileStatement()
{
    const Token token = previous();
    consume(TokenKind::LeftParen, "while后缺少左圆括号'('");
    auto condition = parseExpression();
    consume(TokenKind::RightParen, "while条件后缺少右圆括号')'");
    auto node = makeNode(NodeKind::WhileStatement, "", token.line, token.column);
    node->add(std::move(condition));
    node->add(parseControlledStatement());
    return node;
}

ASTPtr SourceParser::parseForStatement()
{
    const Token token = previous();
    consume(TokenKind::LeftParen, "for后缺少左圆括号'('");
    auto node = makeNode(NodeKind::ForStatement, "", token.line, token.column);

    if (match(TokenKind::Semicolon))
        node->add(makeNode(NodeKind::EmptyExpression));
    else if (isDeclarationStart())
    {
        const std::string type = parseType();
        const Token& name = consume(TokenKind::Identifier, "for初始化声明缺少变量名");
        node->add(parseVariableDeclaration(type, name, true));
    }
    else
    {
        node->add(parseExpression());
        consume(TokenKind::Semicolon, "for初始化表达式后缺少分号';'");
    }

    if (match(TokenKind::Semicolon))
        node->add(makeNode(NodeKind::EmptyExpression));
    else
    {
        node->add(parseExpression());
        consume(TokenKind::Semicolon, "for条件后缺少分号';'");
    }

    if (check(TokenKind::RightParen))
        node->add(makeNode(NodeKind::EmptyExpression));
    else
        node->add(parseExpression());
    consume(TokenKind::RightParen, "for循环表达式后缺少右圆括号')'");
    node->add(parseControlledStatement());
    return node;
}

ASTPtr SourceParser::parseControlledStatement()
{
    // 注释不能成为if/while/for的执行体。必要时创建复合语句，
    // 把注释和真正的受控语句放进同一个块，保持程序语义不变。
    skipComments();
    std::vector<ASTPtr> comments = std::move(deferredComments_);
    deferredComments_.clear();

    auto statement = parseStatement();
    for (auto& comment : deferredComments_) comments.push_back(std::move(comment));
    deferredComments_.clear();
    if (comments.empty()) return statement;

    if (statement->kind == NodeKind::CompoundStatement)
    {
        statement->children.insert(statement->children.begin(),
                                   std::make_move_iterator(comments.begin()),
                                   std::make_move_iterator(comments.end()));
        return statement;
    }

    const int line = comments.front()->line;
    const int column = comments.front()->column;
    auto block = makeNode(NodeKind::CompoundStatement, "", line, column);
    for (auto& comment : comments) block->add(std::move(comment));
    block->add(std::move(statement));
    return block;
}

ASTPtr SourceParser::parseReturnStatement()
{
    const Token token = previous();
    auto node = makeNode(NodeKind::ReturnStatement, "", token.line, token.column);
    if (!check(TokenKind::Semicolon)) node->add(parseExpression());
    consume(TokenKind::Semicolon, "return语句后缺少分号';'");
    return node;
}

ASTPtr SourceParser::parseExpression() { return parseAssignment(); }

ASTPtr SourceParser::makeBinary(const Token& op, ASTPtr left, ASTPtr right)
{
    auto node = makeNode(NodeKind::BinaryExpression, op.text, op.line, op.column);
    node->add(std::move(left));
    node->add(std::move(right));
    return node;
}

ASTPtr SourceParser::parseAssignment()
{
    // 赋值是右结合运算符，因此右操作数继续递归解析assignment。
    auto left = parseLogicalOr();
    if (match(TokenKind::Assign))
    {
        const Token op = previous();
        return makeBinary(op, std::move(left), parseAssignment());
    }
    return left;
}

ASTPtr SourceParser::parseLogicalOr()
{
    auto result = parseLogicalAnd();
    while (match(TokenKind::OrOr))
    {
        const Token op = previous();
        result = makeBinary(op, std::move(result), parseLogicalAnd());
    }
    return result;
}

ASTPtr SourceParser::parseLogicalAnd()
{
    auto result = parseEquality();
    while (match(TokenKind::AndAnd))
    {
        const Token op = previous();
        result = makeBinary(op, std::move(result), parseEquality());
    }
    return result;
}

ASTPtr SourceParser::parseEquality()
{
    auto result = parseRelational();
    while (check(TokenKind::Equal) || check(TokenKind::NotEqual))
    {
        const Token op = advance();
        result = makeBinary(op, std::move(result), parseRelational());
    }
    return result;
}

ASTPtr SourceParser::parseRelational()
{
    auto result = parseAdditive();
    while (check(TokenKind::Greater) || check(TokenKind::Less) ||
           check(TokenKind::GreaterEqual) || check(TokenKind::LessEqual))
    {
        const Token op = advance();
        result = makeBinary(op, std::move(result), parseAdditive());
    }
    return result;
}

ASTPtr SourceParser::parseAdditive()
{
    auto result = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus))
    {
        const Token op = advance();
        result = makeBinary(op, std::move(result), parseMultiplicative());
    }
    return result;
}

ASTPtr SourceParser::parseMultiplicative()
{
    auto result = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash) ||
           check(TokenKind::Percent))
    {
        const Token op = advance();
        result = makeBinary(op, std::move(result), parseUnary());
    }
    return result;
}

ASTPtr SourceParser::parseUnary()
{
    if (check(TokenKind::Plus) || check(TokenKind::Minus) || check(TokenKind::Bang))
    {
        const Token op = advance();
        auto node = makeNode(NodeKind::UnaryExpression, op.text, op.line, op.column);
        node->add(parseUnary());
        return node;
    }
    return parsePostfix();
}

ASTPtr SourceParser::parsePostfix()
{
    // 调用和数组访问优先级最高，并允许连续出现，例如f(a)[i]。
    auto result = parsePrimary();
    while (true)
    {
        if (match(TokenKind::LeftParen))
        {
            const Token token = previous();
            auto arguments = makeNode(NodeKind::ArgumentList, "", token.line, token.column);
            if (!check(TokenKind::RightParen))
            {
                do { arguments->add(parseAssignment()); }
                while (match(TokenKind::Comma));
            }
            consume(TokenKind::RightParen, "函数实参列表后缺少右圆括号')'");
            auto call = makeNode(NodeKind::CallExpression, "", token.line, token.column);
            call->add(std::move(result));
            call->add(std::move(arguments));
            result = std::move(call);
        }
        else if (match(TokenKind::LeftBracket))
        {
            const Token token = previous();
            if (check(TokenKind::RightBracket)) fail(peek(), "数组访问的下标不能为空");
            auto index = parseExpression();
            consume(TokenKind::RightBracket, "数组下标后缺少右中括号']'");
            auto subscript = makeNode(NodeKind::SubscriptExpression, "", token.line, token.column);
            subscript->add(std::move(result));
            subscript->add(std::move(index));
            result = std::move(subscript);
        }
        else break;
    }
    return result;
}

ASTPtr SourceParser::parsePrimary()
{
    if (match(TokenKind::Identifier))
        return makeNode(NodeKind::Identifier, previous().text,
                        previous().line, previous().column);
    if (match(TokenKind::IntegerLiteral))
        return makeNode(NodeKind::IntegerLiteral, previous().text,
                        previous().line, previous().column);
    if (match(TokenKind::FloatingLiteral))
        return makeNode(NodeKind::FloatingLiteral, previous().text,
                        previous().line, previous().column);
    if (match(TokenKind::CharacterLiteral))
        return makeNode(NodeKind::CharacterLiteral, previous().text,
                        previous().line, previous().column);
    if (match(TokenKind::StringLiteral))
        return makeNode(NodeKind::StringLiteral, previous().text,
                        previous().line, previous().column);
    if (match(TokenKind::LeftParen))
    {
        auto expression = parseExpression();
        consume(TokenKind::RightParen, "括号表达式缺少右圆括号')'");
        return expression;
    }
    fail(peek(), "此处需要标识符、常量或括号表达式");
}

const char* nodeKindName(NodeKind kind)
{
    switch (kind)
    {
    case NodeKind::Program: return "程序";
    case NodeKind::Preprocessor: return "预处理指令";
    case NodeKind::Comment: return "注释";
    case NodeKind::VariableDeclaration: return "变量声明";
    case NodeKind::Declarator: return "声明符";
    case NodeKind::ArrayDimension: return "数组维度";
    case NodeKind::Initializer: return "初始化器";
    case NodeKind::FunctionDeclaration: return "函数声明";
    case NodeKind::FunctionDefinition: return "函数定义";
    case NodeKind::ParameterList: return "形参列表";
    case NodeKind::Parameter: return "形参";
    case NodeKind::CompoundStatement: return "复合语句";
    case NodeKind::ExpressionStatement: return "表达式语句";
    case NodeKind::IfStatement: return "if语句";
    case NodeKind::WhileStatement: return "while语句";
    case NodeKind::ForStatement: return "for语句";
    case NodeKind::ReturnStatement: return "return语句";
    case NodeKind::BreakStatement: return "break语句";
    case NodeKind::ContinueStatement: return "continue语句";
    case NodeKind::EmptyStatement: return "空语句";
    case NodeKind::BinaryExpression: return "双目表达式";
    case NodeKind::UnaryExpression: return "一元表达式";
    case NodeKind::CallExpression: return "函数调用";
    case NodeKind::ArgumentList: return "实参列表";
    case NodeKind::SubscriptExpression: return "数组访问";
    case NodeKind::Identifier: return "标识符";
    case NodeKind::IntegerLiteral: return "整数常量";
    case NodeKind::FloatingLiteral: return "浮点常量";
    case NodeKind::CharacterLiteral: return "字符常量";
    case NodeKind::StringLiteral: return "字符串常量";
    case NodeKind::EmptyExpression: return "空表达式";
    }
    return "未知节点";
}

void printAst(const ASTNode& root) { printAstNode(root, "", true); }

std::size_t countAstNodes(const ASTNode& root)
{
    std::size_t count = 1;
    for (const auto& child : root.children) count += countAstNodes(*child);
    return count;
}
