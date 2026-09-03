#include"parser.h"
#include<string>
#include<vector>

int w;  //获得gettoken函数的返回值即读入的单词种类编码
char kind[100]; //储存类型关键字
char tokenText0[100]; //储存第一个函数名或者变量名
char parser_error[100]; //储存语法错误说明
int indent0 = 0;  //初始化缩进值
queue<print> printList; //用于方便打印缩进
static int last_expression_end = -1;

enum SemanticType { SEM_UNKNOWN, SEM_CHAR, SEM_INT, SEM_FLOAT, SEM_DOUBLE, SEM_VOID };
struct ExpressionSemantic {
	SemanticType type;
	int modifiable;
	int is_array;
	int is_const;
};
struct VariableSemanticInfo {
	std::string name;
	SemanticType type;
	int is_array;
	int is_const;
};
struct FunctionSemanticInfo {
	std::string name;
	SemanticType return_type;
	std::vector<SemanticType> parameter_types;
	int is_definition;
};
static std::vector<VariableSemanticInfo> semantic_variables;
static std::vector<FunctionSemanticInfo> semantic_functions;
static std::vector<SemanticType> current_parameter_types;
static SemanticType last_expression_type = SEM_UNKNOWN;
static SemanticType current_function_return_type = SEM_UNKNOWN;
static int current_type_is_const = 0;

static SemanticType semantic_type_from_spelling(const char* spelling)
{
	std::string type = spelling;
	if (type.find("void") != std::string::npos) return SEM_VOID;
	if (type.find("char") != std::string::npos) return SEM_CHAR;
	if (type.find("float") != std::string::npos) return SEM_FLOAT;
	if (type.find("double") != std::string::npos) return SEM_DOUBLE;
	return SEM_INT;
}

static SemanticType find_variable_type(const char* name)
{
	for (auto it = semantic_variables.rbegin(); it != semantic_variables.rend(); ++it)
		if (it->name == name) return it->type;
	return SEM_UNKNOWN;
}

static const VariableSemanticInfo* find_variable_info(const char* name)
{
	for (auto it = semantic_variables.rbegin(); it != semantic_variables.rend(); ++it)
		if (it->name == name) return &*it;
	return NULL;
}

static const FunctionSemanticInfo* find_function_info(const char* name)
{
	for (auto it = semantic_functions.rbegin(); it != semantic_functions.rend(); ++it)
		if (it->name == name) return &*it;
	return NULL;
}

static SemanticType common_arithmetic_type(SemanticType left, SemanticType right)
{
	if (left == SEM_UNKNOWN || right == SEM_UNKNOWN) return SEM_UNKNOWN;
	if (left == SEM_DOUBLE || right == SEM_DOUBLE) return SEM_DOUBLE;
	if (left == SEM_FLOAT || right == SEM_FLOAT) return SEM_FLOAT;
	if (left == SEM_INT || right == SEM_INT) return SEM_INT;
	return SEM_CHAR;
}

static int is_assignment_operator(const char* op)
{
	return !strcmp(op, "=") || !strcmp(op, "+=") || !strcmp(op, "-=") ||
		!strcmp(op, "*=") || !strcmp(op, "/=") || !strcmp(op, "%=");
}

static int is_type_token(int token)
{
	return token == CONST || token == VOID || token == CHAR || token == SHORT || token == INT ||
		token == LONG || token == SIGNED || token == UNSIGNED ||
		token == FLOAT || token == DOUBLE;
}

static status parse_type_specifier(FILE* fp, int allow_void)
{
	int void_count = 0, char_count = 0, short_count = 0, int_count = 0;
	int long_count = 0, signed_count = 0, unsigned_count = 0;
	int float_count = 0, double_count = 0, const_count = 0, total = 0;
	std::string spelling;
	while (is_type_token(w)) {
		if (!spelling.empty()) spelling += " ";
		spelling += token_text;
		total++;
		switch (w) {
		case CONST: const_count++; break;
		case VOID: void_count++; break;
		case CHAR: char_count++; break;
		case SHORT: short_count++; break;
		case INT: int_count++; break;
		case LONG: long_count++; break;
		case SIGNED: signed_count++; break;
		case UNSIGNED: unsigned_count++; break;
		case FLOAT: float_count++; break;
		case DOUBLE: double_count++; break;
		}
		w = gettoken(fp);
	}

	int type_total = total - const_count;
	int valid = type_total > 0 && const_count <= 1 && signed_count <= 1 && unsigned_count <= 1 &&
		!(signed_count && unsigned_count);
	if (void_count) valid = valid && allow_void && type_total == 1;
	else if (float_count) valid = valid && float_count == 1 && type_total == 1;
	else if (double_count)
		valid = valid && double_count == 1 && long_count <= 1 &&
			type_total == double_count + long_count;
	else if (char_count)
		valid = valid && char_count == 1 && type_total == char_count + signed_count + unsigned_count;
	else
		valid = valid && int_count <= 1 && short_count <= 1 && long_count <= 1 &&
			!(short_count && long_count) &&
			type_total == int_count + short_count + long_count + signed_count + unsigned_count;

	if (!valid) {
		strcpy(parser_error, "非法的类型说明符组合");
		return ERROR;
	}
	strcpy(kind, spelling.c_str());
	current_type_is_const = const_count != 0;
	return OK;
}

