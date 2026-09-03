// 自动回归测试：覆盖解析、AST节点、格式化回读和错误拒绝。
#include "source_formatter.h"
#include "source_lexer.h"
#include "source_parser.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct ParseResult
{
    ASTPtr ast;
    std::string error;
};

static ParseResult parseSource(const std::string& source)
{
    SourceLexer lexer(source);
    std::vector<Token> tokens = lexer.scan();
    for (const Token& token : tokens)
    {
        if (token.kind == TokenKind::Invalid)
            return {nullptr, token.message};
    }
    SourceParser parser(tokens);
    ASTPtr ast = parser.parse();
    if (!ast)
    {
        const auto& diagnostics = parser.diagnostics();
        return {nullptr, diagnostics.empty() ? "未知语法错误"
                                             : diagnostics.front().message};
    }
    return {std::move(ast), ""};
}

static bool containsKind(const ASTNode& node, NodeKind expected)
{
    if (node.kind == expected) return true;
    for (const auto& child : node.children)
        if (containsKind(*child, expected)) return true;
    return false;
}

int main()
{
    // 每个正确用例都必须通过“解析 -> 格式化 -> 再解析”闭环。
    const std::vector<std::string> validCases = {
        "int a, b = 2; double values[10]; int main(void) { int i; return 0; }",
        "int main(void) { int a; a = 1 + 2 * 3 - 4 / 2 % 2; "
        "if (!(a == 1) || a != 2 && a >= 0 && a <= 9) a = -a; return a; }",
        "int sum(int a, int b); int main(void) { int data[2]; data[0] = 1; "
        "sum(data[0], 2); return 0; } int sum(int a, int b) { return a + b; }",
        "int main(void) { int i; for (;;) { break; } for (i = 0; i < 3; i = i + 1) "
        "continue; while (i > 0) i = i - 1; return; }",
        "#define FLAG 1\nint main(void) { int /* 类型与变量之间 */ a; "
        "a /* 表达式中的注释 */ = 1; if (a) // 分支体前注释\n"
        "a = 2; else /* else后的注释 */ a = 3; puts(\"//不是注释\"); return 0; }",
        "int main(void){int a;a=1+2*3;if(a>0){a=a-1;}return a;}"
    };

    int failures = 0;
    for (std::size_t i = 0; i < validCases.size(); ++i)
    {
        ParseResult first = parseSource(validCases[i]);
        if (!first.ast)
        {
            std::cerr << "[FAIL] 正确用例 " << i + 1 << ": " << first.error << '\n';
            ++failures;
            continue;
        }
        const std::string formatted = formatSource(*first.ast);
        ParseResult second = parseSource(formatted);
        if (!second.ast)
        {
            std::cerr << "[FAIL] 格式化回读 " << i + 1 << ": " << second.error << '\n';
            ++failures;
        }
        else
        {
            std::cout << "[PASS] 正确用例与格式化回读 " << i + 1 << '\n';
        }
    }

    // 错误用例覆盖词法错误、分号、括号、表达式和字符常量。
    const std::vector<std::string> invalidCases = {
        "int 1name;",
        "int main(void) { int a a = 1; }",
        "int main(void) { if (1 { return 0; } }",
        "int main(void) { int a; a = 1 + ; }",
        "int main(void) { char c; c = 'x; }"
    };
    for (std::size_t i = 0; i < invalidCases.size(); ++i)
    {
        ParseResult result = parseSource(invalidCases[i]);
        if (result.ast)
        {
            std::cerr << "[FAIL] 错误用例 " << i + 1 << " 未被拒绝\n";
            ++failures;
        }
        else
        {
            std::cout << "[PASS] 错误用例 " << i + 1 << " 已拒绝\n";
        }
    }

    // 综合用例还要检查课程要求中的关键AST节点是否真正存在。
    const std::string integration =
        "#include <stdio.h>\n#define N 10\n// 输出一个整数\n"
        "int printValue(int value);\nint main(void) { int values[N]; int i; "
        "for (i = 0; i < N; i = i + 1) values[i] = i * 2; "
        "if (N > 0 && values[0] < values[1]) printValue(values[1]); return 0; } "
        "int printValue(int value) { printf(\"%d\", value); return value; }";
    ParseResult result = parseSource(integration);
    const std::vector<NodeKind> requiredKinds = {
        NodeKind::Preprocessor, NodeKind::Comment, NodeKind::FunctionDeclaration,
        NodeKind::FunctionDefinition, NodeKind::ForStatement, NodeKind::IfStatement,
        NodeKind::CallExpression, NodeKind::SubscriptExpression
    };
    if (!result.ast)
    {
        std::cerr << "[FAIL] 综合用例: " << result.error << '\n';
        ++failures;
    }
    else
    {
        for (NodeKind kind : requiredKinds)
        {
            if (!containsKind(*result.ast, kind))
            {
                std::cerr << "[FAIL] 综合AST缺少节点: " << nodeKindName(kind) << '\n';
                ++failures;
            }
        }
        std::ofstream output("formatted_smoke.c", std::ios::binary);
        output << formatSource(*result.ast);
        if (!output) { std::cerr << "[FAIL] 无法写入编译冒烟测试文件\n"; ++failures; }
    }

    if (failures != 0)
    {
        std::cerr << "共 " << failures << " 项失败\n";
        return 1;
    }
    std::cout << "全部自动回归测试通过\n";
    return 0;
}
