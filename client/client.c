#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>     //  Chứa cấu trúc cần thiết cho socket. 
#include <netinet/in.h>     //  Thư viện chứa các hằng số, cấu trúc khi sử dụng địa chỉ trên internet
#include <arpa/inet.h>
#include <unistd.h>

#define BUFF_SIZE 1024
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)
		
int main(int argc, char *argv[])
{
    int port;
    int server_fd;
    struct sockaddr_in server_addr;
	memset(&server_addr, '0',sizeof(server_addr));
	
    /* Đọc portnumber từ command line */
    if (argc < 3) {
        printf("command : ./client <server address> <port number>\n");
        exit(1);
    }
    port = atoi(argv[2]);
	
    /* Init server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
	
	/* convert IPv4 addresses from text to binary form */
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) == -1) 
        handle_error("inet_pton()");
	
    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        handle_error("socket()");
	
    /* Connect to server */
    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        handle_error("connect()");
	
	
	while(1)
	{
		int numb_write;
		char sendbuff_to_socket[BUFF_SIZE];
		
		memset(sendbuff_to_socket, '0', BUFF_SIZE);
		
		printf("Please enter the message: ");
        fgets(sendbuff_to_socket, BUFF_SIZE, stdin);
		
		numb_write = write(server_fd, sendbuff_to_socket, sizeof(sendbuff_to_socket));
        if (numb_write == -1)     
            handle_error("write()");
	}

    return 0;
}

