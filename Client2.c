#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFF 64
#define DEFAULT_PORT 3667
#define MAX_THREADS 50

void *thread_routine(void *arg) {
	char *word = (char *) arg;
	int client_fd;
	struct sockaddr_in server;
	int i;
	
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	server.sin_family = AF_INET;
	server.sin_port = htons(DEFAULT_PORT);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	connect(client_fd, (struct sockaddr *) &server, sizeof(server));
	
	char *response = malloc(BUFF);
		
	write(client_fd, word, BUFF);
	read(client_fd, response, BUFF);
	printf("The word %s was %s\n", word, response);
	free(response);
}

void  main(int argc, char **argv) {
	FILE *fp = fopen("words.txt", "r");
	int i, j;
	char **words = malloc(MAX_THREADS * sizeof(char *));
	
	pthread_t threads[MAX_THREADS];
	
	for (i = 0; i < MAX_THREADS; i++) {
		words[i] = malloc(BUFF);
		fgets(words[i], BUFF, fp);
	}
	fclose(fp);
	for (i = 0; i < MAX_THREADS; i++) {
		pthread_create(&threads[i], NULL, &thread_routine, words[i]);
	}
		
	for (i = 0; i < MAX_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
}
