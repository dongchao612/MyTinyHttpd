/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */

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
// #define ISspace(x) isspace((int)(x))
#define ISspace(x) (x == ' ')

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

// 没有发现文件 404 错误
void not_found(int);

// 接读取文件返回给请求的http客户端
void serve_file(int client, const char *filename);

// 返回http头
void headers(int, const char *);

// 读取文件
void cat(int, FILE *);

// 执行cgi文件
void execute_cgi(int, const char *, const char *, const char *);

// 错误请求 400错误
void bad_request(int client);

// 无法执行cgi文件 500错误
void cannot_execute(int client);

// 如果不是Get或者Post，就报方法没有实现
void unimplemented(int);
/**************************************************/

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/

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

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
// 把错误信息写到 perror 并退出。
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/

// 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
int startup(int *port)
{
    int serv_sock = 0;
    int on = 0;
    struct sockaddr_in serv_addr; // 定义在<netinet/in.h>中，是一个结构体，用于存储IPv4地址信息

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        error_die("socket");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // ipv4
    serv_addr.sin_port = htons(*port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) == -1)
    {
        error_die("setsockopt failed");
    }

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
    if (listen(serv_sock, 5) == -1) // 开始监听
    {
        error_die("listen");
    }

    return serv_sock; // 返回服务器套接字
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
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

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/

/*
处理从套接字上监听到的一个 HTTP 请求
在这里可以很大一部分地体现服务器处理请求流程。
*/
void accept_request(void *arg)
{
    int clnt_sock = *(int *)arg;

    char buf[1024];

    //"GET / HTTP/1.1\n"
    // 读http 请求的第一行数据（request line），把请求方法存进 method 中
    int numchars = get_line(clnt_sock, buf, sizeof(buf));

    int i = 0, j = 0;
    char method[255];
    // 第一行字符串提取Get
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1)) // 根据空格定位请求方法
    {
        method[i] = buf[i];
        i++;
    }
    j = i;
    method[i] = '\0';

    // 如果请求的方法不是 GET 或 POST 任意一个的话就直接发送 response 告诉客户端没实现该方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(clnt_sock);
        return;
    }

    int cgi = 0;
    // 如果是POST，cgi置为1
    if (strcasecmp(method, "POST") == 0)
    {
        cgi = 1;
    }

    i = 0;
    // 跳过所有的空白字符(空格)
    while (ISspace(buf[j]) && (j < numchars))
    {
        j++;
    }
    char url[255];
    //  //然后把 URL 读出来放到 url 数组中 得到 "/"
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    /*注意：如果你的http的网址为http://192.168.0.23:47310/index.html
    那么你得到的第一条http信息为GET /index.html HTTP/1.1，那么
    解析得到的就是/index.html */

    char *query_string = NULL;
    // 如果这个请求是一个 GET 方法的话,用一个指针指向 url
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        // 去遍历这个 url，跳过字符 ？前面的所有字符，如果遍历完毕也没找到字符 ？则退出循环
        while ((*query_string != '?') && (*query_string != '\0'))
        {
            query_string++;
        }

        // 退出循环后检查当前的字符是 ？还是字符串(url)的结尾
        if (*query_string == '?')
        {
            cgi = 1;              // 如果是 ？ 的话，证明这个请求需要调用 cgi，将 cgi 标志变量置一(true)
            *query_string = '\0'; // 从字符 ？ 处把字符串 url 给分隔会两份
            query_string++;       // 使指针指向字符 ？后面的那个字符
        }
    }

    char path[512];
    // 将前面分隔两份的前面那份字符串，拼接在字符串htdocs的后面之后就输出存储到数组 path 中。
    //  相当于现在 path 中存储着一个字符串
    sprintf(path, "htdocs%s", url);

    // 默认地址，解析到的路径如果为/，则自动加上index.html
    if (path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }

    struct stat st;
    // 获得文件信息 在系统上去查询该文件是否存在
    if (stat(path, &st) == -1)
    {
        // 如果不存在，那把这次 http 的请求后续的内容(head 和 body)全部读完并忽略
        while ((numchars > 0) && strcmp("\n", buf))
        {
            numchars = get_line(clnt_sock, buf, sizeof(buf));
        }

        // 然后返回一个找不到文件的 response 给客户端
        not_found(clnt_sock);
    }
    else
    {
        if ((st.st_mode & __S_IFMT) == __S_IFDIR)
        {
            // 如果这个文件是个目录，那就需要再在 path 后面拼接一个"/index.html"的字符串
            strcat(path, "/index.html");
        }
        // 如果你的文件默认是有执行权限的，自动解析成cgi程序，
        // 如果有执行权限但是不能执行，会接受到报错信号
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            // 如果这个文件是一个可执行文件，不论是属于用户/组/其他这三者类型的，就将 cgi 标志变量置一
            cgi = 1;
        }
        if (!cgi)
        {
            // 如果不需要 cgi 机制的话，
            //  接读取文件返回给请求的http客户端
            serve_file(clnt_sock, path);
        }
        else
        {
            // 执行cgi文件  如果需要则调用
            execute_cgi(clnt_sock, path, method, query_string);
        }
    }
    close(clnt_sock);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
// 运行 cgi 程序的处理，也是个主要函数。
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    // 缓冲区
    char buf[1024];

    // 进程pid和状态
    pid_t pid;
    int status;

    int i;

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
    send(client, buf, strlen(buf), 0); // 服务器响应信息

    // 2根管道
    int cgi_output[2];
    int cgi_input[2];

    // 建立output管道 子进程写管道
    if (pipe(cgi_output) < 0)
    {
        cannot_execute(client);
        return;
    }

    // 建立input管道 子进程读管道
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
    if ((pid = fork()) < 0) // 子线程创建失败
    {
        cannot_execute(client);
        return;
    }
    if (pid == 0) /* child: CGI script */
    {

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
        char meth_env[255];
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env); // 修改环境变量

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

        char c;

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

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
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

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
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

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
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

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
// 接读取文件返回给请求的http客户端
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
        not_found(client);
    }
    else
    {

        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
// 读取文件
void cat(int client, FILE *resource) // 读取服务器上某个文件写到 socket 套接字。
{
    char buf[1024];

    // 从文件文件描述符中读取指定内容
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
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
    int serv_sock = -1, clnt_sock = -1; // 服务器端套接字 客户端套接字
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