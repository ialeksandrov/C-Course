#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>

#define PORT "3456"
#define BACKLOG 10
#define DATA_SIZE 100
#define DAEMON_NAME "server6d"

void daemonize() {
	pid_t pid, sid;

	/* create clild */
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* forked, parent should exit */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* change file mask */
	umask(0);

	/* create a new sid for the child */
	sid = setsid();
	if (sid < 0)
		exit(EXIT_FAILURE);

	/* make working directory "/" */
	if ((chdir("/")) < 0)
		exit(EXIT_FAILURE);

	/* close standard file descriptors */
	close(0);
	close(1);
	close(2);
}

/* handle SIGCHLD */
void signal_handler(int sig) {
	if (sig == SIGCHLD) { 
		int saved_errno = errno;
		while(waitpid(-1, NULL, WNOHANG) > 0);
		errno = saved_errno;
	}
}


/* works for IPv4 and IPv6 */
void *get_address(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	} else if (sa->sa_family == AF_INET6) {
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
	} else {
		return NULL;
	}
}


/* remove new line characters */
void remove_nl(char *str) {
	char *c;

	c = strchr(str, '\n');
	if (c) *c = 0;
	c = strchr(str, '\r');
	if (c) *c = 0; 
}


/* handle client connections */
void handle_client(int client_fd) {
    int bytes;
	char command[DATA_SIZE] = {0};

	while (1) {
		if ((bytes = recv(client_fd, command, DATA_SIZE, 0)) == -1) {
			perror("recv");
		}

		if (bytes == 0) {
			shutdown(client_fd, SHUT_RDWR);
			close(client_fd);
			syslog(LOG_NOTICE, "remote closed connection on fd = %d\n", client_fd);
			_exit(0);
		}

		command[bytes] = 0;
		remove_nl(command);

		if (!strcmp(command, "hello")) {
			if (send(client_fd, "Hello man!\n", 11, 0) == -1) {
				perror("send");
			}
		} else if (!strcmp(command, "quit")) {
			if (send(client_fd, "Good bye!\n", 10, 0) == -1) {
				perror("send");
			}
			break;
		} else {
			if (send(client_fd, "Wrong command!\n", 15, 0) == -1) {
				perror("send");
			}
		}
	}
	
	shutdown(client_fd, SHUT_RDWR);
	close(client_fd);
	syslog(LOG_NOTICE, "closed connection on fd = %d\n", client_fd);
	_exit(0);
}


int main(void) {
	int listen_fd, client_fd;
	struct addrinfo hints, *server_info, *sip;
	struct sockaddr_storage client_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char addr_str[INET6_ADDRSTRLEN];
	int ret;
	
	/* set log mask and open sysog */
	setlogmask(LOG_UPTO(LOG_NOTICE));
	openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

	syslog(LOG_NOTICE, "Daemonizing...");
	daemonize();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((ret = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;
	}

	// bind to the first we can
	for(sip = server_info; sip != NULL; sip = sip->ai_next) {
		if ((listen_fd = socket(sip->ai_family, sip->ai_socktype, sip->ai_protocol)) == -1) {
			perror("socket()");
			continue;
		}

		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt()");
			exit(1);
		}

		if (bind(listen_fd, sip->ai_addr, sip->ai_addrlen) == -1) {
			close(listen_fd);
			perror("bind()");
			continue;
		}

		break;
	}

	freeaddrinfo(server_info);

	if (sip == NULL)  {
		fprintf(stderr, "bind failed!\n");
		exit(1);
	}

	if (listen(listen_fd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	syslog(LOG_NOTICE, "listening on port %s...\n", PORT);

	while(1) {
		sin_size = sizeof(client_addr);
		client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(client_addr.ss_family, get_address((struct sockaddr *)&client_addr), addr_str, sizeof(addr_str));
		syslog(LOG_NOTICE, "new connection from %s (fd = %d)\n", addr_str, client_fd);

		if (!fork()) {
			close(listen_fd);
			handle_client(client_fd);
		}
		
		close(client_fd);
	}
	closelog();
	return 0;
}

