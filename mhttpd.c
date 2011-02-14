#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#define SERVERNAME "mhttpd"

#define SERVPORT 8000
#define MAXCLIENT 10
#define REQUEST_MAX_SIZE 10240
#define RESPONSE_MAX_SIZE 102400
#define BUFFER_SIZE 8192

struct request
{
	char method[10];
	char path[256];
	char protocol[20];
};

void response(int client_sock_fd, int status, char *title);
void handle_request(int client_sock_fd, const char *buf);

int main(int argc, char *argv[])
{
	int pid;
	int serv_sock_fd;
	int client_sock_fd;
	struct sockaddr_in server_addr;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_size;
	char buf[REQUEST_MAX_SIZE];

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

	fprintf(stdout, "Start Server listening port %d.\n", SERVPORT);
	fprintf(stdout, "Waiting client connection ...\n");

	while (1)
	{
		remote_addr_size = sizeof(struct sockaddr_in);
		if ((client_sock_fd = accept(serv_sock_fd,
				(struct sockaddr*) &remote_addr, &remote_addr_size)) == -1)
		{
			perror("accept socket error.");
			continue;
		}
		fprintf(stdout, "received a connection from %s.\n", inet_ntoa(
				remote_addr.sin_addr));

		pid = fork();
		if (pid == -1)
		{
			perror("fork error.");
			close(client_sock_fd);
			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			if (read(client_sock_fd, buf, sizeof(buf)) == -1)
			{
				response(client_sock_fd, 500, "Internal Server Error");
				perror("read error.");
				exit(EXIT_FAILURE);
			}
			/* child process to handle url request. */
			handle_request(client_sock_fd, buf);
			exit(0);
		}

		wait(NULL);
		close(client_sock_fd);
		fprintf(stdout, "request finished.\n");
	}

	close(serv_sock_fd);

	return EXIT_SUCCESS;
}

void response_header(char *header_buf, int status, char *title, char *mine_type)
{
	time_t now;
	char time_buf[100];
	char buf[BUFFER_SIZE];

	memset(buf, 0, sizeof(header_buf));

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "HTTP/1.0 %d %s\r\n", status, title);
	strcat(header_buf, buf);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "Server: %s\r\n", SERVERNAME);
	strcat(header_buf, buf);

	memset(buf, 0, sizeof(buf));
	now = time((time_t *) 0);
	strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(
			&now));
	sprintf(buf, "Date: %s\r\n", time_buf);
	strcat(header_buf, buf);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "Connection: close\r\n");
	strcat(header_buf, buf);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "Content-Type: %s\r\n", mine_type);
	strcat(header_buf, buf);

	strcat(header_buf, "\r\n");
}

void response(int client_sock_fd, int status, char *title)
{
	char header_buf[RESPONSE_MAX_SIZE];
	char buf[BUFFER_SIZE];
	char buf_all[RESPONSE_MAX_SIZE];
	response_header(header_buf, status, title, "text/html");
	fprintf(stdout, "%s", header_buf);
	write(client_sock_fd, header_buf, sizeof(header_buf));

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "<html><head><title>mhttpd</title></head>mhttpd works!</html>");
	strcat(buf_all, buf);

	write(client_sock_fd, buf_all, sizeof(buf_all));
}

int parse_request(const char *buf, struct request *request)
{
	char *first_line_request;
	char method[10], path[256], protocol[20];

	assert(request != NULL);

	/* NOTE: Now just get the first line. */
	first_line_request = strtok((char*) buf, "\r\n");
	if (first_line_request == NULL)
	{
		return -1;
	}
	if (sscanf(first_line_request, "%s %s %s", method, path, protocol) != 3)
	{
		return -1;
	}
	fprintf(stdout, "method: %s\n", method);
	fprintf(stdout, "path: %s\n", path);
	fprintf(stdout, "protocol: %s\n", protocol);

	strcpy(request->method, method);
	strcpy(request->path, path);
	strcpy(request->protocol, protocol);
}

void handle_request(int client_sock_fd, const char *buf)
{
	struct request request;

	fprintf(stdout, "handle_request\n%s\n", buf);
	if (parse_request(buf, &request) == -1)
	{
		response(client_sock_fd, 400, "Bad Request");
		return;
	}
	if (strcasecmp(request.method, "GET") != 0)
	{
		response(client_sock_fd, 501, "Not Implemented");
		return;
	}
	if (strcasecmp(request.protocol, "HTTP/1.1") != 0)
	{
		response(client_sock_fd, 501, "Not Implemented");
		return;
	}
	if (strcasecmp(request.method, "GET") == 0)
	{
		response(client_sock_fd, 200, "OK");
		return;
	}
	response(client_sock_fd, 400, "Bad Request");
}

