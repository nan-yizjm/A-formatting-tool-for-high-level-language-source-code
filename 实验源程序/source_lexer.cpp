// SourceLexer实现。扫描器一次只向前移动，不进行回溯。
#include "source_lexer.h"

#include <cctype>
#include <unordered_map>

namespace
{
// 关键字表只负责分类；关键字是否出现在合法语法位置由解析器判断。
const std::unordered_map<std::string, TokenKind> KEYWORDS = {
    {"auto", TokenKind::KwAuto}, {"break", TokenKind::KwBreak},
    {"case", TokenKind::KwCase}, {"char", TokenKind::KwChar},
    {"const", TokenKind::KwConst}, {"continue", TokenKind::KwContinue},
    {"default", TokenKind::KwDefault}, {"do", TokenKind::KwDo},
    {"double", TokenKind::KwDouble}, {"else", TokenKind::KwElse},
    {"enum", TokenKind::KwEnum}, {"extern", TokenKind::KwExtern},
    {"float", TokenKind::KwFloat}, {"for", TokenKind::KwFor},
    {"goto", TokenKind::KwGoto}, {"if", TokenKind::KwIf},
    {"int", TokenKind::KwInt}, {"long", TokenKind::KwLong},
    {"register", TokenKind::KwRegister}, {"return", TokenKind::KwReturn},
    {"short", TokenKind::KwShort}, {"signed", TokenKind::KwSigned},
    {"sizeof", TokenKind::KwSizeof}, {"static", TokenKind::KwStatic},
    {"struct", TokenKind::KwStruct}, {"switch", TokenKind::KwSwitch},
    {"typedef", TokenKind::KwTypedef}, {"union", TokenKind::KwUnion},
    {"unsigned", TokenKind::KwUnsigned}, {"void", TokenKind::KwVoid},
    {"volatile", TokenKind::KwVolatile}, {"while", TokenKind::KwWhile}
};

bool identifierStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool identifierPart(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
}

SourceLexer::SourceLexer(std::string source) : source_(std::move(source)) {}

bool SourceLexer::atEnd() const { return current_ >= source_.size(); }

char SourceLexer::peek(std::size_t offset) const
{
    const std::size_t position = current_ + offset;
    return position < source_.size() ? source_[position] : '\0';
}

char SourceLexer::advance()
{
    if (atEnd()) return '\0';
    const char c = source_[current_++];
    if (c == '\n')
    {
        ++line_;
        column_ = 1;
        lineHasOnlyWhitespace_ = true;
    }
    else
    {
        ++column_;
        if (c != ' ' && c != '\t' && c != '\r')
            lineHasOnlyWhitespace_ = false;
    }
    return c;
}

bool SourceLexer::match(char expected)
{
    if (atEnd() || peek() != expected) return false;
    advance();
    return true;
}

void SourceLexer::add(TokenKind kind, std::size_t start, int line, int column,
                      const std::string& message)
{
    tokens_.push_back({kind, source_.substr(start, current_ - start),
                       line, column, message});
}

void SourceLexer::scanIdentifier(std::size_t start, int line, int column)
{
    while (identifierPart(peek())) advance();
    const std::string text = source_.substr(start, current_ - start);
    const auto found = KEYWORDS.find(text);
    add(found == KEYWORDS.end() ? TokenKind::Identifier : found->second,
        start, line, column);
}

void SourceLexer::scanNumber(std::size_t start, int line, int column)
{
    // 数字扫描按“十六进制”与“十/八进制及浮点数”两条路径处理。
    // 即使发现错误也会读完当前数字，避免一个错误拆成多个无关Token。
    bool floating = source_[start] == '.';
    bool invalid = false;
    std::string message;

    if (source_[start] == '0' && (peek() == 'x' || peek() == 'X'))
    {
        advance();
        const std::size_t digitStart = current_;
        while (std::isxdigit(static_cast<unsigned char>(peek()))) advance();
        if (current_ == digitStart)
        {
            invalid = true;
            message = "十六进制常量缺少有效数字";
        }
        while (peek() == 'u' || peek() == 'U' ||
               peek() == 'l' || peek() == 'L') advance();
        if (identifierPart(peek()))
        {
            invalid = true;
            message = "十六进制常量包含非法字符";
            while (identifierPart(peek())) advance();
        }
        add(invalid ? TokenKind::Invalid : TokenKind::IntegerLiteral,
            start, line, column, message);
        return;
    }

    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.')
    {
        floating = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    if (peek() == 'e' || peek() == 'E')
    {
        floating = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        const std::size_t exponentStart = current_;
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (current_ == exponentStart)
        {
            invalid = true;
            message = "浮点常量的指数部分缺少数字";
        }
    }

    if (floating)
    {
        if (peek() == 'f' || peek() == 'F' ||
            peek() == 'l' || peek() == 'L') advance();
    }
    else
    {
        const std::size_t suffixStart = current_;
        while (peek() == 'u' || peek() == 'U' ||
               peek() == 'l' || peek() == 'L') advance();
        const std::string digits = source_.substr(start, suffixStart - start);
        if (digits.size() > 1 && digits[0] == '0')
        {
            for (char c : digits)
            {
                if (c == '8' || c == '9')
                {
                    invalid = true;
                    message = "八进制常量只能包含0到7";
                    break;
                }
            }
        }
    }

    if (identifierStart(peek()))
    {
        invalid = true;
        message = "数字常量后出现非法字母";
        while (identifierPart(peek())) advance();
    }
    add(invalid ? TokenKind::Invalid
                : (floating ? TokenKind::FloatingLiteral
                            : TokenKind::IntegerLiteral),
        start, line, column, message);
}

void SourceLexer::scanQuoted(std::size_t start, int line, int column, char quote)
{
    // 反斜杠和其后的字符合起来算一个逻辑字符；反斜杠换行不计入。
    bool closed = false;
    int logicalCharacters = 0;
    while (!atEnd())
    {
        const char c = advance();
        if (c == '\n') break;
        if (c == '\\')
        {
            if (!atEnd() && peek() == '\n')
            {
                advance();
                continue;
            }
            if (!atEnd() && peek() == '\r' && peek(1) == '\n')
            {
                advance();
                advance();
                continue;
            }
            if (!atEnd())
            {
                advance();
                ++logicalCharacters;
            }
            continue;
        }
        if (c == quote)
        {
            closed = true;
            break;
        }
        ++logicalCharacters;
    }

    if (!closed)
    {
        add(TokenKind::Invalid, start, line, column,
            quote == '"' ? "字符串常量缺少结束引号"
                         : "字符常量缺少结束引号");
    }
    else if (quote == '\'' && logicalCharacters != 1)
    {
        add(TokenKind::Invalid, start, line, column,
            "字符常量必须包含一个字符或一个转义序列");
    }
    else
    {
        add(quote == '"' ? TokenKind::StringLiteral
                         : TokenKind::CharacterLiteral,
            start, line, column);
    }
}

void SourceLexer::scanLineComment(std::size_t start, int line, int column)
{
    while (!atEnd() && peek() != '\n') advance();
    add(TokenKind::LineComment, start, line, column);
}

void SourceLexer::scanBlockComment(std::size_t start, int line, int column)
{
    bool closed = false;
    while (!atEnd())
    {
        if (peek() == '*' && peek(1) == '/')
        {
            advance(); advance();
            closed = true;
            break;
        }
        advance();
    }
    add(closed ? TokenKind::BlockComment : TokenKind::Invalid,
        start, line, column, closed ? "" : "块注释缺少结束符*/");
}

void SourceLexer::scanPreprocessor(std::size_t start, int line, int column)
{
    // 预处理内容不参与C子集语法分析，按整行原文保存；支持反斜杠续行。
    bool continued;
    do
    {
        continued = false;
        while (!atEnd() && peek() != '\n') advance();
        std::size_t check = current_;
        while (check > start && (source_[check - 1] == ' ' ||
               source_[check - 1] == '\t' || source_[check - 1] == '\r')) --check;
        if (check > start && source_[check - 1] == '\\' && !atEnd())
        {
            advance();
            continued = true;
        }
    } while (continued);
    add(TokenKind::Preprocessor, start, line, column);
}

void SourceLexer::scanToken()
{
    // 保存Token起点，再由各专用扫描函数推进current_到Token末尾。
    const std::size_t start = current_;
    const int startLine = line_;
    const int startColumn = column_;
    const bool directiveAllowed = lineHasOnlyWhitespace_;
    const char c = advance();

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return;
    if (c == '#' && directiveAllowed)
    {
        scanPreprocessor(start, startLine, startColumn);
        return;
    }
    if (identifierStart(c))
    {
        scanIdentifier(start, startLine, startColumn);
        return;
    }
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && std::isdigit(static_cast<unsigned char>(peek()))))
    {
        scanNumber(start, startLine, startColumn);
        return;
    }

