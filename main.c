#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

#define FIFO_FILE   "./logFifo"
#define BUFF_SIZE   1024

int fifo_fd;
int sequence_number = 0;
char log_event[BUFF_SIZE];
int flag;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct sharedData
{

};

void log_process(void);
void thread_manager(void);
void* connection_manager(void* arg);
void* data_manager(void* arg);
void* storage_manager(void* arg);
void get_timestamp(char *buffer, size_t buffer_size);


void main(void)
{	
	// Create FIFO
	mkfifo(FIFO_FILE, 0666);
	
	pid_t log = fork();
	while(1)
	{
		if (log == 0) {
			// Log process
			log_process();
		} else if (log > 0) {
			// Main process
			scanf("%d", &flag);
			thread_manager();
		} else {
        	// Error
			printf("Fork failed.\n");
   		}
	}
	unlink(FIFO_FILE);
}

void log_process(void)
{
	int log_fd;
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

void thread_manager(void)
{
	// Create thread: connection thread, data thread, storage thread
	pthread_t connection_thread, data_thread, storage_thread;
	
	pthread_create(&connection_thread, NULL, connection_manager, NULL);
	pthread_create(&data_thread, NULL, data_manager, NULL);
	pthread_create(&storage_thread, NULL, storage_manager, NULL);

	pthread_join(connection_thread, NULL);
	pthread_join(data_thread, NULL);
    pthread_join(storage_thread, NULL);

}

void* connection_manager(void* arg)
{
	int sensorNodeID;

	pthread_mutex_lock(&mutex);
	fifo_fd = open(FIFO_FILE, O_WRONLY);

	switch (flag) {
        case 1:
            snprintf(log_event, sizeof(log_event), "A sensor node with <sensorNodeID> has opened a new connection.\n");
			printf("A sensor node with <sensorNodeID> has opened a new connection.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
        case 2:
            snprintf(log_event, sizeof(log_event), "The sensor node with <sensorNodeID> has closed the connection.\n");
			printf("The sensor node with <sensorNodeID> has closed the connection.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
    }

	close(fifo_fd);
	pthread_mutex_unlock(&mutex);

	return NULL;
}

void* data_manager(void* arg)
{

	pthread_mutex_lock(&mutex);
	fifo_fd = open(FIFO_FILE, O_WRONLY);

	switch (flag) {
        case 3:
            snprintf(log_event, sizeof(log_event), "The sensor node with <sensorNodeID> reports it’s too cold (running avg temperature = <value>).\n");
            printf("The sensor node with <sensorNodeID> reports it’s too cold (running avg temperature = <value>).\n");
			write(fifo_fd, log_event, BUFF_SIZE);
			break;
        case 4:
            snprintf(log_event, sizeof(log_event), "The sensor node with <sensorNodeID> reports it’s too hot (running avg temperature = <value>).\n");
			printf("The sensor node with <sensorNodeID> reports it’s too hot (running avg temperature = <value>).\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
		case 5:
            snprintf(log_event, sizeof(log_event), "Received sensor data with invalid sensor node ID <node-ID>.\n");
			printf("Received sensor data with invalid sensor node ID <node-ID>.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
    }


	close(fifo_fd);
	pthread_mutex_unlock(&mutex);

	return NULL;
}

void* storage_manager(void* arg)
{

	pthread_mutex_lock(&mutex);
	fifo_fd = open(FIFO_FILE, O_WRONLY);

	switch (flag) {
        case 6:
            snprintf(log_event, sizeof(log_event), "Connection to SQL server established.\n");
			printf("Connection to SQL server established.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
        case 7:
            snprintf(log_event, sizeof(log_event), "New table <name-of-table> created.\n");
			printf("New table <name-of-table> created.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
		case 8:
            snprintf(log_event, sizeof(log_event), "Connection to SQL server lost.\n");
			printf("Connection to SQL server lost.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
		case 9:
            snprintf(log_event, sizeof(log_event), "Unable to connect to SQL server.\n");
			printf("Unable to connect to SQL server.\n");
			write(fifo_fd, log_event, BUFF_SIZE);
            break;
    }
	
	close(fifo_fd);
	pthread_mutex_unlock(&mutex);

	return NULL;
}

void get_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}