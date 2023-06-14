#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>

#include <ctype.h>
#include <unistd.h>

#include <sys/wait.h>

#include <pthread.h>

typedef struct sockaddr_in sockaddr_in;

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

// 宏定义，是否是空格
#define ISspace(x) isspace((int)(x))

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

// 如果方法没有实现，就返回此信息
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

// 把错误信息写到 perror 并退出。
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

// 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
int startup(int *port)
{
    int serv_sock = 0;
    struct sockaddr_in serv_addr; // 定义在<netinet/in.h>中，是一个结构体，用于存储IPv4地址信息

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        error_die("socket");
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(*port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_die("bind");
    }

    // 如果端口没有设置，提供个随机端口
    if (*port == 0) /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(serv_addr);
        /*
        getsockname() 返回当前名称指定的套接字。namelen 参数应被初始化，以指示的空间指向量的名字。返回时，包含名称的实际大小（以字节为单位）.
        */
        if (getsockname(serv_sock, (struct sockaddr *)&serv_addr, &namelen) == -1)
        {
            error_die("getsockname");
        }
        *port = ntohs(serv_addr.sin_port);
    }

    // 监听
    if (listen(serv_sock, 5) == -1)
    {
        error_die("listen");
    }
    return (serv_sock);
}

// 读取套接字的一行，把回车换行等情况都统一为换行符结束。
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}

/*
处理从套接字上监听到的一个 HTTP 请求
在这里可以很大一部分地体现服务器处理请求流程。
*/
void accept_request(void *arg)
{
    int clnt_sock = *(int *)arg;

    char buf[1024];

    //"GET / HTTP/1.1\n"
    int numchars = get_line(clnt_sock, buf, sizeof(buf));
    // printf("\nclnt_sock = %d\tnumchars = %d\tbuf = %s", clnt_sock, numchars, buf);

    int i = 0, j = 0;
    char method[255];
    // 第一行字符串提取Get
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j = i;
    method[i] = '\0';
    // printf("method = %s\n", method);

    // 判断是Get还是Post
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(clnt_sock);
        return;
    }

    int cgi = 0;
    // 如果是POST，cgi置为1
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while (ISspace(buf[j]) && (j < numchars))
    {
        j++;
    }
    char url[255];
    //  得到 "/"
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';
    // printf("url = %s\n", url);
    /*注意：如果你的http的网址为http://192.168.0.23:47310/index.html
    那么你得到的第一条http信息为GET /index.html HTTP/1.1，那么
    解析得到的就是/index.html */

    char *query_string = NULL;
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
        {
            query_string++;
        }
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    // printf("query_string = %s\n", query_string);

    char path[512];
    sprintf(path, "htdocs%s", url);

    // 默认地址，解析到的路径如果为/，则自动加上index.html
    if (path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }
    // printf("path = %s\n", path); // path = htdocs/index.html

    struct stat st;
    // 获得文件信息
    if (stat(path, &st) == -1)
    {
        // printf("path error\n");
        // 把所有http信息读出然后丢弃
        while ((numchars > 0) && strcmp("\n", buf))
        {
            numchars = get_line(clnt_sock, buf, sizeof(buf));
        }

        // 没有找到
        not_found(clnt_sock);
    }
    else
    {
        // printf("path OK\n");
        if ((st.st_mode & __S_IFMT) == __S_IFDIR)
        {
            strcat(path, "/index.html");
        }
        // 如果你的文件默认是有执行权限的，自动解析成cgi程序，
        // 如果有执行权限但是不能执行，会接受到报错信号
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            cgi = 1;
        }
        if (!cgi)
        {
            // printf("serve_file\n");
            // 接读取文件返回给请求的http客户端
            serve_file(clnt_sock, path);
        }
        else
        {
            // printf("execute_cgi\n");
            // 执行cgi文件
            execute_cgi(clnt_sock, path, method, query_string);
        }
    }
    close(clnt_sock);
}