    switch (c)
    {
    case '"': scanQuoted(start, startLine, startColumn, '"'); return;
    case '\'': scanQuoted(start, startLine, startColumn, '\''); return;
    case '+': add(TokenKind::Plus, start, startLine, startColumn); return;
    case '-': add(TokenKind::Minus, start, startLine, startColumn); return;
    case '*': add(TokenKind::Star, start, startLine, startColumn); return;
    case '%': add(TokenKind::Percent, start, startLine, startColumn); return;
    case '{': add(TokenKind::LeftBrace, start, startLine, startColumn); return;
    case '}': add(TokenKind::RightBrace, start, startLine, startColumn); return;
    case '[': add(TokenKind::LeftBracket, start, startLine, startColumn); return;
    case ']': add(TokenKind::RightBracket, start, startLine, startColumn); return;
    case '(': add(TokenKind::LeftParen, start, startLine, startColumn); return;
    case ')': add(TokenKind::RightParen, start, startLine, startColumn); return;
    case ';': add(TokenKind::Semicolon, start, startLine, startColumn); return;
    case ',': add(TokenKind::Comma, start, startLine, startColumn); return;
    case '=': add(match('=') ? TokenKind::Equal : TokenKind::Assign,
                         start, startLine, startColumn); return;
    case '!': add(match('=') ? TokenKind::NotEqual : TokenKind::Bang,
                         start, startLine, startColumn); return;
    case '>': add(match('=') ? TokenKind::GreaterEqual : TokenKind::Greater,
                         start, startLine, startColumn); return;
    case '<': add(match('=') ? TokenKind::LessEqual : TokenKind::Less,
                         start, startLine, startColumn); return;
    case '&':
        if (match('&')) add(TokenKind::AndAnd, start, startLine, startColumn);
        else add(TokenKind::Invalid, start, startLine, startColumn,
                 "本工具仅支持逻辑与&&，不支持按位与&");
        return;
    case '|':
        if (match('|')) add(TokenKind::OrOr, start, startLine, startColumn);
        else add(TokenKind::Invalid, start, startLine, startColumn,
                 "本工具仅支持逻辑或||，不支持按位或|");
        return;
    case '/':
        if (match('/')) scanLineComment(start, startLine, startColumn);
        else if (match('*')) scanBlockComment(start, startLine, startColumn);
        else add(TokenKind::Slash, start, startLine, startColumn);
        return;
    default:
        add(TokenKind::Invalid, start, startLine, startColumn, "无法识别的字符");
    }
}

