#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVPORT 8000
#define MAXCLIENT 10

int main(int argc, char *argv[])
{
	int pid;
	int serv_sock_fd;
	int client_sock_fd;
	struct sockaddr_in server_addr;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_size;

	if ((serv_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		perror("socket create failed.");
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(SERVPORT);

	if (bind(serv_sock_fd, (struct sockaddr*) &server_addr,
			sizeof(struct sockaddr_in)) == -1)
	{
		perror("bind socket error.");
		exit(EXIT_FAILURE);
	}

	if (listen(serv_sock_fd, MAXCLIENT) == -1)
	{
		perror("listen socket error.");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		remote_addr_size = sizeof(struct sockaddr_in);
		if ((client_sock_fd = accept(serv_sock_fd,
				(struct sockaddr*) &remote_addr, &remote_addr_size)) == -1)
		{
			perror("accept socket error.");
			continue;
		}
		printf("received a connection from.\n");
		pid = fork();
		if (pid == -1)
		{
			perror("fork error.");
			close(client_sock_fd);
			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			/* child process to handle url request. */
			handle_request();
		}

		wait(NULL);
		close(client_sock_fd);
	}

	close(serv_sock_fd);

	return EXIT_SUCCESS;
}

void handle_request()
{

}
