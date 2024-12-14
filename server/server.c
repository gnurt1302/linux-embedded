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
#include <sys/epoll.h>

#define FIFO_FILE   		"./logFifo"
#define BUFF_SIZE   		1024
#define LISTEN_BACKLOG 		10
#define MAX_EVENTS 			10
#define INIT_VALUE_CAPA		100


#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int count = 1;
int sequence_number = 0;
char log_event[BUFF_SIZE];
int flag;
int new_data_flag = 0;

pthread_mutex_t fifo_mutex 			= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shared_data_mutex 	= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t 	new_data_cond 		= PTHREAD_COND_INITIALIZER;

typedef struct node
{
	int nodeID;
	int fd;
	float *value;
	int value_count;
	int value_capa;
	struct node *next;
} sensor_node_t;

sensor_node_t *head;
int count_nodeID = 1;

void add_node(int fd);
void remove_node(int fd);
void store_sensor_data(int fd, char *msg);
void fetch_sensor_data();

void log_process(void);
void thread_manager(void);
void *connection_manager(void *arg);
void *data_manager(void *arg);
void *storage_manager(void *arg);

void get_timestamp(char *buffer, size_t buffer_size);
void write_to_fifo(char *log_event);
void socket_init(int *port);


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

void add_node(int fd)
{
	sensor_node_t *current_node = head;
	
	while(current_node != NULL){
		if (current_node->fd == fd){
			return;
		}
	}
	
	sensor_node_t *new_sensor_node = (sensor_node_t*)malloc(sizeof(sensor_node_t));
	
	new_sensor_node->fd 			= fd;
	new_sensor_node->nodeID 		= count_nodeID++;
	new_sensor_node->value_capa 	= INIT_VALUE_CAPA;
    new_sensor_node->value_count 	= 0;
    new_sensor_node->value 			= (float*)malloc(new_sensor_node->value_capa * sizeof(float));
	new_sensor_node->next 			= NULL;
	
	head = new_sensor_node;
	
	printf("A sensor node with sensorNodeID: %d has opened a new connection.\n", new_sensor_node->nodeID);
}

void remove_node(int fd)
{
	sensor_node_t *prev_node = NULL;
	sensor_node_t *current_node = head;
	while(current_node != NULL){
		if (current_node->fd == fd){
			if (prev_node == NULL) {
                head = current_node->next;
            } else {
                prev_node->next = current_node->next;
            }
			printf("The sensor node with sensorNodeID:%d has closed the connection\n", current_node->nodeID);
			free(current_node);
			return;
		}
		prev_node = current_node;
		current_node = current_node->next;
	}
}

void store_sensor_data(int fd, char *msg)
{	
	float value = atof(msg);
	sensor_node_t *current_node = head;
	
	while(current_node != NULL){
		if (current_node->fd == fd){
			if (current_node->value_count == current_node->value_capa) {
				current_node->value_capa *= 2;
				current_node->value = (float*)realloc(current_node->value, current_node->value_capa * sizeof(float));
			}
			current_node->value[current_node->value_count++] = value;
			new_data_flag = 1;
			pthread_cond_signal(&new_data_cond);
			printf("Node ID %d: Received value %.2f\n", current_node->nodeID, value);
			return;
		}
		current_node = current_node->next;
	}
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

void *connection_manager(void *arg)
{
	char **argbuff = (char **) arg;
	int port = atoi(argbuff[1]);

	int server_fd, client_fd, epoll_fd, new_socket_fd_fd;
	int new_socket_fd;
	int n_events;
	int opt = 1;
	char buffer[BUFF_SIZE], log_event[BUFF_SIZE];

	struct sockaddr_in server_addr, client_addr;
	struct epoll_event ev, events[MAX_EVENTS];

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
	int addr_len = sizeof(client_addr);

	// Create socket 
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
        handle_error("socket()");
	
	// Prevent error: “address already in use” 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        handle_error("setsockopt()");
	
	// Init server address 
	server_addr.sin_family		= AF_INET;
    server_addr.sin_port		= htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Bind socket to address 
	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		handle_error("bind()");
		
	// Listen to incoming connections 
	if (listen(server_fd, LISTEN_BACKLOG) < 0)
        handle_error("listen()");

	printf("Server listening on port %d ...\n", port);

	// Create epoll instance 
	if ((epoll_fd = epoll_create1(0)) < 0) 
        handle_error("epoll_create1()");
	
	// Add the server socket to the epoll instance 
	ev.events = EPOLLIN;
    ev.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) 
        handle_error("epoll_ctl: server_fd");
    
	// Main loop for connection management
	while(1)
	{
		// Wait for events on the epoll instance
		n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n_events < 0){
			handle_error("epoll_wait");
			continue;
		}
            
		// Process each event
		for (int i = 0; i < n_events; i++) {
			if (events[i].data.fd == server_fd){
				// Accept new connection
				new_socket_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (new_socket_fd < 0) 
                    handle_error("accept()");

				// Add new client to epoll
				ev.events 	= EPOLLIN;
                ev.data.fd 	= new_socket_fd;

				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket_fd, &ev) < 0) 
                    handle_error("epoll_ctl: new_socket_fd");
				
				// Add to sensor node list
				printf("New connection: socket fd %d, IP %s, port %d\n",
                       new_socket_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				pthread_mutex_lock(&shared_data_mutex);
				add_node(new_socket_fd);
				pthread_mutex_unlock(&shared_data_mutex);
			} else {
				// Handle data from existing client
				client_fd = events[i].data.fd;
                int byte_read = read(client_fd, buffer, BUFF_SIZE);

                if (byte_read > 0) {
					// Process received data
                    // Null-terminate the buffer and process the message
                    buffer[byte_read] = '\0';
                    printf("Message from client (fd %d): %s", client_fd, buffer);
					pthread_mutex_lock(&shared_data_mutex);
					store_sensor_data(client_fd, buffer);
					pthread_mutex_unlock(&shared_data_mutex);
                } else {
					// Client disconnected or error
                    printf("Client disconnected: socket fd %d\n", client_fd);
					// Remove client from linked list
					pthread_mutex_lock(&shared_data_mutex);
					remove_node(client_fd);
					pthread_mutex_unlock(&shared_data_mutex);
					// Remove client from epoll and close socket
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                }
			}
		}
	}
	close(server_fd);
    close(epoll_fd);

	return NULL;

}