std::vector<Token> SourceLexer::scan()
{
    while (!atEnd()) scanToken();
    tokens_.push_back({TokenKind::EndOfFile, "", line_, column_, ""});
    return tokens_;
}

bool isTypeKeyword(TokenKind kind)
{
    switch (kind)
    {
    case TokenKind::KwChar: case TokenKind::KwDouble: case TokenKind::KwFloat:
    case TokenKind::KwInt: case TokenKind::KwLong: case TokenKind::KwShort:
    case TokenKind::KwSigned: case TokenKind::KwUnsigned: case TokenKind::KwVoid:
        return true;
    default: return false;
    }
}

const char* tokenKindName(TokenKind kind)
{
    switch (kind)
    {
    case TokenKind::EndOfFile: return "文件结束";
    case TokenKind::Invalid: return "错误Token";
    case TokenKind::Identifier: return "标识符";
    case TokenKind::IntegerLiteral: return "整数常量";
    case TokenKind::FloatingLiteral: return "浮点常量";
    case TokenKind::CharacterLiteral: return "字符常量";
    case TokenKind::StringLiteral: return "字符串常量";
    case TokenKind::Preprocessor: return "预处理指令";
    case TokenKind::LineComment: return "行注释";
    case TokenKind::BlockComment: return "块注释";
    case TokenKind::Plus: return "加法运算符";
    case TokenKind::Minus: return "减法运算符";
    case TokenKind::Star: return "乘法运算符";
    case TokenKind::Slash: return "除法运算符";
    case TokenKind::Percent: return "取模运算符";
    case TokenKind::Assign: return "赋值运算符";
    case TokenKind::Equal: case TokenKind::NotEqual: case TokenKind::Greater:
    case TokenKind::Less: case TokenKind::GreaterEqual: case TokenKind::LessEqual:
        return "关系运算符";
    case TokenKind::AndAnd: return "逻辑与";
    case TokenKind::OrOr: return "逻辑或";
    case TokenKind::Bang: return "逻辑非";
    case TokenKind::LeftBrace: return "左花括号";
    case TokenKind::RightBrace: return "右花括号";
    case TokenKind::LeftBracket: return "左中括号";
    case TokenKind::RightBracket: return "右中括号";
    case TokenKind::LeftParen: return "左圆括号";
    case TokenKind::RightParen: return "右圆括号";
    case TokenKind::Semicolon: return "分号";
    case TokenKind::Comma: return "逗号";
    default: return "关键字";
    }
}
