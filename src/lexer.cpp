#include"lexer.h"
int line_num = 1;
char token_text[100];
char token_error[100];
keyword n[IDENT] = {
	{"auto",AUTO},{"break",BREAK},{"case",CASE},{"char",CHAR},{"const",CONST},{"continue",CONTINUE},{"default",DEFAULT},{"do",DO},
	{"double",DOUBLE},{"else",ELSE},{"enum",ENUM},{"extern",EXTERN},{"float",FLOAT},{"for",FOR},{"goto",GOTO},{"if",IF},
	{"int",INT},{"long",LONG},{"register",REGISTER},{"return",RETURN},{"short",SHORT},{"signed",SIGNED},{"sizeof",SIZEOF},{"static",STATIC},
	{"struct",STRUCT},{"switch",SWITCH},{"typedef",TYPEDEF},{"union",UNION},{"unsigned",UNSIGNED},{"void",VOID},{"volatile",VOLATILE},{"while",WHILE},
	{"include",INCLUDE},{"define",DEFINE}
};

static int just_saw_pound = 0;    //上一个单词是#
static int expect_header_name = 0; //刚识别完#include，等待<头文件名>

static int finish_invalid_char_constant(FILE* fp, int c, int i)
{
	while (c != '\'' && c != '\n' && c != EOF) {
		token_text[i++] = (char)c;
		c = fgetc(fp);
	}
	if (c == '\'') token_text[i++] = (char)c;
	else if (c == '\n') ungetc(c, fp);
	token_text[i] = '\0';
	strcpy(token_error, "非法字符常量");
	return ERROR_TOKEN;
}

