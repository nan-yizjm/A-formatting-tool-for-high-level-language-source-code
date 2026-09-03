#include"printfile.h"
#include<vector>
#include<string>
#include<cctype>

// 单个 token：种类 + 文本
struct Tok {
	int kind;
	std::string text;
};

// 顶层单元：0 变量声明，1 函数原型，2 函数定义
struct Unit {
	std::vector<Tok> toks;
	int kind;
};

// 读取 # 指令剩余部分（从 # 之后到行尾），换行符不保留
static std::string read_directive(FILE* fp)
{
	std::string s;
	int c;
	while ((c = fgetc(fp)) != EOF && c != '\n') {
		if (c != '\r') s += (char)c;
	}
	return s;
}

// 把指令整理成 "#include <stdio.h>" 这类带空格的形式
static std::string format_directive(const std::string& raw)
{
	std::string s = raw;
	size_t pos = 0;
	while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
	std::string rest = s.substr(pos);
	if (rest.compare(0, 7, "include") == 0 &&
		(rest.size() == 7 || (!isalnum((unsigned char)rest[7]) && rest[7] != '_'))) {
		size_t p = 7;
		while (p < rest.size() && (rest[p] == ' ' || rest[p] == '\t')) p++;
		return "#include " + rest.substr(p);
	}
	if (rest.compare(0, 6, "define") == 0 &&
		(rest.size() == 6 || (!isalnum((unsigned char)rest[6]) && rest[6] != '_'))) {
		size_t p = 6;
		while (p < rest.size() && (rest[p] == ' ' || rest[p] == '\t')) p++;
		return "#define " + rest.substr(p);
	}
	return "#" + s;
}

// 输出缩进（每层 4 个空格）
static void write_indent(FILE* out, int indent)
{
	for (int i = 0; i < indent; i++) fputs("    ", out);
}

// 当前 token 前是否需要空格
static int needs_space_before(int cur, int prev)
{
	if (cur == RS || cur == RM || cur == COMMA || cur == SEMI) return 0;  // ) ] , ; 紧贴前面
	if (cur == LM) return 0;                     // 数组下标 [ 紧贴变量名
	if (prev == LS || prev == LM) return 0;      // ( [ 后面不留空格
	if (prev == RL) return 0;                    // } else 的空格已由大括号逻辑处理
	if (cur == LS) {
		if (prev == IDENT || prev == RS || prev == RM) return 0;  // 函数调用 func(
		return 1;                                  // if( while( for( 前面留空格
	}
	if (prev == POUND) return 0;
	return 1;
}

// 按缩进格式输出一个顶层单元
static void emit_unit(FILE* out, const std::vector<Tok>& toks)
{
	int indent = 0;
	int paren_depth = 0;
	int at_line_start = 1;
	int prev = 0;
	for (size_t i = 0; i < toks.size(); i++) {
		int k = toks[i].kind;
		const char* text = toks[i].text.c_str();

		if (k == RL) {   // 右大括号单独处理
			if (!at_line_start) fputc('\n', out);
			if (indent > 0) indent--;
			write_indent(out, indent);
			fputs("}", out);
			if (i + 1 < toks.size() && toks[i + 1].kind == ELSE) {
				fputc(' ', out);   // } else 保持同行
				at_line_start = 0;
			}
			else {
				fputc('\n', out);
				at_line_start = 1;
			}
			prev = k;
			continue;
		}

		if (at_line_start) {
			write_indent(out, indent);
		}
		else if (needs_space_before(k, prev)) {
			fputc(' ', out);
		}
		fputs(text, out);
		at_line_start = 0;

		if (k == LL) {   // { 后换行并增加缩进
			fputc('\n', out);
			indent++;
			at_line_start = 1;
		}
		else if (k == SEMI && paren_depth == 0) {  // for 头部的分号不换行
			fputc('\n', out);
			at_line_start = 1;
		}

		if (k == LS) paren_depth++;
		if (k == RS) paren_depth--;
		prev = k;
	}
}

status PrintFile(FILE* fp) {
	std::vector<Unit> vars, protos, defs;
	std::vector<std::string> directives;
	std::vector<Tok> current;
	int brace_depth = 0;
	int w;
	extern char token_text[100];
	extern int line_num;
	line_num = 1;
	// 第一遍：把顶层单元按 变量/原型/定义 分类收集
	while ((w = gettoken(fp)) != EOF) {
		if (w == LINENOTE || w == BLOCKNOTE) continue;   // 注释直接丢弃
		if (w == POUND) {
			directives.push_back(format_directive(read_directive(fp)));
			continue;
		}
		current.push_back({ w, token_text });
		if (w == LL) {
			brace_depth++;
		}
		else if (w == RL) {
			brace_depth--;
			if (brace_depth == 0) {
				defs.push_back({ current, 2 });
				current.clear();
			}
		}
		else if (w == SEMI && brace_depth == 0 && !current.empty()) {
			int has_paren = 0;
			for (const auto& t : current) {
				if (t.kind == LS) { has_paren = 1; break; }
			}
			if (has_paren) protos.push_back({ current, 1 });
			else vars.push_back({ current, 0 });
			current.clear();
		}
	}

	FILE* out = fopen("../output/C_print_file.txt", "w");
	if (!out) return ERROR;

	// 第二遍：指令、变量、原型、定义依次输出，组与组之间空一行
	int need_blank = 0;
	if (!directives.empty()) {
		for (const auto& d : directives) fprintf(out, "%s\n", d.c_str());
		need_blank = 1;
	}
	if (!vars.empty()) {
		if (need_blank) fputc('\n', out);
		for (const auto& u : vars) emit_unit(out, u.toks);
		need_blank = 1;
	}
	if (!protos.empty()) {
		if (need_blank) fputc('\n', out);
		for (const auto& u : protos) emit_unit(out, u.toks);
		need_blank = 1;
	}
	for (size_t i = 0; i < defs.size(); i++) {
		if (need_blank) fputc('\n', out);
		emit_unit(out, defs[i].toks);
		need_blank = 1;
	}
	fclose(out);
	return OK;
}