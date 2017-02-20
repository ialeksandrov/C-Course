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
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>

#define PORT 3456
#define BACKLOG 10
#define DATA_SIZE 100
#define DAEMON_NAME "serverd"

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
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t sin_size;
	int yes = 1;
	struct sigaction sa;
	char addr_str[INET6_ADDRSTRLEN];
	int ret;
	/* set log mask and open sysog */
	setlogmask(LOG_UPTO(LOG_NOTICE));
	openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

	syslog(LOG_NOTICE, "Daemonizing...");
	daemonize();

	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket()");
		exit(1);
	}

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		exit(1);
	}

	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  	server_addr.sin_port = htons((unsigned short)PORT);

	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		close(listen_fd);
		perror("bind()");
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

	syslog(LOG_NOTICE, "listening on port %d...\n", PORT);

	while(1) {
		sin_size = sizeof(client_addr);
		client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(client_addr.sin_family, &(client_addr.sin_addr), addr_str, sizeof(addr_str));
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