int gettoken(FILE* fp) {
	int c; //必须使用int，才能可靠地区分所有字符和EOF
	int i=0; //用于做token_text的存储
	int j=0; //用于与关键字做比对
	token_text[0] = '\0';
	token_error[0] = '\0';
	
	while (( c = fgetc(fp)) == ' ' || c == '\n'|| c == '\t'|| c == '\r' ||c==EOF)  //跳过空白符和制表符
	{
		if (c == '\n') line_num++;	//读取换行符时计数
		if (c == EOF) { just_saw_pound = 0; expect_header_name = 0; return EOF; }
	}
	if (c == EOF) { just_saw_pound = 0; expect_header_name = 0; return EOF; }
	//include状态机：#后面只关心关键字include，include后面只关心<头文件名>
	if (expect_header_name && c != '<') expect_header_name = 0;
	if (just_saw_pound && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'))
		just_saw_pound = 0;
	//判断标识符或者关键字
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		do {
			token_text[i++] = c;
			c = fgetc(fp);
		} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'));
		token_text[i] = '\0';
		ungetc(c, fp);

		for (; j < IDENT; j++) {
			if (!strcmp(token_text, n[j].key)) {
				if (just_saw_pound && n[j].enum_key == INCLUDE) expect_header_name = 1;
				just_saw_pound = 0;
				return n[j].enum_key;
			}
		}
		just_saw_pound = 0;
		return IDENT;
	}

	//判断标识符
	if (c == '_') {
		do {
			token_text[i++] = c;
			c = fgetc(fp);
		} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'));
		token_text[i] = '\0';
		ungetc(c, fp);
		just_saw_pound = 0;
		return IDENT;
	}

	//判断数字（整型或浮点型）
	if (c > '0' && c <= '9') {  //首非0
		token_text[i++] = c;
		c = fgetc(fp);
		if (c >= '0' && c <= '9') {
			do {
				token_text[i++] = c;
				c = fgetc(fp);
			} while (c >= '0' && c <= '9'); 

			if (c == 'u' || c == 'U') {
stationA:  //u/U后后缀
				token_text[i++] = c;
				c = fgetc(fp);
				if (c == 'l' || c == 'L') {
					token_text[i++] = c;
					token_text[i] = '\0';
					return UNSIGNED_LONG_CONST;
				}
				else {
					ungetc(c, fp);
					token_text[i] = '\0';
					return UNSIGNED_CONST;
				}
			}
			else if (c == 'l' || c == 'L') {
stationB: //l/L后缀(int)
				token_text[i++] = c;
				token_text[i] = '\0';
				return LONG_CONST;
			}
			else if (c == '.') {
				goto station1;
			}
			else if (c == 'e' || c == 'E') {
				goto stationE;
			}
			else {
				ungetc(c, fp);
				token_text[i] = '\0';
				return INT_CONST;
			}

		}
		else if (c == '.') {
station1://情况1（小数点）*可能没数字情况
			do {
				token_text[i++] = c;
				c = fgetc(fp);
			} while (c >= '0' && c <= '9');
			if(c=='f'||c=='F'){
stationC: //f/F后缀
				token_text[i++] = c;
				token_text[i] = '\0';
				return FLOAT_CONST;
			}
			else if (c == 'l' || c == 'L') {
stationD: //l/L后缀（double）
				token_text[i++] = c;
				token_text[i] = '\0';
				return LONG_DOUBLE_CONST;
			}
			else if (c == 'e' || c == 'E') {
stationE:  //e/E的情况
				token_text[i++] = c;
				c = fgetc(fp);
				if (c == '+' || c == '-') {
					token_text[i++] = c;
					c = fgetc(fp);
				}
				if (c >= '0' && c <= '9') {
					do {
						token_text[i++] = c;
						c = fgetc(fp);
					} while (c >= '0' && c <= '9');
					if (c == 'f' || c == 'F') {
						goto stationC;
					}
					else if (c == 'l' || c == 'L') {
						goto stationD;
					}
					else {
						ungetc(c, fp);
						token_text[i] = '\0';
						return DOUBLE_CONST;
					}
				}	
				else goto stationERROR;
			}
			else {
				ungetc(c, fp);
				token_text[i] = '\0';
				return DOUBLE_CONST;
			}

		}
		else if (c == 'e' || c == 'E') {
			goto stationE;
		}
		else if (c == 'u' || c == 'U') {
			goto stationA;
		}
		else if (c == 'l' || c == 'L') {
			goto stationB;
		}
		else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
stationERROR:
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_') {
				do {
					token_text[i++] = (char)c;
					c = fgetc(fp);
				} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') || c == '_' || c == '.');
			}
			if (c != EOF) ungetc(c, fp);
			token_text[i] = '\0';
			if (token_text[0] >= '0' && token_text[0] <= '9')
				strcpy(token_error, "非法数值常量");
			return ERROR_TOKEN;
		}
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return INT_CONST;
		}
	}


	if (c == '0') {  //首为0
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == 'x' || c == 'X') {  //判断十六进制数
			token_text[i++] = c;
			c = fgetc(fp);
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
				do {
					token_text[i++] = c;
					c = fgetc(fp);
				} while ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
				if (c == 'u' || c == 'U') goto stationA;
				else if (c == 'l' || c == 'L') goto stationB;
				else if ((c >= 'g' && c <= 'z' && c != 'u' && c != 'l') || (c >= 'G' && c <= 'Z' && c != 'U' && c != 'L')) goto stationERROR;
				else {
					ungetc(c, fp);
					token_text[i] = '\0';
					return INT_CONST;
				}
			}
			else goto stationERROR;
		}
		else if (c >= '0' && c <= '7') { //判断八进制数
			do {
				token_text[i++] = c;
				c = fgetc(fp);
			} while (c >= '0' && c <= '7');
			if (c == 'u' || c == 'U') goto stationA;
			else if (c == 'l' || c == 'L') goto stationB;
			else if ((c >= 'a' && c <= 'z' && c != 'u' && c != 'l') || (c >= 'A' && c <= 'Z' && c != 'U' && c != 'L') || c == '_' || (c >= '8' && c <= '9')) goto stationERROR;
			else {
				ungetc(c, fp);
				token_text[i] = '\0';
				return INT_CONST;
			}
		}
		else if (c == '.') goto station1;
		else if (c == 'e' || c == 'E') goto stationE;
		else if (c == 'u' || c == 'U') goto stationA;
		else if (c == 'l' || c == 'L') goto stationB;
		else if ((c >= 'a' && c <= 'z' ) || (c >= 'A' && c <= 'Z' ) || c == '_' || (c >= '8' && c <= '9')) goto stationERROR;
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return INT_CONST;
		}
		
	}

	if (c == '.') {  //出现小数点的情况*后面必须出现数字
		token_text[i++] = c;
		c = fgetc(fp);
		if (c >= '0' && c <= '9') {
			do {
				token_text[i++] = c;
				c = fgetc(fp);
			} while (c >= '0' && c <= '9');

			if (c == 'f' || c == 'F')
			{
				goto stationC;
			}
			else if (c == 'l' || c == 'L')
			{
				goto stationD;
			}
			else if (c == 'e' || c == 'E') {
				goto stationE;
			}
			else {
				ungetc(c, fp);
				token_text[i] = '\0';
				return DOUBLE_CONST;
			}
		}
		else {
			strcpy(token_error, "不支持的符号");
			if (c != EOF) ungetc(c, fp);
			token_text[i] = '\0';
			return ERROR_TOKEN;
		}
	}

	switch (c) {
	case '=':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return EQUAL;
		}
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return EQUAL_TO;
		}
	case'{':
		token_text[i++] = c;
		token_text[i] = '\0';
		return LL;

	case'}':
		token_text[i++] = c;
		token_text[i] = '\0';
		return RL;

	case'[':
		token_text[i++] = c;
		token_text[i] = '\0';
		return LM;

	case']':
		token_text[i++] = c;
		token_text[i] = '\0';
		return RM;

	case'(':
		token_text[i++] = c;
		token_text[i] = '\0';
		return LS;

	case')':
		token_text[i++] = c;
		token_text[i] = '\0';
		return RS;

	case';':
		token_text[i++] = c;
		token_text[i] = '\0';
		return SEMI;

	case',':
		token_text[i++] = c;
		token_text[i] = '\0';
		return COMMA;

	case'#':
		token_text[i++] = c;
		token_text[i] = '\0';
		just_saw_pound = 1;
		return POUND;

	case'>':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return MORE_EQUAL;
		}
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return MORE;
		}

	case'<':
		if (expect_header_name) {  //#include后面的<...>整体作为头文件名，不再拆分
			expect_header_name = 0;
			token_text[0] = '<';
			i = 1;
			while ((c = fgetc(fp)) != EOF && c != '>' && c != '\n') {
				if (i < 99) token_text[i++] = (char)c;
			}
			if (c == '>') {
				if (i < 99) token_text[i++] = '>';
			}
			else if (c == '\n') line_num++;
			token_text[i] = '\0';
			return HEADER_NAME;
		}
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return LESS_EQUAL;
		}
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return LESS;
		}

	case'!':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return UNEQUAL;
		}
		else {
			goto stationERROR;
		}

	case'&':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '&') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return AND;
		}
		else {
			goto stationERROR;
		}

	case'|':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '|') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return OR;
		}
		else {
			goto stationERROR;
		}

	case'+':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return PLUS_EQUAL;
		}
		ungetc(c, fp);
		token_text[i] = '\0';
		return PLUS;

	case'-':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return MINUS_EQUAL;
		}
		ungetc(c, fp);
		token_text[i] = '\0';
		return MINUS;

	case'*':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return MULTIPLY_EQUAL;
		}
		ungetc(c, fp);
		token_text[i] = '\0';
		return MULTIPLY;

	case'%':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return MOD_EQUAL;
		}
		ungetc(c, fp);
		token_text[i] = '\0';
		return MOD;

	case'/':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '=') {
			token_text[i++] = c;
			token_text[i] = '\0';
			return DIVIDE_EQUAL;
		}
		else if (c == '/') {   //判断行注释：只跳过不缓存，避免长注释撑爆 token_text
			do {
				c = fgetc(fp);
			} while (c != '\n' && c != EOF);
			if (c == '\n'|| c == EOF) {
				ungetc(c, fp);
				return LINENOTE;
			}
		}
		else if (c == '*') {   //判断块注释：只跳过不缓存，避免长注释撑爆 token_text
			while ((c = fgetc(fp)) != EOF) {
				if (c == '*') {
					c = fgetc(fp);
					if (c == '/') return BLOCKNOTE;
					if (c == EOF) break;
					if (c == '\n') line_num++;
				}
				else if (c == '\n') {
					line_num++;
				}
			}
			strcpy(token_text, "/*");
			strcpy(token_error, "未闭合的块注释");
			return ERROR_TOKEN;
		}
		else {
			ungetc(c, fp);
			token_text[i] = '\0';
			return DIVIDE;
		}

	case'\'':
		token_text[i++] = c;
		c = fgetc(fp);
		if (c == '\\') {   //判断‘\x’的情况
			token_text[i++] = c;
			c = fgetc(fp);
			if (c >= '0' && c <= '7') {  //八进制转义最多包含三个0～7数字
				int octal_digits = 0;
				do {
					token_text[i++] = c;
					c = fgetc(fp);
					octal_digits++;
				} while (c >= '0' && c <= '7' && octal_digits < 3);
				if (c == '\'') {  
					token_text[i++] = c;
					token_text[i] = '\0';
					return CHAR_CONST;
				}
				else return finish_invalid_char_constant(fp, c, i);
			}
			else if (c == '8' || c == '9') {
				return finish_invalid_char_constant(fp, c, i);
			}
			else if (c == '\'') {  //判断'\''的情况
				token_text[i++] = c;
				c = fgetc(fp);
				if (c == '\'') {
					token_text[i++] = c;
					token_text[i] = '\0';
					return CHAR_CONST;
				}
				else return finish_invalid_char_constant(fp, c, i);
			}
			else if (c == 'x' || c == 'X') {  //十六进制转义至少包含一个十六进制数字
				token_text[i++] = c;
				c = fgetc(fp);
				if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
					(c >= 'A' && c <= 'F'))) {
					return finish_invalid_char_constant(fp, c, i);
				}
				do {
					token_text[i++] = c;
					c = fgetc(fp);
				} while ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
					(c >= 'A' && c <= 'F'));
				if (c == '\'') {
					token_text[i++] = c;
					token_text[i] = '\0';
					return CHAR_CONST;
				}
				return finish_invalid_char_constant(fp, c, i);
			}
			else if (strchr("\"?\\abfnrtv", c) != NULL) {  //标准简单转义字符
				token_text[i++] = c;
				c = fgetc(fp);
				if (c == '\'') {
					token_text[i++] = c;
					token_text[i] = '\0';
					return CHAR_CONST;
				}
				else return finish_invalid_char_constant(fp, c, i);
			}
			else {
				return finish_invalid_char_constant(fp, c, i);
			}
		}
		else if (c == '\'') {  //空字符常量
			token_text[i++] = (char)c;
			token_text[i] = '\0';
			strcpy(token_error, "空字符常量");
			return ERROR_TOKEN;
		}
		else {  //判断‘x’的情况，x不能为'和\在上面讨论过
			token_text[i++] = c;
			c = fgetc(fp);
			if (c == '\'') {
				token_text[i++] = c;
				token_text[i] = '\0';
				return CHAR_CONST;
			}
			else return finish_invalid_char_constant(fp, c, i);
		}

	case'"':
		token_text[i++] = c;
		while ((c = fgetc(fp)) != '"') {
			token_text[i++] = c;
			if (c == '\\') {
				c = fgetc(fp);
				if (c == '"') {  //识别字符串最后一个字符为'\\'的错误情况【亮点】
					do {
						token_text[i++] = c;
						c = fgetc(fp);
					} while ((c != '"' )&& (c != '\n' )&& (c != EOF));
					if (c == '"') { token_text[i++] = c; token_text[i] = '\0'; return STRING_CONST; }
					else goto stationERROR;
				}
				else if (c == '\n' || c == '\r') {  //反斜杠续行，兼容 CRLF 换行
					if (c == '\r') {
						c = fgetc(fp);
						if (c != '\n') ungetc(c, fp);
						else c = '\n';
					}
					line_num++; token_text[i++] = c;
				}
				else token_text[i++] = c;
			}
			else if (c == '\n') { //判断直接换行情况【亮点】
				line_num++;
				while ((c = fgetc(fp)) != '"' && c != '\n' && c != EOF)
					token_text[i++] = (char)c;
				if (c == '"') token_text[i++] = (char)c;
				else if (c == '\n') {
					token_text[i++] = (char)c;
					line_num++;
				}
				token_text[i] = '\0';
				strcpy(token_error, "非法字符串常量");
				return ERROR_TOKEN;
			}
			else if (c == EOF) goto stationERROR;
		}
		token_text[i++] = c;
		token_text[i] = '\0';
		return STRING_CONST;

	default:
		if (c == EOF) return EOF;
		token_text[0] = (char)c;
		token_text[1] = '\0';
		strcpy(token_error, "不支持的符号");
		return ERROR_TOKEN;

	}//switch结束


}
