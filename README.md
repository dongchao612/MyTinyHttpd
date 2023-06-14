# MyTinyHttpd
一个简易的 http 服务器

## 目的
1. 复习socket套接字编程
2. 复习http协议及实现
3. 复习进程与线程的创建
4. 熟悉一些基础概念 
5. 了解c++ cgi
6. pipe进程间通信

### HTTP协议:
超文本传输协议（英文：HyperText Transfer Protocol，缩写：HTTP），是应用层的通信协议，通常，由HTTP客户端发起一个请求，创建一个到服务器指定端口（默认是**80**端口）的TCP连接。HTTP服务器则在那个端口监听客户端的请求。一旦收到请求，服务器会向客户端返回一个状态，比如 **"HTTP/1.1 200 0K"**，以及返回的内容，如请求的文件。

### HTTP工作的原理:
1. 客户端连接到Web服务器。
一个HTTP客户端，通常是浏览器，与Web服务器的HTTP端口（默认为80）建立一个TCP套接字连接
2. 发送HTTP请求
通过TCP套接字，客户端向web服务器发送一个文本的请求报文，一个请求报文有:

    ```text
    请求行\r\n
    请求头部\r\n
    \rln
    请求数据\r\n
    ```

3. 服务器接受请求并返回HTTP响应
服务器read套接字，解析请求文本，定位请求资源。服务器将资源write到TCP套接字，由客户端读取。一个响应文本由:
    ```text
    状态行\r\n
    响应头部\r\n\r\n
    响应数据
    ```

4. 客户端浏览器解析HTML内容
客户端read套接字，解析响应文本，查看状态码。处理和显示信息.。
5. 断开TCP连接 (请求头connection=close)
6. HTTP请求格式(请求协议)
   
    ```text
    --------------------------------------------
    |请求方法|空格符|URL|空格符|协议版本|回车符|换行符|
    --------------------------------------------
    --------------------------------------------
    |头部字段名|:|         值        |回车符|换行符|
    --------------------------------------------
                ......................  
    --------------------------------------------
    |头部字段名|:|         值        |回车符|换行符|
    --------------------------------------------
    -----------
    |'\r'|'\n'|
    -----------
    --------------------------------------------
                ......................                      请求数据
    --------------------------------------------
    ```
    例子
    ```text
    POST /htdocs/index.html HTTP/1.1 \r\n
    HOST www.soment.com              \r\n
    Content-Length:9                 \r\n
    \r\n
    color=red
    ```

7. HTTP响应格式(响应协议)

    ```text
    ------------------------------------------------------
    |协议版本|空格符|状态玛|空格符|协议文字描述|回车符|换行符|
    ------------------------------------------------------
    --------------------------------------------
    |头部字段名|:|         值        |回车符|换行符|
    --------------------------------------------
                ......................  
    --------------------------------------------
    |头部字段名|:|         值        |回车符|换行符|
    --------------------------------------------
    -----------
    |'\r'|'\n'|
    -----------
    --------------------------------------------
                ......................                      响应数据
    --------------------------------------------
    ```
    例子
    ```text
    HTTP/1.1 200 OK        \r\n
    Content-Type:text/html \r\n
    Content-Length:362     \r\n
    \r\n
    <html>
    </html>
    ```

8. 状态玛

-  400：客户端请求有语法错误，不能被服务器所理解
-  404：服务器未找到请求的页面
-  500：服务器内部遇到错误，无法完成请求


### 进程 正在被执行的程序

1. 进程id
    ```c
    pid_t id;// 每个进程都有唯一的id标识进程 类型是int
    ```
2. fork()
    ```c
    pid=fork();// 创建子进程父子进程除了进程id不一样，其他都一样
    ```
    pid = 0 子进程
    pid > 0 父进程

3. exec函数族
    ```c
    int execl(const char *path,const char*arg0,...);
    // path 程序的路径 /htdocs/test.cgi
    execl("/htdocs/test.cgi","/htdocs/test.cgi",NULL);
    ```
4. waitpid()函数
    ```c
    pid_t waitpid(pit_t pid,int *stat_loc,int options);// 父进程等待子进程执行结束,清理子进程
    ```

### 线程 程序中一个函数的一次执行
1. 线程id

    ```c
    pthread_t tid;// 进程可以包含多个线程 线程id在特定进程中唯一
    ```
2. pthread_create() //创建线程

    ```c
    int pthread_create(pthread_t* restrict tidp,const pthread_attr_t* restrict_attr,void* (*start_rtn)(void*),void *restrict arg);
    /*
    (1)tidp：事先创建好的pthread_t类型的参数。成功时tidp指向的内存单元被设置为新创建线程的线程ID。
    (2)attr：用于定制各种不同的线程属性。通常直接设为NULL。
    (3)start_rtn：新创建线程从此函数开始运行。无参数是arg设为NULL即可。
    (4)arg：start_rtn函数的参数。无参数时设为NULL即可。有参数时输入参数的地址。当多于一个参数时应当使用结构体传入。
    */

    void* accept_request(void*);
    pthread_t newthread；
    pthread_create(&newthread,NULL,accept_request,(void*)10);
    ```
 3. pthread_self() 
 某个线程查询自己的线程id是什么
 1. 线程退出
    ```c
    void pthread_exit(vois* retval);
    ```
 2. 线程分离

    ```c
    void pthread_detach(vois* retval); //线程结束后不需要清理 不会产生僵尸进程
    ```

