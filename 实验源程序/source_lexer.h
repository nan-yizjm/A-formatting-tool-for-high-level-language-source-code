// 词法分析器接口：把UTF-8 C源代码扫描为带位置的Token序列。
#pragma once

#include <string>
#include <vector>

enum class TokenKind
{
    // 文件控制、字面量和需要原文保存的Token。
    EndOfFile, Invalid,
    Identifier, IntegerLiteral, FloatingLiteral, CharacterLiteral,
    StringLiteral, Preprocessor, LineComment, BlockComment,
    // C语言关键字。语法分析器只接受课程要求的子集。
    KwAuto, KwBreak, KwCase, KwChar, KwConst, KwContinue, KwDefault,
    KwDo, KwDouble, KwElse, KwEnum, KwExtern, KwFloat, KwFor, KwGoto,
    KwIf, KwInt, KwLong, KwRegister, KwReturn, KwShort, KwSigned,
    KwSizeof, KwStatic, KwStruct, KwSwitch, KwTypedef, KwUnion,
    KwUnsigned, KwVoid, KwVolatile, KwWhile,
    // 课程子集使用的运算符和界符。
    Plus, Minus, Star, Slash, Percent, Assign, Equal, NotEqual,
    Greater, Less, GreaterEqual, LessEqual, AndAnd, OrOr, Bang,
    LeftBrace, RightBrace, LeftBracket, RightBracket,
    LeftParen, RightParen, Semicolon, Comma
};

struct Token
{
    TokenKind kind;
    std::string text;
    int line;
    int column;
    std::string message; // 仅Invalid Token携带具体错误原因。
};

class SourceLexer
{
public:
    explicit SourceLexer(std::string source);
    std::vector<Token> scan();

private:
    std::string source_;
    std::size_t current_ = 0;
    int line_ = 1;
    int column_ = 1;
    bool lineHasOnlyWhitespace_ = true; // 用于判断#是否位于预处理行开头。
    std::vector<Token> tokens_;

    bool atEnd() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void add(TokenKind kind, std::size_t start, int line, int column,
             const std::string& message = {});
    void scanToken();
    void scanIdentifier(std::size_t start, int line, int column);
    void scanNumber(std::size_t start, int line, int column);
    void scanQuoted(std::size_t start, int line, int column, char quote);
    void scanLineComment(std::size_t start, int line, int column);
    void scanBlockComment(std::size_t start, int line, int column);
    void scanPreprocessor(std::size_t start, int line, int column);
};

const char* tokenKindName(TokenKind kind);
bool isTypeKeyword(TokenKind kind);
