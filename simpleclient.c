#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct sockaddr_in sockaddr_in;

int main(int argc, char *argv[])
{
    int clnt_sock;
    int serv_adr_sz;
    sockaddr_in serv_adrr;
    int result;
    char ch = 'A';

    clnt_sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_adrr.sin_family = AF_INET;
    serv_adrr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adrr.sin_port = htons(atoi(argv[2]));

    serv_adr_sz = sizeof(serv_adrr);
    result = connect(clnt_sock, (struct sockaddr *)&serv_adrr, serv_adr_sz);

    if (result == -1)
    {
        perror("oops: client1");
        exit(1);
    }

    write(clnt_sock, &ch, 1);
    read(clnt_sock, &ch, 1);
    printf("char from server = %c\n", ch);

    close(clnt_sock);
    
    exit(0);
}