### 套接字socket
1. 套接字文件描述符
    ```c
    int sfd;
    ```
2. 创建socket()
    ```c
    int socket( int domain,int type,int protocol);
    /*
    (1)domain：一个地址描述。仅支持AF_INET格式，也就是说ARPA Internet地址格式。
    (2)type：指定socket类型。新套接口的类型描述类型，如TCP（SOCK_STREAM）和UDP(SOCK_DGRAM)。常用的socket类型有，SOCK_STREAM、SOCK_DGRAM、SOCK_RAW、SOCK_PACKET、SOCK_SEQPACKET等等。
    (3)protocol：顾名思义，就是指定协议。套接口所用的协议。如调用者不想指定，可用0。常用的协议有，IPPROTO_TCP、IPPROTO_UDP、IPPROTO_STCP、IPPROTO_TIPC等,它们分别对应TCP传输协议、UDP传输协议、STCP传输协议、TIPC传输协议。
    */
    ```

3. 绑定ip和端口
    ```c
    int bind(int socket,              //socket()创建的socket
             struct sockaddr *address, //
             socklen_t address_len); //sizeof(address)
    ```
    struct sockaddr 与 struct sockaddr_in 两个结构体大小相同

    ```c
    struct sockaddr{
        sa_family_t sin_family;//地址族
        char sa_data[14];//14字节，包含套接字的地址和端口信息
    }
    struct sockaddr_in{
        sa_family_t sin_family;//地址族
        unit16_t sin_port;//16位两字节端口号
        struct in_addr sin_addr;//32位4字节ip地址
        char sin_zero[8];//未使用
    }
    struct in_addr{
        In_addr_t s_addr;//32位4字节ip地址
    }
    // sin_port和sin_addr都必须是为网络字节序 大端排序方式
    // 计算机显示的数字都是主机字节序
    ```

4. int listen(int socket, int backlog);
   使套接字开始监听链接，并制定连接队列的大
5. 处理连接请求

    ```c
    int accept(int socket,
               struct sockaddr *restrict address,//客户端的地址信
               socklen_t *restrict address,address_len);
    // 返回一个新的与客户端建立连接的套接字
    ```

### pipe和CGI
1. pipe
    ```c
    int filedes[2];
    int pipe(int filedes);
    filedes[0];// 读端
    filedes[1];// 写端
    ```
2. dup2
    ```c
    int dup2(int filedes,int filedes2); //复制文件描述符filedes到filedes2
    ```
3. CGI


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

## 执行流程
（1） 服务器启动，在指定端口或随机选取端口绑定 httpd 服务。

（2）收到一个 HTTP 请求时（其实就是 listen 的端口 accpet 的时候），派生一个线程运行 accept_request 函数。

（3）取出 HTTP 请求中的 method (GET 或 POST) 和 url,。对于 GET 方法，如果有携带参数，则 query_string 指针指向 url 中 ？ 后面的 GET 参数。

（4） 格式化 url 到 path 数组，表示浏览器请求的服务器文件路径，在 tinyhttpd 中服务器文件是在 htdocs 文件夹下。当 url 以 / 结尾，或 url 是个目录，则默认在 path 中加上 index.html，表示访问主页。

（5）如果文件路径合法，对于无参数的 GET 请求，直接输出服务器文件到浏览器，即用 HTTP 格式写到套接字上，跳到（10）。其他情况（带参数 GET，POST 方式，url 为可执行文件），则调用 excute_cgi 函数执行 cgi 脚本。

（6）读取整个 HTTP 请求并丢弃，如果是 POST 则找出 Content-Length. 把 HTTP 200  状态码写到套接字。

（7） 建立两个管道，cgi_input 和 cgi_output, 并 fork 一个进程。

（8） 在子进程中，把 STDOUT 重定向到 cgi_outputt 的写入端，把 STDIN 重定向到 cgi_input 的读取端，关闭 cgi_input 的写入端 和 cgi_output 的读取端，设置 request_method 的环境变量，GET 的话设置 query_string 的环境变量，POST 的话设置 content_length 的环境变量，这些环境变量都是为了给 cgi 脚本调用，接着用 execl 运行 cgi 程序。

（9） 在父进程中，关闭 cgi_input 的读取端 和 cgi_output 的写入端，如果 POST 的话，把 POST 数据写入 cgi_input，已被重定向到 STDIN，读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT。接着关闭所有管道，等待子进程结束。

（10） 关闭与浏览器的连接，完成了一次 HTTP 请求与回应，因为 HTTP 是无连接的。

![图解](https://img-blog.csdnimg.cn/20210909201533131.png?)


