# MyTinyHttpd
一个简易的 http 服务器
## 函数声明
``` text
/**************************************************/
// 错误输出
void error_die(const char *sc);

// 绑定监听套接字
int startup(int *port);

// 每次收到请求，创建一个线程来处理接受到的请求
void accept_request(void *arg);

// 得到一行数据,只要发现c为\n,就认为是一行结束，如果读到\r,再用MSG_PEEK的方式读入一个字符，如果是\n，从socket用读出
// 如果是下个字符则不处理，将c置为\n，结束。如果读到的数据为0中断，或者小于0，也视为结束，c置为\n
int get_line(int, char *, int);

// 没有发现文件
void not_found(int);

// 接读取文件返回给请求的http客户端
void serve_file(int client, const char *filename);

// 返回http头
void headers(int, const char *);

// 读取文件
void cat(int, FILE *);

// 执行cgi文件
void execute_cgi(int, const char *, const char *, const char *);

// 错误请求
void bad_request(int client);

// 无法执行cgi文件
void cannot_execute(int client);

// 如果不是Get或者Post，就报方法没有实现
void unimplemented(int);
/**************************************************/
```