status program(FILE* fp, CTree& T)  //语法单位<程序>的子程序
{
	CTree c;
	parser_error[0] = '\0';
	semantic_variables.clear();
	semantic_functions.clear();
	current_function_return_type = SEM_UNKNOWN;
	indent0 = 0;                       //重置缩进值，防止多次运行之间状态泄漏
	while (!printList.empty()) printList.pop();  //清空打印队列
	struct print elem = { indent0,line_num };
	printList.push(elem);//存入程序的缩进值
	w = gettoken(fp);
	if (!ExtDefList(fp, c)) return ERROR;  //调用外部定义序列函数
	T.n = 1; T.r = 0;
	T.nodes[0].data = (char*)malloc((strlen("程序") + 1) * sizeof(char)); //定义语法树的根结点nodes[0]
	strcpy(T.nodes[0].data, "程序");
	T.nodes[0].indent = 0;
	T.nodes[0].firstchild = NULL;
	InsertChild(T, T.r, 1, c); //将c子树插入到语法树中
	return OK;
}

status ExtDefList(FILE* fp, CTree& T) //语法单位<外部定义序列>的子程序
{
	CTree c; 
	status flag;//查看是否建立第二棵子树
	if (w == EOF) return INFEASIBLE;
	T.n = 1;  T.r = 0;
	T.nodes[0].data = (char*)malloc((strlen("外部定义序列") + 1) * sizeof(char)); //创建外部定义序列树
	strcpy(T.nodes[0].data, "外部定义序列");
	T.nodes[0].indent = 0;
	T.nodes[0].firstchild = NULL;
	if (!ExtDef(fp, c)) return ERROR;
	InsertChild(T, T.r, 1, c); //处理一个外部定义，得到一棵子树，作为根的第一棵子树
	flag = ExtDefList(fp, c);
	if (flag == OK) InsertChild(T, T.r, 2, c); //得到的子树，作为根的第二棵子树
	if (flag==ERROR) return ERROR;
	return OK;
}

status ExtDef(FILE* fp, CTree& T)  //语法单位<外部定义>的子程序
{
	status flag;
	if (!parse_type_specifier(fp, 1)) return ERROR;
	if (w != IDENT) return ERROR;
	strcpy(tokenText0, token_text);		//保存第一个变量名或函数名到tokenText0
	w = gettoken(fp);
	if (w != LS) flag = ExtVarDef(fp, T);	//调用外部变量定义子程序
	else flag = funcDef(fp, T);			//调用函数定义子程序
	if (!flag) return ERROR;
	return OK;
}

