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

// 判断字符能否作为标识符的首字符。
// 课程工具遵循C语言常见规则：首字符只能是英文字母或下划线。
bool identifierStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// 判断字符能否作为标识符的后续字符。
// 与首字符相比，后续位置额外允许数字，例如value1。
bool identifierPart(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
}

// 保存待扫描的源代码并初始化扫描状态。
// current_从0开始，line_和column_从源文件左上角的1:1开始计数。
SourceLexer::SourceLexer(std::string source) : source_(std::move(source)) {}

// 判断扫描位置是否已经到达源代码末尾。
bool SourceLexer::atEnd() const { return current_ >= source_.size(); }

// 查看后续字符而不移动扫描位置。
// 超出源代码范围时返回'\0'，使调用方不必每次都手动检查越界。
char SourceLexer::peek(std::size_t offset) const
{
    const std::size_t position = current_ + offset;
    return position < source_.size() ? source_[position] : '\0';
}

// 读取一个字符并同步更新行号、列号和行首状态。
// lineHasOnlyWhitespace_用于判断#是否位于一行开头，从而识别预处理指令。
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

// 若下一个字符匹配则读取它，否则保持当前位置不变。
// 该函数用于识别==、!=、>=、&&等由两个字符组成的运算符。
bool SourceLexer::match(char expected)
{
    if (atEnd() || peek() != expected) return false;
    advance();
    return true;
}

// 从起点到当前位置截取原文并加入Token序列。
// Token保留行列和原始拼写，错误诊断和格式化显示都依赖这些信息。
void SourceLexer::add(TokenKind kind, std::size_t start, int line, int column,
                      const std::string& message)
{
    tokens_.push_back({kind, source_.substr(start, current_ - start),
                       line, column, message});
}

// 扫描标识符，并根据关键字表确定最终类别。
// 若文本在KEYWORDS中则生成关键字Token，否则生成普通标识符Token。
void SourceLexer::scanIdentifier(std::size_t start, int line, int column)
{
    while (identifierPart(peek())) advance();
    const std::string text = source_.substr(start, current_ - start);
    const auto found = KEYWORDS.find(text);
    add(found == KEYWORDS.end() ? TokenKind::Identifier : found->second,
        start, line, column);
}

// 扫描整数或浮点常量，同时识别常见的数字书写错误。
// 支持十进制、八进制、十六进制、科学计数法和常见的u/l/f后缀。
void SourceLexer::scanNumber(std::size_t start, int line, int column)
{
    // 以0x或0X开头的十六进制常量单独处理，其他数字走十/八进制及浮点路径。
    // 即使发现错误也会读完当前数字，避免一个错误拆成多个无关Token和诊断。
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

// 扫描字符串或字符常量，并验证引号和字符长度。
// quote参数决定当前处理双引号字符串还是单引号字符常量。
void SourceLexer::scanQuoted(std::size_t start, int line, int column, char quote)
{
    // 反斜杠和其后的字符合起来算一个逻辑字符；反斜杠换行不计入。
    // 因此'\\n'是合法字符常量，而'abc'会因逻辑字符数大于1被拒绝。
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

// 扫描从双斜杠开始到行末的行注释。
// 换行符留给主扫描循环处理，以便行号能够正确递增。
void SourceLexer::scanLineComment(std::size_t start, int line, int column)
{
    while (!atEnd() && peek() != '\n') advance();
    add(TokenKind::LineComment, start, line, column);
}

// 扫描块注释，并在缺少结束符时生成错误Token。
// 块注释允许跨行，advance()会自动维护跨行后的行号和列号。
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

// 扫描完整预处理指令并保留其原始文本。
// 预处理指令不拆成普通Token，避免#include <stdio.h>中的尖括号干扰语法分析。
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

// 根据当前首字符分派到对应的Token扫描逻辑。
// 每次调用只产生一个有效Token、一个错误Token，或跳过一段空白。
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

// 扫描整个源代码，并在结尾补充文件结束Token。
// EOF让语法分析器能用统一方式检测输入结束和缺失的右括号等错误。
std::vector<Token> SourceLexer::scan()
{
    while (!atEnd()) scanToken();
    tokens_.push_back({TokenKind::EndOfFile, "", line_, column_, ""});
    return tokens_;
}

// 判断Token是否可作为课程子集中的基本类型关键字。
// const、static等修饰词由解析器的typeModifier()在更高一层处理。
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

// 将Token类别转换为供终端表格显示的中文名称。
// 关键字统一显示为“关键字”，无需为每个关键字重复建立显示文本。
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
