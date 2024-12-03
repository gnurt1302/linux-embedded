#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>     //  Chứa cấu trúc cần thiết cho socket. 
#include <netinet/in.h>     //  Thư viện chứa các hằng số, cấu trúc khi sử dụng địa chỉ trên 
#include <string.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>

#define FIFO_FILE   		"./logFifo"
#define BUFF_SIZE   		1024
#define LISTEN_BACKLOG 		10

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int sequence_number = 0;
char log_event[BUFF_SIZE];
int flag;

pthread_mutex_t fifo_mutex 			= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shared_data_mutex 	= PTHREAD_MUTEX_INITIALIZER;

struct sharedData
{

};

void log_process(void);
void thread_manager(void);
void* connection_manager(void* arg);
void* data_manager(void* arg);
void* storage_manager(void* arg);
void get_timestamp(char *buffer, size_t buffer_size);

int socket_init(int *port);


void main(int argc, char *argv[])
{	
	// Create FIFO
	mkfifo(FIFO_FILE, 0666);
	
	pid_t log = fork();
	if (log == 0) {
		/* Log process */ 
		log_process();
	} else if (log > 0) {
		/* Main process */

		if (argc < 2) {
			printf("No port provided\n");
			printf("Command: ./main <port number>\n");
			exit(EXIT_FAILURE);
		}

		
		pthread_t connection_thread, data_thread, storage_thread;

		pthread_create(&connection_thread, NULL, connection_manager, (void *)argv);
		pthread_create(&data_thread, NULL, data_manager, NULL);
		pthread_create(&storage_thread, NULL, storage_manager, NULL);

		pthread_join(connection_thread, NULL);
		pthread_join(data_thread, NULL);
		pthread_join(storage_thread, NULL);

	} else {
		// Error
		printf("Fork failed.\n");
	}
	
	unlink(FIFO_FILE);
}

void log_process(void)
{
	int log_fd, fifo_fd;
	char buffer[256];
	char timestamp[20];

	fifo_fd = open(FIFO_FILE, O_RDONLY);
	log_fd = open("gateway.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
	
	int bytes = read(fifo_fd, buffer, sizeof(buffer));
	if (bytes > 0)
	{
		get_timestamp(timestamp, sizeof(timestamp));

		char log_message[512];
		snprintf(log_message, sizeof(log_message), "%u %s %s\n", sequence_number++, timestamp, buffer);

		write(log_fd, log_message, strlen(log_message));
	}
	

	close(fifo_fd);
	close(log_fd);

}

void* connection_manager(void* arg)
{
	int server_fd, new_socket_fd, fifo_fd;
	int addr_len;
	struct sockaddr_in client_addr;
	
	/* TCP socket connection */
	char **argbuff = (char **) arg;
	int port = atoi(argbuff[1]);

	server_fd = socket_init(&port);
	addr_len = sizeof(client_addr);
	
	while(1)
	{
		printf("Server is listening at port: %d \n....\n",port);
		new_socket_fd  = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t *)&addr_len);
		if (new_socket_fd < 0)
            handle_error("accept()");
		printf("Server: got connection \n");
		while (1)
		{
			char recvbuff_from_socket[BUFF_SIZE];
			int numb_read = read(new_socket_fd, recvbuff_from_socket, BUFF_SIZE);
			if(numb_read == -1)
				handle_error("read()");
			printf("Message from sensor node: %s\n", recvbuff_from_socket);
		}
	}
	close(server_fd);

	/* Write data to shared data */
	// pthread_mutex_lock(&shared_data_mutex);
	
	// /* ... */
	
	// pthread_mutex_unlock(&shared_data_mutex);
	
	
	/* Write log events to FIFO */
	// pthread_mutex_lock(&fifo_mutex);
	// int fifo_fd = open(FIFO_FILE, O_WRONLY);
	
	// /* ... */
	
	// write(fifo_fd, log_event, BUFF_SIZE);
	// close(fifo_fd);
	// pthread_mutex_unlock(&fifo_mutex);

	return NULL;

}

void* data_manager(void* arg)
{
	return NULL;
}

void* storage_manager(void* arg)
{
	return NULL;
}

void get_timestamp(char *buffer, size_t buffer_size) 
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int socket_init(int *port)
{
	struct sockaddr_in server_addr;
	int server_fd;
	int opt = 1;

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
	
	/* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
        handle_error("socket()");
	
	/* Prevent error: “address already in use” */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        handle_error("setsockopt()");
	
	/* Init server address */
	server_addr.sin_family		= AF_INET;
    server_addr.sin_port		= htons(*port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	/* Bind socket to address */
	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		handle_error("bind()");
		
	/* Listen to incoming connections */
	if (listen(server_fd, LISTEN_BACKLOG) < 0)
        handle_error("listen()");
	
	return(server_fd);
}