// 运行 cgi 程序的处理，也是个主要函数。
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    // 缓冲区
    char buf[1024];

    // 2根管道
    int cgi_output[2];
    int cgi_input[2];

    // 进程pid和状态
    pid_t pid;
    int status;

    int i;
    char c;

    // 读取的字符数
    int numchars = 1;

    // http的content_length
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';

    // 忽略大小写比较字符串
    if (strcasecmp(method, "GET") == 0)
    {
        // 读取数据，把整个header都读掉，以为Get写死了直接读取index.html，
        // 没有必要分析余下的http信息了
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    else
    {
        /* POST */
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            /*如果是POST请求，就需要得到Content-Length，
            Content-Length：这个字符串一共长为15位，所以
            取出头部一句后，将第16位设置结束符，进行比较
            第16位置为结束*/
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
            {
                // 内存从第17位开始就是长度，
                //  将17位开始的所有字符串转成整数就是content_length
                content_length = atoi(&(buf[16]));
            }
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1)
        {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    // 建立output管道
    if (pipe(cgi_output) < 0)
    {
        cannot_execute(client);
        return;
    }

    // 建立input管道
    if (pipe(cgi_input) < 0)
    {
        cannot_execute(client);
        return;
    }
    // fork后管道都复制了一份，都是一样的
    // 子进程关闭2个无用的端口，避免浪费
    // ×<------------------------->1    output
    // 0<-------------------------->×   input

    // 父进程关闭2个无用的端口，避免浪费
    // 0<-------------------------->×   output
    // ×<------------------------->1    input
    // 此时父子进程已经可以通信

    // fork进程，子进程用于执行CGI
    // 父进程用于收数据以及发送子进程处理的回复数据
    if ((pid = fork()) < 0)
    {
        cannot_execute(client);
        return;
    }
    if (pid == 0) /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        // 子进程输出重定向到output管道的1端
        dup2(cgi_output[1], 1);

        // 子进程输入重定向到input管道的0端
        dup2(cgi_input[0], 0);

        // 关闭无用管道口
        close(cgi_output[0]);
        close(cgi_input[1]);

        // CGI环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0)
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else
        { /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        // 替换执行path
        execl(path, path, NULL);
        // 如果path有问题，例如将html网页改成可执行的，但是执行后m为-1
        // 退出子进程，管道被破坏，但是父进程还在往里面写东西，触发Program received signal SIGPIPE, Broken pipe.
        exit(0);
    }
    else
    { /* parent */
        // 关闭无用管道口
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
        {
            for (i = 0; i < content_length; i++)
            {
                // 得到post请求数据，写到input管道中，供子进程使用
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        // 从output管道读到子进程处理后的信息，然后send出去
        while (read(cgi_output[0], &c, 1) > 0)
        {
            send(client, &c, 1, 0);
        }

        // 完成操作后关闭管道
        close(cgi_output[0]);
        close(cgi_input[1]);

        // 等待子进程返回
        waitpid(pid, &status, 0);
    }
}

// 主要处理发生在执行 cgi 程序时出现的错误。
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

// 返回给客户端这是个错误请求，HTTP 状态吗 400 BAD REQUEST.
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

// 未找到网页错误
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';

    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    {
        numchars = get_line(client, buf, sizeof(buf));
    }

    resource = fopen(filename, "r");
    if (resource == NULL)
    {
        // printf("resource==NULL\n");
        not_found(client);
    }
    else
    {
        // printf("resource!=NULL\n");
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}
void cat(int client, FILE *resource) // 读取服务器上某个文件写到 socket 套接字。
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
void headers(int client, const char *filename) // 把 HTTP 响应的头部写到套接字。
{
    char buf[1024];
    (void)filename; /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}
int main(void)
{
    int serv_sock, clnt_sock; // 服务器端套接字 客户端套接字
    sockaddr_in clnt_addr;

    int port = 0; // 端口号【自动获取】

    pthread_t newthread;

    serv_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        socklen_t clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (clnt_sock == -1)
        {
            error_die("accept");
        }

        /* accept_request(&clnt_sock);*/
        // 每次收到请求，创建一个线程来处理接受到的请求
        // 把client_sock转成地址作为参数传入pthread_create
        if (pthread_create(&newthread, NULL, (void *)accept_request, (void *)&clnt_sock) != 0)
        {
            perror("pthread_create");
        }
    }
    close(serv_sock);

    return 0;
}