status ExtVarDef(FILE* fp, CTree& T)  //语法单位<外部变量定义>子程序
{
	CTree c; CTree p;
	T.n = 1; T.r = 0;
	T.nodes[0].data = (char*)malloc((strlen("外部变量定义：") + 1) * sizeof(char)); //生成外部变量定义结点
	strcpy(T.nodes[0].data, "外部变量定义：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;
	c.n = 1; c.r = 0;   //生成外部变量类型结点
	c.nodes[0].data = (char*)malloc((strlen(kind) + strlen("类型：") + 1) * sizeof(char));
	strcpy(c.nodes[0].data, "类型：");
	strcat(c.nodes[0].data, kind);
	c.nodes[0].indent = 1;
	c.nodes[0].firstchild = NULL;
	if (!InsertChild(T, T.r, 1, c)) return ERROR;		//c作为T的第一个孩子
	if (!VarList(fp, p)) return ERROR;
	if (!InsertChild(T, T.r, 2, p)) return ERROR;		//p作为T的第二个孩子
	return OK;
}

status VarList(FILE* fp, CTree& T)  //语法单位<变量序列>子程序
{
	CTree c; CTree t; //c树用来构建此变量结点，t树用来构建可能存在的下一变量结点
	std::string declared_name = tokenText0;
	SemanticType declared_type = semantic_type_from_spelling(kind);
	int declared_as_array = 0;
	int declared_as_const = current_type_is_const;
	T.n = 1; T.r = 0;//生成变量序列结点
	T.nodes[0].data = (char*)malloc((strlen("变量序列") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "变量序列");
	T.nodes[0].indent = 0;
	T.nodes[0].firstchild = NULL;
	if (w == LM)     //区分是数组变量还是变量
	{
		declared_as_array = 1;
		do {
			strcat(tokenText0, "[");
			w = gettoken(fp);
			if (w != INT_CONST) return ERROR;
			strcat(tokenText0, token_text);
			w = gettoken(fp);
			if (w != RM) return ERROR;
			strcat(tokenText0, "]");
			w = gettoken(fp);
		} while (w == LM);
		c.n = 1; c.r = 0;   //创建多维数组结点
		c.nodes[0].data = (char*)malloc((strlen(tokenText0) + strlen("Array: ") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "Array: ");
		strcat(c.nodes[0].data, tokenText0);
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
	}
	else
	{
		c.n = 1; c.r = 0;  //创建变量结点
		c.nodes[0].data = (char*)malloc((strlen(tokenText0) + strlen("ID: ") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "ID: ");
		strcat(c.nodes[0].data, tokenText0);
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
	}
	if (w == EQUAL_TO) {
		CTree initializer;
		w = gettoken(fp);
		if (w == COMMA || w == SEMI) {
			strcpy(parser_error, "声明初始化缺少表达式");
			return ERROR;
		}
		if (!exp(fp, initializer, SEMI, COMMA)) return ERROR;
		if (!InsertChild(c, c.r, 1, initializer)) return ERROR;
		w = last_expression_end;
	}
	if (!InsertChild(T, T.r, 1, c))	return ERROR;//识别的变量结点作为T的第一个孩子
	semantic_variables.push_back({ declared_name, declared_type, declared_as_array, declared_as_const });
	if (w != COMMA && w != SEMI) {
		strcpy(parser_error, "变量声明缺少分号或逗号");
		return ERROR;
	}
	if (w == SEMI)								//如果标识符后是分号，直接结束
	{
		w = gettoken(fp);
		return OK;
	}
	w = gettoken(fp);
	if (w != IDENT) return ERROR;	//如果w不是标识符则报错，反之后面还有第二个变量
	strcpy(tokenText0, token_text);
	w = gettoken(fp);
	if (!VarList(fp, t)) return ERROR;
	if (!InsertChild(T, T.r, 2, t)) return ERROR;
	return OK;
}

status funcDef(FILE* fp, CTree& T)  //语法单位<函数定义>子程序
{
	CTree c; CTree p; //c结点为函数返回值类型，p结点为函数名结点
	CTree q; CTree f; CTree s; //q结点为参数序列结点,f为函数体结点，s为可能的复合语句结点
	SemanticType function_return_type = semantic_type_from_spelling(kind);
	std::string function_name = tokenText0;
	current_parameter_types.clear();
	T.n = 1; T.r = 0;		//生成函数定义结点T
	T.nodes[0].data = (char*)malloc((strlen("函数定义：") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "函数定义：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;
	c.n = 1; c.r = 0;  //生成函数返回值类型结点c
	c.nodes[0].data = (char*)malloc((strlen(kind) + strlen("类型：") + 1) * sizeof(char));
	strcpy(c.nodes[0].data, "类型：");
	strcat(c.nodes[0].data, kind);
	c.nodes[0].indent = 1;
	c.nodes[0].firstchild = NULL;
	if (!InsertChild(T, T.r, 1, c)) return ERROR;
	w = gettoken(fp);
	//函数括号内可能无参数，可能是void，可能是参数序列，其他情况报错
	if (w != RS && w != VOID && w != INT && w != LONG && w != SHORT && w != SIGNED && w != UNSIGNED && w != FLOAT && w != DOUBLE && w != CHAR)
		return ERROR;
	p.n = 1; p.r = 0;		//生成函数名结点
	p.nodes[0].data = (char*)malloc((strlen(tokenText0) + strlen("函数名：") + 1) * sizeof(char));
	strcpy(p.nodes[0].data, "函数名：");
	strcat(p.nodes[0].data, tokenText0);
	p.nodes[0].indent = 1;
	p.nodes[0].firstchild = NULL;
	if (w == RS || w == VOID) //判断void情况或参数序列情况
	{
		if (w == VOID)
		{
			w = gettoken(fp);
			if (w != RS) return ERROR;
		}
	}
	else
	{
		if (!ParameList(fp, q)) return ERROR;
		if (!InsertChild(p, p.r, 1, q)) return ERROR;
	}
	if (!InsertChild(T, T.r, 2, p)) return ERROR;
	w = gettoken(fp);
	if (w != SEMI && w != LL) return ERROR;
	semantic_functions.push_back({ function_name, function_return_type,
		current_parameter_types, w == LL });
	if (w == SEMI) {
		strcpy(T.nodes[0].data, "函数声明：");
		w = gettoken(fp);
		return OK;
	}
	f.n = 1; f.r = 0;  //生成函数体结点
	f.nodes[0].data = (char*)malloc((strlen("函数体：") + 1) * sizeof(char));
	strcpy(f.nodes[0].data, "函数体：");
	f.nodes[0].indent = 1;
	f.nodes[0].firstchild = NULL;
	SemanticType previous_return_type = current_function_return_type;
	current_function_return_type = function_return_type;
	if (!CompStat(fp, s)) return ERROR;
	current_function_return_type = previous_return_type;
	if (!InsertChild(f, f.r, 1, s)) return ERROR;
	if(!InsertChild(T, T.r, 3, f))return ERROR;
	return OK;
}

status ParameList(FILE* fp, CTree& T)  //语法单位<形参序列>子程序
{
	CTree c; //c用于生成形参子树
	CTree p; //p用于生成可能出现的下一个形参序列
	T.n = 1; T.r = 0;  //生成形参序列结点
	T.nodes[0].data = (char*)malloc((strlen("形参序列") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "形参序列");
	T.nodes[0].indent = 0;
	T.nodes[0].firstchild = NULL;
	if (!FormParDef(fp, c)) return ERROR;
	if (!InsertChild(T, T.r, 1, c)) return ERROR;
	w = gettoken(fp);
	if (w != RS && w != COMMA) return ERROR;
	if (w == COMMA)
	{
		w = gettoken(fp);
		if (!ParameList(fp, p)) return ERROR;
		InsertChild(T, T.r, 2, p);
	}
	return OK;
}

status FormParDef(FILE* fp, CTree& T)  //语法单位<形参>子程序
{
//w此前已读取第一个形参类型
	CTree c; //用于生成形参类型结点
	CTree p;  //用于生成形参变量结点
	T.n = 1; T.r = 0;	//生成形参结点
	T.nodes[0].data = (char*)malloc((strlen("形参：") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "形参：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;	
	if (!parse_type_specifier(fp, 0)) return ERROR;
	c.n = 1; c.r = 0;		//生成形参类型结点
	c.nodes[0].data = (char*)malloc((strlen(kind) + strlen("类型：") + 1) * sizeof(char));
	strcpy(c.nodes[0].data, "类型：");
	strcat(c.nodes[0].data, kind);
	c.nodes[0].indent = 1;
	c.nodes[0].firstchild = NULL;
	InsertChild(T, T.r, 1, c);
	if (w != IDENT) return ERROR;
	std::string parameter_name = token_text;
	p.n = 1; p.r = 0;   //生成形参变量结点
	p.nodes[0].data = (char*)malloc((strlen(token_text) + strlen("ID: ") + 1) * sizeof(char));
	strcpy(p.nodes[0].data, "ID: ");
	strcat(p.nodes[0].data, token_text);
	p.nodes[0].indent = 1;
	p.nodes[0].firstchild = NULL;
	InsertChild(T, T.r, 2, p);
	SemanticType parameter_type = semantic_type_from_spelling(kind);
	current_parameter_types.push_back(parameter_type);
	semantic_variables.push_back({ parameter_name, parameter_type, 0, current_type_is_const });
	return OK;
}

status CompStat(FILE* fp, CTree& T)  //语法单位<复合语句>子程序
{
	CTree c; CTree p; //c用于生成局部变量定义子树，p用于生成语句序列子树
	status flag;
	print elem;
	T.n = 1; T.r = 0;		//生成复合语句结点
	T.nodes[0].data = (char*)malloc((strlen("复合语句：") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "复合语句：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;
	//注意其中局部变量说明和语句序列都可以为空
	w = gettoken(fp);
	elem = { ++indent0,line_num };
	printList.push(elem);  //添加缩进值
	if (is_type_token(w))
	{
		if (!LocVarList(fp, c)) return ERROR;
		if (!InsertChild(T, T.r, 1, c)) return ERROR;
		flag = StatList(fp, p);
		if (!flag) return ERROR;
		if (flag == OK)
			if (!InsertChild(T, T.r, 2, p)) return ERROR;
	}
	else
	{
		flag = StatList(fp, p);
		if (!flag) return ERROR;
		if (flag == OK)
			if (!InsertChild(T, T.r, 1, p)) return ERROR;
	}
	elem = { --indent0,line_num };
	printList.push(elem);
	if (w != RL) return ERROR;
	w = gettoken(fp);
	return OK;
}

status LocVarList(FILE* fp, CTree& T)  //语法单位<局部变量定义序列>子程序
{
	CTree c; CTree p;//c生成局部变量定义子树，p生成可能存在的下一个局部变量定义序列子树
	status flag;
	if (!is_type_token(w))
		return INFEASIBLE;
	//读到的后继单词不为类型说明符时，变量定义序列结束
	T.n = 1;  T.r = 0;  //生成局部变量定义序列结点
	T.nodes[0].data = (char*)malloc((strlen("局部变量定义序列") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "局部变量定义序列");
	T.nodes[0].indent = 0;
	T.nodes[0].firstchild = NULL;
	if (!LocVarDef(fp, c)) return ERROR;
	if (!InsertChild(T, T.r, 1, c)) return ERROR;
	flag = LocVarList(fp, p);
	if (flag == OK) InsertChild(T, T.r, 2, p);
	if (!flag) return ERROR;
	return OK;
}

status LocVarDef(FILE* fp, CTree& T)//语法单位<局部变量定义>子程序
{
	CTree c;  CTree p; //c生成局部变量类型结点,p生成变量序列子树
	if (!parse_type_specifier(fp, 0)) return ERROR;
	T.n = 1; T.r = 0;		//生成局部变量定义结点
	T.nodes[0].data = (char*)malloc((strlen("局部变量定义：") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "局部变量定义：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;	
	
	c.n = 1; c.r = 0;		//生成局部变量类型结点
	c.nodes[0].data = (char*)malloc((strlen(kind) + strlen("类型：") + 1) * sizeof(char));
	strcpy(c.nodes[0].data, "类型：");
	strcat(c.nodes[0].data, kind);
	c.nodes[0].indent = 1;
	c.nodes[0].firstchild = NULL;
	if (!InsertChild(T, T.r, 1, c)) return ERROR;
	if (w != IDENT) return ERROR;
	strcpy(tokenText0, token_text);
	w = gettoken(fp);
	if (!VarList(fp, p)) return ERROR;
	if (!InsertChild(T, T.r, 2, p)) return ERROR;
	return OK;
}

status StatList(FILE* fp, CTree& T)  //语法单位<语句序列>子程序
{
	CTree c; CTree p;  //c生成语句树,p生成可能出现的语句序列树
	status flag;
	flag = Statement(fp, c);
	if (flag == INFEASIBLE) return INFEASIBLE;
	if (!flag) return ERROR;
	else
	{
		T.n = 1; T.r = 0;		//生成语句序列结点
		T.nodes[0].data = (char*)malloc((strlen("语句序列") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "语句序列");
		T.nodes[0].indent = 0;
		T.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 1, c);
		flag = StatList(fp, p);
		if (!flag) return ERROR;
		if (flag == OK)
			if (!InsertChild(T, T.r, 2, p))
				return ERROR;
	}
	return OK;
}

status Statement(FILE* fp, CTree& T)  //语法单位<语句>子程序
{
	CTree c; CTree p; CTree q; CTree k; 
	print elem;
	
	if (is_type_token(w)) {
		if (!LocVarDef(fp, T)) return ERROR;
		return OK;
	}
	else if (w == IF) {//分析条件语句,p用于生成表达式树,q用于生成if模块子句数，k用于生成else模块子句数
		w = gettoken(fp);
		if (w != LS) return ERROR;
		w = gettoken(fp);
		if (w == RS) return ERROR;
		if (!exp(fp, p, RS)) return ERROR;
		c.n = 1; c.r = 0;  //生成if语句子树
		c.nodes[0].data = (char*)malloc((strlen("条件：") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "条件：");
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
		InsertChild(c, c.r, 1, p);
		w = gettoken(fp);
		if (w == LL)
		{
			if (!CompStat(fp, q)) return ERROR;
		}
		else
		{
			elem = { ++indent0,line_num };
			printList.push(elem);
			if (!Statement(fp, q)) return ERROR;
			elem = { --indent0,line_num };
			printList.push(elem);
		}
		p.n = 1; p.r = 0;
		p.nodes[0].data = (char*)malloc((strlen("IF子句：") + 1) * sizeof(char));
		strcpy(p.nodes[0].data, "IF子句：");
		p.nodes[0].indent = 1;
		p.nodes[0].firstchild = NULL;
		InsertChild(p, p.r, 1, q);
		if (w == ELSE)
		{
			w = gettoken(fp);
			if (w == LL)
			{
				if (!CompStat(fp, k)) return ERROR;
			}
			else
			{
				elem = { ++indent0,line_num };
				printList.push(elem);
				if (!Statement(fp, k)) return ERROR;
				elem = { --indent0,line_num };
				printList.push(elem);
			}
			q.n = 1; q.r = 0;  //生成else语句子树
			q.nodes[0].data = (char*)malloc((strlen("ELSE子句：") + 1) * sizeof(char));
			strcpy(q.nodes[0].data, "ELSE子句：");
			q.nodes[0].indent = 1;
			q.nodes[0].firstchild = NULL;
			InsertChild(q, q.r, 1, k);
			T.n = 1; T.r = 0;
			T.nodes[0].data = (char*)malloc((strlen("if-else语句：") + 1) * sizeof(char));
			strcpy(T.nodes[0].data, "if-else语句：");
			T.nodes[0].indent = 1;
			T.nodes[0].firstchild = NULL;
			InsertChild(T, T.r, 1, c);
			InsertChild(T, T.r, 2, p);
			InsertChild(T, T.r, 3, q);
		}
		else
		{
			T.n = 1; T.r = 0;
			T.nodes[0].data = (char*)malloc((strlen("if语句：") + 1) * sizeof(char));
			strcpy(T.nodes[0].data, "if语句：");
			T.nodes[0].indent = 1;
			T.nodes[0].firstchild = NULL;
			InsertChild(T, T.r, 1, c);
			InsertChild(T, T.r, 2, p);
		}
		return OK;
	}
	else if (w == LL) {
		if (!CompStat(fp, T)) return ERROR;
		return OK;
	}
	else if (w == FOR) {
		//分析for语句
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("for语句：") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "for语句：");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		c.n = 1; c.r = 0;
		c.nodes[0].data = (char*)malloc((strlen("初始表达式：") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "初始表达式：");
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 1, c);
		c.n = 1; c.r = 0;
		c.nodes[0].data = (char*)malloc((strlen("终止条件：") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "终止条件：");
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 2, c);
		c.n = 1; c.r = 0;
		c.nodes[0].data = (char*)malloc((strlen("循环表达式：") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "循环表达式：");
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 3, c);
		c.n = 1; c.r = 0;
		c.nodes[0].data = (char*)malloc((strlen("for子句：") + 1) * sizeof(char));
		strcpy(c.nodes[0].data, "for子句：");
		c.nodes[0].indent = 1;
		c.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 4, c);
		w = gettoken(fp);
		if (w != LS) return ERROR;
		w = gettoken(fp);
		if (!exp(fp, c, SEMI)) return ERROR;
		InsertChild(T, T.nodes[0].firstchild->child, 1, c);
		w = gettoken(fp);
		if (w == SEMI) return ERROR;
		if (!exp(fp, c, SEMI)) return ERROR;
		InsertChild(T, T.nodes[0].firstchild->next->child, 1, c);
		w = gettoken(fp);
		if (!exp(fp, c, RS)) return ERROR;
		InsertChild(T, T.nodes[0].firstchild->next->next->child, 1, c);
		w = gettoken(fp);
		if (w == LL)
		{
			if (!CompStat(fp, c)) return ERROR;
		}
		else
		{
			elem = { ++indent0,line_num };
			printList.push(elem);
			if (!Statement(fp, c)) return ERROR;
			elem = { --indent0,line_num };
			printList.push(elem);
		}
		InsertChild(T, T.nodes[0].firstchild->next->next->next->child, 1, c);
		return OK;
	}
	else if (w == WHILE) {
		w = gettoken(fp);
		if (w != LS) return ERROR;
		w = gettoken(fp);
		if (w == RS) return ERROR;
		if (!exp(fp, c, RS)) return ERROR;
		w = gettoken(fp);
		if (w == LL)
		{
			if (!CompStat(fp, p)) return ERROR;
		}
		else
		{
			elem = { ++indent0,line_num };
			printList.push(elem);
			if (!Statement(fp, p)) return ERROR;
			elem = { --indent0,line_num };
			printList.push(elem);
		}
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("while语句：") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "while语句：");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		InsertChild(T, T.r, 1, c);
		InsertChild(T, T.r, 2, p);
		return OK;
	}
	else if (w == CONTINUE) {
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("continue语句：") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "continue语句：");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		w = gettoken(fp);
		if (w != SEMI) {
			strcpy(parser_error, "continue语句缺少分号");
			return ERROR;
		}
		w = gettoken(fp);
		return OK;
	}
	else if (w == BREAK) {
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("break语句：") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "break语句：");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		w = gettoken(fp);
		if (w != SEMI) {
			strcpy(parser_error, "break语句缺少分号");
			return ERROR;
		}
		w = gettoken(fp);
		return OK;
	}
	else if (w == RETURN) {
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("return语句：") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "return语句：");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		w = gettoken(fp);
		if (w == SEMI) return ERROR;
		if (!exp(fp, c, SEMI)) return ERROR;
		if (last_expression_type != SEM_UNKNOWN && current_function_return_type != SEM_UNKNOWN &&
			last_expression_type != current_function_return_type) {
			strcpy(parser_error, "return表达式类型与函数返回类型不一致");
			return ERROR;
		}
		w = gettoken(fp);
		InsertChild(T, T.r, 1, c);
		return OK;
	}
	else if (w == LS || w == IDENT || w == INT_CONST || w == UNSIGNED_CONST || w == LONG_CONST || w == UNSIGNED_LONG_CONST || w == DOUBLE_CONST || w == FLOAT_CONST || w == LONG_DOUBLE_CONST || w == CHAR_CONST) {
		if (!exp(fp, T, SEMI)) return ERROR;
		w = gettoken(fp);
		return OK;
	}
	else if (w == RL) {
		return INFEASIBLE;
	}
	else if (w == SEMI) {
		T.n = 1; T.r = 0;
		T.nodes[0].data = (char*)malloc((strlen("空语句") + 1) * sizeof(char));
		strcpy(T.nodes[0].data, "空语句");
		T.nodes[0].indent = 1;
		T.nodes[0].firstchild = NULL;
		w = gettoken(fp);
		return OK;
	}
	else return ERROR;

}

status exp(FILE* fp, CTree& T, int endsym, int alt_endsym)//语法单位<表达式>子程序
{
	//已经读入了第一个单词在w中
	SqStack op;		//运算符栈
	SqStack opn;	//操作数栈
	std::stack<ExpressionSemantic> semantic_stack;
	CTree* node = (CTree*)malloc(sizeof(CTree));
	CTree* child1 = (CTree*)malloc(sizeof(CTree));
	CTree* child2 = (CTree*)malloc(sizeof(CTree));
	T.n = 1; T.r = 0;  //生成表达式结点
	T.nodes[0].data = (char*)malloc((strlen("表达式语句：") + 1) * sizeof(char));
	strcpy(T.nodes[0].data, "表达式语句：");
	T.nodes[0].indent = 1;
	T.nodes[0].firstchild = NULL;
	int error = 0;
	last_expression_end = -1;
	last_expression_type = SEM_UNKNOWN;
	node->n = 1; node->r = 0; //设立起止符号
	node->nodes[0].data = (char*)malloc((strlen("#") + 1) * sizeof(char));
	strcpy(node->nodes[0].data, "#");
	node->nodes[0].firstchild = NULL;
	InitStack(opn);  InitStack(op);  Push(op, node);		//初始化，将起止符#入栈
	while ((w != POUND || strcmp(node->nodes[0].data, "#")) && !error)  //当运算符栈栈顶不是起止符号，并没有错误时
	{
		if (w == IDENT || w == INT_CONST || w == UNSIGNED_CONST || w == LONG_CONST
			|| w == UNSIGNED_LONG_CONST || w == DOUBLE_CONST || w == FLOAT_CONST
			|| w == LONG_DOUBLE_CONST || w == CHAR_CONST)
		{
			SemanticType operand_type = SEM_UNKNOWN;
			ExpressionSemantic operand_semantic = { SEM_UNKNOWN, 0, 0, 0 };
			int operand_was_identifier = w == IDENT;
			std::string operand_name = token_text;
			if (operand_was_identifier) {
				const VariableSemanticInfo* variable = find_variable_info(token_text);
				operand_type = variable ? variable->type : SEM_UNKNOWN;
				operand_semantic.modifiable = variable ? !variable->is_const : 1;
				operand_semantic.is_array = variable ? variable->is_array : 0;
				operand_semantic.is_const = variable ? variable->is_const : 0;
			}
			else if (w == FLOAT_CONST) operand_type = SEM_FLOAT;
			else if (w == DOUBLE_CONST || w == LONG_DOUBLE_CONST) operand_type = SEM_DOUBLE;
			else operand_type = SEM_INT;
			operand_semantic.type = operand_type;
			node = (CTree*)malloc(sizeof(CTree));
			node->n = 1; node->r = 0;
			node->nodes[0].data = (char*)malloc((strlen(token_text) + strlen("ID: ") + 1) * sizeof(char));
			strcpy(node->nodes[0].data, "ID: ");
			strcat(node->nodes[0].data, token_text);
			node->nodes[0].indent = 1;
			node->nodes[0].firstchild = NULL;
			w = gettoken(fp);
			while (operand_was_identifier && (w == LS || w == LM)) {
				if (w == LS) {
					CTree* call_node = (CTree*)malloc(sizeof(CTree));
					call_node->n = 1; call_node->r = 0;
					std::string label = "函数调用：" + operand_name;
					call_node->nodes[0].data = (char*)malloc(label.size() + 1);
					strcpy(call_node->nodes[0].data, label.c_str());
					call_node->nodes[0].indent = 1;
					call_node->nodes[0].firstchild = NULL;
					int argument_position = 1;
					w = gettoken(fp);
					while (w != RS) {
						CTree argument;
						if (!exp(fp, argument, RS, COMMA)) return ERROR;
						if (!InsertChild(*call_node, call_node->r, argument_position++, argument)) return ERROR;
						int delimiter = last_expression_end;
						if (delimiter == RS) break;
						w = gettoken(fp);
					}
					w = gettoken(fp);
					node = call_node;
					const FunctionSemanticInfo* function = find_function_info(operand_name.c_str());
					operand_type = function ? function->return_type : SEM_UNKNOWN;
					operand_semantic = { operand_type, 0, 0, 0 };
				}
				else {
					CTree index_expression;
					w = gettoken(fp);
					if (w == RM || !exp(fp, index_expression, RM)) return ERROR;
					CTree* access_node = (CTree*)malloc(sizeof(CTree));
					access_node->n = 1; access_node->r = 0;
					access_node->nodes[0].data = (char*)malloc(strlen("数组访问") + 1);
					strcpy(access_node->nodes[0].data, "数组访问");
					access_node->nodes[0].indent = 1;
					access_node->nodes[0].firstchild = NULL;
					InsertChild(*access_node, access_node->r, 1, *node);
					InsertChild(*access_node, access_node->r, 2, index_expression);
					node = access_node;
					const VariableSemanticInfo* variable = find_variable_info(operand_name.c_str());
					operand_type = variable ? variable->type : SEM_UNKNOWN;
					operand_semantic = { operand_type, variable ? !variable->is_const : 1,
						0, variable ? variable->is_const : 0 };
					w = gettoken(fp);
				}
			}
			Push(opn, node);			//根据w生成一个结点，结点指针进栈opn
			semantic_stack.push(operand_semantic);
		}
		else if (w == PLUS || w == MINUS || w == MULTIPLY || w == DIVIDE ||
			w == MOD || w == LS || w == RS || ((w>=MORE)&&(w<= LESS_EQUAL)) || w == EQUAL_TO ||
			(w >= PLUS_EQUAL && w <= MOD_EQUAL) || w == AND || w == OR || w == POUND)
		{
			node = (CTree*)malloc(sizeof(CTree));
			GetTop(op, node);
			if (w == POUND) strcpy(token_text, "#");
			switch (precede(node->nodes[0].data, token_text))
			{
			case '<':
				node = (CTree*)malloc(sizeof(CTree));
				node->nodes[0].data = (char*)malloc((strlen(token_text) + 1) * sizeof(char));
				strcpy(node->nodes[0].data, token_text);
				node->nodes[0].indent = 1;
				node->nodes[0].firstchild = NULL;
				node->n = 1; node->r = 0;
				Push(op, node);		//根据w生成一个结点，结点指针进栈opn
				w = gettoken(fp);
				break;
			case '=':
				if (!Pop(op, node)) error++;
				w = gettoken(fp);
				break;   //去括号
			case '>': {
				ExpressionSemantic right_semantic = { SEM_UNKNOWN, 0, 0, 0 };
				ExpressionSemantic left_semantic = { SEM_UNKNOWN, 0, 0, 0 };
				if (!semantic_stack.empty()) { right_semantic = semantic_stack.top(); semantic_stack.pop(); }
				if (!semantic_stack.empty()) { left_semantic = semantic_stack.top(); semantic_stack.pop(); }
				if (!Pop(opn, child2)) error++;
				if (!Pop(opn, child1)) error++;
				if (!Pop(op, node)) error++;
				//根据运算符栈退栈得到的运算符node和操作数的结点指针child1和child2，
				//完成建立生成一个运算符的结点，结点指针进栈opn
				InsertChild(*node, node->r, 1, *child1);
				InsertChild(*node, node->r, 2, *child2);
				Push(opn, node);
				if (is_assignment_operator(node->nodes[0].data)) {
					if (left_semantic.is_array) {
						strcpy(parser_error, "数组不能整体参与赋值");
						error = 1;
					}
					else if (!left_semantic.modifiable) {
						strcpy(parser_error, left_semantic.is_const ?
							"const对象不能赋值" : "赋值左侧必须是可修改对象");
						error = 1;
					}
					else if (right_semantic.is_array || right_semantic.type == SEM_VOID) {
						strcpy(parser_error, "运算符两侧类型不兼容");
						error = 1;
					}
					semantic_stack.push({ left_semantic.type, 0, 0, 0 });
				}
				else if (left_semantic.is_array || right_semantic.is_array ||
					left_semantic.type == SEM_VOID || right_semantic.type == SEM_VOID) {
					strcpy(parser_error, "运算符两侧类型不兼容");
					error = 1;
					semantic_stack.push({ SEM_UNKNOWN, 0, 0, 0 });
				}
				else if (!strcmp(node->nodes[0].data, "==") || !strcmp(node->nodes[0].data, "!=") ||
					!strcmp(node->nodes[0].data, ">") || !strcmp(node->nodes[0].data, "<") ||
					!strcmp(node->nodes[0].data, ">=") || !strcmp(node->nodes[0].data, "<=") ||
					!strcmp(node->nodes[0].data, "&&") || !strcmp(node->nodes[0].data, "||"))
					semantic_stack.push({ SEM_INT, 0, 0, 0 });
				else semantic_stack.push({
					common_arithmetic_type(left_semantic.type, right_semantic.type), 0, 0, 0 });
				break;
			}
			default:
				if (w == endsym || w == alt_endsym) {
					last_expression_end = w;
					w = POUND;
				} //遇到结束标记，w被替换成#
				else error++;
			}
		}
		else if (w == endsym || w == alt_endsym) {
			last_expression_end = w;
			w = POUND;
		}//遇到结束标记，w被替换成#
		else {
			if (endsym == SEMI && (w == RL || w == EOF))
				strcpy(parser_error, "表达式语句缺少分号");
			error = 1;
		}
		GetTop(op, node);
	}
	if (error) return ERROR;
	GetTop(opn, node);
	InsertChild(T, T.r, 1, *node);
	if (!semantic_stack.empty()) last_expression_type = semantic_stack.top().type;
	return OK;
}

char precede(char* a, char* b)
{
	auto priority = [&](const char* op) {
		if (is_assignment_operator(op)) return 1;
		if (!strcmp(op, "||")) return 2;
		if (!strcmp(op, "&&")) return 3;
		if (!strcmp(op, "==") || !strcmp(op, "!=")) return 4;
		if (!strcmp(op, "<") || !strcmp(op, ">") ||
			!strcmp(op, "<=") || !strcmp(op, ">=")) return 5;
		if (!strcmp(op, "+") || !strcmp(op, "-")) return 6;
		if (!strcmp(op, "*") || !strcmp(op, "/") || !strcmp(op, "%")) return 7;
		return -1;
	};

	if (!strcmp(a, "#") && !strcmp(b, "#")) return '=';
	if (!strcmp(a, "#")) return strcmp(b, ")") ? '<' : '?';
	if (!strcmp(a, "(") && !strcmp(b, ")")) return '=';
	if (!strcmp(a, "(")) return strcmp(b, "#") ? '<' : '?';
	if (!strcmp(b, "(")) return '<';
	if (!strcmp(b, ")") || !strcmp(b, "#")) return '>';
	if (!strcmp(a, ")")) return '>';

	int left_priority = priority(a);
	int right_priority = priority(b);
	if (left_priority < 0 || right_priority < 0) return '?';
	if (left_priority < right_priority) return '<';
	if (left_priority > right_priority) return '>';
	return is_assignment_operator(a) ? '<' : '>';
}

status PrintTree(char* data, int indent) //打印函数
{
	int i;
	for (i = 0; i < indent; i++)
		printf("\t");
	printf("%s\n", data);
	return OK;
}
