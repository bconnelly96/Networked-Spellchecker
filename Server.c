#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define DEFAULT_DICT "dict.txt"
#define DEFAULT_PORT 3667
#define FMESSAGE "OK"
#define NFMESSAGE "MISSPELLED"
#define FILE_MAX 32
#define MAX_WORD_SIZE 128
#define MAX_WORKERS 20
#define MAX_REQUESTS 20

char **dict;
int words_in_dict;
int work_queue[MAX_REQUESTS];
sem_t mutex, empty, full;
int work_in_queue;

/*INPUT: takes a string representing a dictionary filename
 *FUNCTION: reads the contents of a dictionary file into an array of strings;
 *OUTPUT: returns an array of strings*/
char **handle_dict(char *dict_name) {
	FILE *fp = fopen(dict_name, "r");
	char c;
	int i;
	
	while (!feof(fp)) {
		if ((c = getc(fp)) == '\n')
			words_in_dict++;
	}
	rewind(fp);
	/*gets individual strings & copies them into the string array*/
	 char **dict = malloc(words_in_dict * sizeof(char *));
	 for (i = 0; i < words_in_dict; i++) {
		dict[i] = malloc(MAX_WORD_SIZE);
		fgets(dict[i], MAX_WORD_SIZE, fp);
	 }
	 fclose(fp);
	 return dict;
}

/*INPUT: takes a void pointer arg
 *FUNCTION: each thread blocks until work is in the queue;
 *when work is in the queue, each thred attempts to acquire the lock;
 *only one acquires it and that thread takes work off of the queue and releases the lock;
 *threads who acquired the lock compare the message sent by a client to a dictionary file
 *and send back to the client the validity of their word
 *OUTPUT: writes a message to the client; returns a pointer to void*/
void *thread_routine(void *arg) {
	int i, value;
	int *id = (int *) arg;
	int clientfd = 0;
	int not_found = 1;
	
	while (1) {
		char *message = malloc(MAX_WORD_SIZE);
		/*wait for work*/
		if (work_in_queue == 0) 
			sem_wait(&empty);
		/*acquire lock before entering c-s*/
		sem_wait(&mutex);
		/*get first piece of work off of queue*/
		for (i = 0; i < MAX_REQUESTS; i++) {
			if (work_queue[i] != 0) {
				clientfd = work_queue[i];
				/*zero out the work queue*/
				work_queue[i] = 0;
				work_in_queue--;
				/*alert main thread that previously full work queue now has an empty slot*/
				if (work_in_queue == (MAX_REQUESTS - 1))
					sem_post(&full);
				break;
			}
		}
		/*release lock*/
		sem_post(&mutex);
		/*read message from the given client*/
		read(clientfd, message, MAX_WORD_SIZE);
		/*determine if the message was spelt correctly; respond to client accordingly*/
		for (i = 0; i < words_in_dict; i++) {
			if (strncmp(message, dict[i], MAX_WORD_SIZE) == 0) {
				write(clientfd, FMESSAGE, MAX_WORD_SIZE);
				not_found = 0;
				break;
			}
		}
		if (not_found) {
			write(clientfd, NFMESSAGE, MAX_WORD_SIZE);
		}
		free(message);
	}
}

/*INPUT: takes optional input of a port number followed by a dictionary filename
 *FUNCTION: loads a dictionary file into memory; creates a thread pool; creates 
 *a socket and accepts connections from clients from the socket; places socket 
 *descriptors on a queue for worker threads to take from; uses semaphores to ensure
 *mutual exclusion
 *OUTPUT: produces no output; returns void*/
void main(int argc, char **argv) {
	int port = 0;
	char *dict_name;
	int listenfd = 0, workfd = 0;
	struct sockaddr_in server;
	int i, value;
	work_in_queue = 0;
	
	/*handles cmd line input*/
	if (argc > 1) {
		port = atoi(argv[1]);
		dict_name = malloc(FILE_MAX);
		strncpy(dict_name, argv[2], FILE_MAX);
	}
	else {
		port = DEFAULT_PORT;
		dict_name = DEFAULT_DICT;
	}
	
	/*load dictionary into memory*/
	dict = handle_dict(dict_name);
	
	/*Creates a socket descriptor; assigns it to listenfd*/	
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		perror("descriptor error: ");
	/*sets the values of the sockaddr_in struct*/
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	/*binds the socket descriptor to the server's address*/
	if (bind(listenfd, (struct sockaddr *)&server, sizeof(server)) == -1)
		perror("binding error: ");
	/*set up fd for listening*/
	listen(listenfd, 128);
	
	/*initialize semaphores*/
	sem_init(&mutex, 0, 1);
	sem_init(&empty, 0, 0);
	sem_init(&full, 0, 0);
	
	/*create worker pool*/
	pthread_t thread_pool[MAX_WORKERS];
	for (i = 0; i < MAX_WORKERS; i++) {
		pthread_create(&thread_pool[i], NULL, &thread_routine, &i);
	}
	/*main thread loop*/
	while (1) {
		workfd = accept(listenfd, NULL, NULL);
		/*make sure that there is room in queue*/
		if (work_in_queue == MAX_REQUESTS) {
			sem_wait(&full);
		}
		/*acquire lock before entering c-s*/
		sem_wait(&mutex);
		/*place client fd in first empty slot in queue*/
		for (i = 0; i < MAX_REQUESTS; i++) {
			if (work_queue[i] == 0) {
				work_queue[i] = workfd;
				work_in_queue++;
				sem_post(&empty);
				break;
			}
		}
		/*release lock*/
		sem_post(&mutex);
	}
}

