#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>

#define PORT 5000
#define BUFSIZE 1024
#define DEMON_NAME "serverd"
#define MAX_CLIENTS 50


static unsigned int cli_count = 0;
static int uid = 10;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* Client structure */
typedef struct {
	struct sockaddr_in addr;	/* Client remote address */
	int connfd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	char name[32];			/* Client name */
} client_t;

client_t *clients[MAX_CLIENTS];

/* Add client to queue */
void queue_add(client_t *cl){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			return;
		}
	}
}

/* Delete client from queue */
void queue_delete(int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				return;
			}
		}
	}
}

/* Send message to all clients */
void send_message_all(char *s){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			write(clients[i]->connfd, s, strlen(s));
		}
	}
}

/* Send message to sender */
void send_message_self(const char *s, int connfd){
	write(connfd, s, strlen(s));
}

/* Send message to client */
void send_message_client(char *s, char *name){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(strcmp(clients[i]->name, name) == 0){
				write(clients[i]->connfd, s, strlen(s));
			}
		}
	}
}

/* Send list of active clients */
void send_active_clients(int connfd){
	int i;
	char s[64];
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			sprintf(s, "[CLIENT] with uid: %d | nickname: [%s]\r\n", clients[i]->uid, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
}

/* Strip CRLF */
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
	printf("%d.%d.%d.%d",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[1024];
	char buff_in[1024];
	int rlen;
  pthread_mutex_init(&lock, NULL);

	cli_count++;
	client_t *cli = (client_t *)arg;

	printf("Client connection  ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
  sprintf(buff_out, "New client joined: Welcome %s\r\n", cli->name);
	send_message_all(buff_out);

	/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0){
	        buff_in[rlen] = '\0';
	        buff_out[0] = '\0';
		strip_newline(buff_in);
		time_t ltime;
    struct tm *Tm;

    ltime=time(NULL);
    Tm=localtime(&ltime);
		/* Ignore empty buffer */
		if(!strlen(buff_in)){
			continue;
		}

		/* Special options */
		if(buff_in[0] == '.'){
			char *command, *param;
			command = strtok(buff_in," ");
			if(!strcmp(command, ".quit")){
				break;
			}else if(!strcmp(command, ".name")){
				param = strtok(NULL, " ");
				if(param){
					char *old_name = strdup(cli->name);
					strcpy(cli->name, param);
					sprintf(buff_out, "[%d:%d:%d] Changed his name from, [%s] to [%s]\r\n",Tm->tm_hour, Tm->tm_min, Tm->tm_sec, old_name, cli->name);
					free(old_name);
					pthread_mutex_lock(&lock);
					send_message_all(buff_out);
					pthread_mutex_unlock(&lock);
				}else{
					send_message_self("[ERROR] NAME CANNOT BE NULL\r\n", cli->connfd);
				}
			}else if(!strcmp(command, ".msg")){
				param = strtok(NULL, " ");
				if(param){
					char *name = param;
					param = strtok(NULL, " ");
					if(param){
						sprintf(buff_out, "[%d:%d:%d][Private Message] from: [%s]", Tm->tm_hour, Tm->tm_min, Tm->tm_sec, cli->name);
						while(param != NULL){
							strcat(buff_out, " ");
							strcat(buff_out, param);
							param = strtok(NULL, " ");
						}
						strcat(buff_out, "\r\n");
						pthread_mutex_lock(&lock);
						send_message_client(buff_out, name);
						pthread_mutex_unlock(&lock);
					}else{
						send_message_self("[ERROR] MESSAGE CANNOT BE NULL\r\n", cli->connfd);
					}
				}else{
					send_message_self("[ERROR] REFERENCE CANNOT BE NULL\r\n", cli->connfd);
				}
			}else if(!strcmp(command, ".msg_all")){
					param = strtok(NULL, " ");
					if(param){
						sprintf(buff_out, "[%d:%d:%d][Message to all]:", Tm->tm_hour, Tm->tm_min, Tm->tm_sec);
						while(param != NULL){
							strcat(buff_out, " ");
							strcat(buff_out, param);
							param = strtok(NULL, " ");
						}
					strcat(buff_out, "\r\n");
					pthread_mutex_lock(&lock);
					send_message_all(buff_out);
					pthread_mutex_unlock(&lock);
			}
			}else if(!strcmp(command, ".list")){
				sprintf(buff_out, "[%d:%d:%d] List of clients: %d\r\n", Tm->tm_hour, Tm->tm_min, Tm->tm_sec, cli_count);
				pthread_mutex_lock(&lock);
				send_message_self(buff_out, cli->connfd);
				pthread_mutex_unlock(&lock);
				pthread_mutex_lock(&lock);
				send_active_clients(cli->connfd);
        pthread_mutex_unlock(&lock);
			}else if(!strcmp(command, ".help")){
				strcat(buff_out, ".quit     Quit chatroom\r\n");
				strcat(buff_out, ".name     <name> Change nickname\r\n");
				strcat(buff_out, ".msg      <nickname> <message> Send private message\r\n");
				strcat(buff_out, ".msg_all  <message> Send message to all");
				strcat(buff_out, ".list     Show active clients\r\n");
				strcat(buff_out, ".help     Show help\r\n");
				pthread_mutex_lock(&lock);
				send_message_self(buff_out, cli->connfd);
				pthread_mutex_unlock(&lock);
			}else{
				send_message_self("[ERROR] UNKOWN COMMAND\r\n", cli->connfd);
			}
		}
	}

	/* Close connection */
	close(cli->connfd);
	sprintf(buff_out, "USER: [%s] left the chat. Bye!\r\n", cli->name);
	pthread_mutex_lock(&lock);
	send_message_all(buff_out);
  pthread_mutex_unlock(&lock);
	/* Delete client from queue and yeild thread */
	queue_delete(cli->uid);
	printf("Closed connection with ip: ");
	print_client_addr(cli->addr);
	printf(" with uid %d\n", cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char *argv[]){
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

	/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

	/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return 1;
	}

	printf("Server listening on port: %d\n", PORT);

	/* Accept clients */
	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS){
			printf("MAX CLIENTS REACHED\n");
			printf("Connection Rejected ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		sprintf(cli->name, "%d", cli->uid);

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
}