void *data_manager(void *arg)
{
	while(1)
	{
		pthread_mutex_lock(&shared_data_mutex);
		while (!new_data_flag)
        {
            pthread_cond_wait(&new_data_cond, &shared_data_mutex);
        }
		sensor_node_t *current_node = head;
		float sum_value, avg_value;
		
		while(current_node != NULL){
			for (int i = 0; i < current_node->value_count; i++) {
				printf("NodeID %d - Value %d: %.2f\n", current_node->nodeID, i + 1, current_node->value[i]);
				sum_value += current_node->value[i];
			}
			avg_value = sum_value / current_node->value_count;
			if (avg_value >= 20)
				printf("The sensor node with sensorNodeID: %d reports it’s too hot (avg temperature = %.2f)\n", current_node->nodeID, avg_value);
			else if (avg_value < 20)
				printf("The sensor node with sensorNodeID: %d reports it’s too cold (avg temperature = %.2f\n)", current_node->nodeID, avg_value);
			else 
				printf("Received sensor data with invalid sensor node ID: %d\n",current_node->nodeID);

			sum_value = 0;
			current_node = current_node->next;
		}
		new_data_flag = 0;
		pthread_mutex_unlock(&shared_data_mutex);
		sleep(5);
	}
	
	return NULL;
}


void *storage_manager(void *arg)
{
	return NULL;
}

void get_timestamp(char *buffer, size_t buffer_size) 
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

void socket_init(int *port)
{
	int server_fd, client_fd, epoll_fd, new_socket_fd_fd;
	int new_socket_fd;
	int n_events;
	int opt = 1;
	char buffer[BUFF_SIZE], log_event[BUFF_SIZE];

	struct sockaddr_in server_addr, client_addr;
	struct epoll_event ev, events[MAX_EVENTS];

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
	int addr_len = sizeof(client_addr);

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

	printf("Server listening on port %d ...\n", *port);

	if ((epoll_fd = epoll_create1(0)) < 0) 
        handle_error("epoll_create1()");
	
	/* Add the server socket to the epoll instance */
	ev.events = EPOLLIN;
    ev.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) 
        handle_error("epoll_ctl: server_fd");
    
	while(1)
	{
		// Wait for events on the epoll instance
		n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n_events < 0)
            handle_error("epoll_wait");
		
		// Process each event
		for (int i = 0; i < n_events; i++) {
			if (events[i].data.fd == server_fd){
				new_socket_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (new_socket_fd < 0) 
                    handle_error("accept()");
				
                printf("New connection: socket fd %d, IP %s, port %d\n",
                       new_socket_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				add_node(new_socket_fd);

				ev.events = EPOLLIN;
                ev.data.fd = new_socket_fd;
				
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket_fd, &ev) < 0) 
                    handle_error("epoll_ctl: new_socket_fd");

			} else {
				client_fd = events[i].data.fd;
                int byte_read = read(client_fd, buffer, BUFF_SIZE);
                if (byte_read > 0) {
                    // Null-terminate the buffer and process the message
                    buffer[byte_read] = '\0';
                    printf("Message from client (fd %d): %s", client_fd, buffer);
					store_sensor_data(client_fd, buffer);
					//fetch_sensor_data();
                } else {
					// Client disconnected or error
                    printf("Client disconnected: socket fd %d\n", client_fd);
					remove_node(client_fd);
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                }
			}
		}
	}
	close(server_fd);
    close(epoll_fd);

}

void write_to_fifo(char *log_event)
{
	int fifo_fd;
	
	pthread_mutex_lock(&fifo_mutex);
	
	fifo_fd = open(FIFO_FILE, O_WRONLY);
	write(fifo_fd, (void*)log_event, BUFF_SIZE);
	
	close(fifo_fd);
	pthread_mutex_unlock(&fifo_mutex);
}



