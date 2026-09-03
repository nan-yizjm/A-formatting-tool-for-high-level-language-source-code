#include"preprocess.h"
#include"lexer.h"
#include<filesystem>
#include<string>

#ifndef CAPSTONE_DEMO_INCLUDE_DIR
#define CAPSTONE_DEMO_INCLUDE_DIR "demo_include"
#endif

static status expand_include(FILE* output, const char* header_text,
	const char* source_path, int quoted)
{
	std::string header_name = header_text;
	if (quoted && header_name.size() >= 2)
		header_name = header_name.substr(1, header_name.size() - 2);

	std::filesystem::path candidates[2];
	int candidate_count = 0;
	if (quoted && source_path && source_path[0])
		candidates[candidate_count++] =
			std::filesystem::path(source_path).parent_path() / header_name;
	candidates[candidate_count++] =
		std::filesystem::path(CAPSTONE_DEMO_INCLUDE_DIR) / header_name;

	FILE* header_fp = NULL;
	for (int i = 0; i < candidate_count && !header_fp; i++)
		header_fp = fopen(candidates[i].string().c_str(), "r");
	if (!header_fp) return ERROR;

	int c;
	while ((c = fgetc(header_fp)) != EOF) {
		// include指令在原文件中只占一行；压平演示头，避免后续报错行号偏移。
		if (c == '\r') {
			int next = fgetc(header_fp);
			if (next != '\n' && next != EOF) ungetc(next, header_fp);
			fputc(' ', output);
		}
		else if (c == '\n') fputc(' ', output);
		else fputc(c, output);
	}
	fputc('\n', output);
	fclose(header_fp);
	return OK;
}

static int directive_is_at_line_start(FILE* fp)
{
	long return_position = ftell(fp);
	if (return_position < 0) return 0;
	long position = return_position - 2; //当前位置在#之后，从#前一个字符开始检查
	while (position >= 0) {
		if (fseek(fp, position, SEEK_SET) != 0) return 0;
		int c = fgetc(fp);
		if (c == '\n') break;
		if (c != ' ' && c != '\t' && c != '\r') {
			fseek(fp, return_position, SEEK_SET);
			return 0;
		}
		position--;
	}
	fseek(fp, return_position, SEEK_SET);
	return 1;
}
define_data data_Def[10];//用于储存define宏定义的内容，全局
include_data data_Inculd[10];//用于储存include文件包含的内容，全局
int data_Def_num;//宏定义个数

//按源文件的行距补齐换行，避免空行丢失导致后续报错行号偏移
static void write_newlines(FILE* output, int count)
{
	for (int k = 0; k < count; k++) fputc('\n', output);
}

status pre_process(FILE* fp, const char* source_path) {
	int w; //接受gettoken读取的返回值
	int i=0,j=0,m;//i是宏定义个数，j是include个数
	int pre_line_num=1;//用于记录换行情况
	char container;//暂时存储字符判断结尾处的分号
	int a, b;//比较行数
	int flag=0;//判断语句中是否出现define的定义
	FILE* mid_fp;
	char filename[50];
	strcpy(filename, "../output/C_mid_file.txt"); //中间文件
	mid_fp = fopen(filename, "w");
	w = gettoken(fp);
	do {
		if (w == POUND) {
			if (!directive_is_at_line_start(fp)) return ERROR;
			w = gettoken(fp);
			if (w == DEFINE) {
				w = gettoken(fp);
				a = line_num;
				if (w == ERROR_TOKEN)return ERROR;
				else if (w == SEMI)return ERROR;
				else if (w == POUND)return ERROR;
				else {
					strcpy(data_Def[i].ident, token_text);
				}
				w = gettoken(fp);
				b = line_num;
				if (w == ERROR_TOKEN)return ERROR;
				else if (w == SEMI)return ERROR;
				else if (w == POUND)return ERROR;
				else {
					strcpy(data_Def[i++].string, token_text);
				}
				if (a != b)return ERROR;
				data_Def_num = i;
				fprintf(mid_fp, "\n");
				w = gettoken(fp); 
				pre_line_num = line_num;
				continue;
			}
			else if (w == INCLUDE) {
				w = gettoken(fp);
				if (w == ERROR_TOKEN) return ERROR;
				else if(w== STRING_CONST){ 
					strcpy(data_Inculd[j++].string, token_text); 
					if (!expand_include(mid_fp, token_text, source_path, 1)) return ERROR;
					if ((container = fgetc(fp)) != ';') { ungetc(container, fp); w = gettoken(fp); pre_line_num = line_num; continue; }
					else return ERROR;
				}
				else if (w == HEADER_NAME) {  //token_text形如<stdio.h>，去掉尖括号得到头文件名
					char header_name[100];
					int header_length = (int)strlen(token_text);
					if (header_length >= 3 && token_text[0] == '<' && token_text[header_length - 1] == '>') {
						int name_length = header_length - 2;
						strncpy(header_name, token_text + 1, name_length);
						header_name[name_length] = '\0';
					}
					else return ERROR;  //缺少右尖括号或名字为空
					strcpy(data_Inculd[j++].string, header_name);
					if (!expand_include(mid_fp, header_name, source_path, 0)) return ERROR;
					if ((container = fgetc(fp)) != ';') { ungetc(container, fp); w = gettoken(fp); pre_line_num = line_num; continue; }
					else return ERROR;
				}
			}
			else return ERROR;
		}
		data_Def_num = i;

		if (w != POUND) {

			if (w == IDENT) { //是标识符时，判断是不是define的类型
				for (m = 0; m < data_Def_num; m++) {
					if (!strcmp(token_text, data_Def[m].ident)) {
						if (pre_line_num != line_num) {
							write_newlines(mid_fp, line_num - pre_line_num);
							fprintf(mid_fp, "%s ", data_Def[m].string);
							flag = 1;
						}
						else { fprintf(mid_fp, "%s ", data_Def[m].string); flag = 1; }
					}
				}
				if (flag != 0) {
					flag = 0;
				}
				else {
					if (pre_line_num != line_num) {
						write_newlines(mid_fp, line_num - pre_line_num);
						fprintf(mid_fp, "%s ", token_text);
					}
					else { fprintf(mid_fp, "%s ", token_text); }
				}
			}
			else if (w == LINENOTE) {
				pre_line_num = line_num;
				w = gettoken(fp); 
				continue;
			}
			else if (w == BLOCKNOTE) {
			
				write_newlines(mid_fp, line_num - pre_line_num);
				pre_line_num = line_num;
				w = gettoken(fp);
				continue;
			}
			else if (w == ERROR_TOKEN) {
				if (pre_line_num != line_num) {
					write_newlines(mid_fp, line_num - pre_line_num);
						fprintf(mid_fp, "%s ", token_text);
				}
				else { fprintf(mid_fp, "%s ", token_text); }
			}
			else {
				if (pre_line_num != line_num) {
					write_newlines(mid_fp, line_num - pre_line_num);
						fprintf(mid_fp, "%s ", token_text);
				}
				else { fprintf(mid_fp, "%s ", token_text); }
			}
		}
		pre_line_num = line_num;

		w = gettoken(fp);
	} while (w != EOF);
	fclose(mid_fp);
	return OK;
}
