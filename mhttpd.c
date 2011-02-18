#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#define SERVERNAME "mhttpd"
#define DEFAULT_WEB_DIR "./default"
#define DEFAULT_WEB_PAGE "index.html"

#define SERVPORT 8000
#define MAXCLIENT 10

#define REQUEST_MAX_SIZE 10240
#define RESPONSE_MAX_SIZE 102400
#define BUFFER_SIZE 8192

struct request
{
	char method[10];
	char path_raw[256];
	char protocol[20];
	char path[256];
	char filename[256];
};

void response(int client_sock_fd, int status, char *title, char *mime_type,
		char *content);
void handle_request(int client_sock_fd, const char *buf);

int init_server()
{
	int serv_sock_fd;
	struct sockaddr_in server_addr;

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
}

void accept_client(int serv_sock_fd)
{
	int pid;
	int client_sock_fd;
	struct sockaddr_in remote_addr;
	socklen_t remote_addr_size;
	char buf[REQUEST_MAX_SIZE];

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
				response(client_sock_fd, 500, "Internal Server Error", NULL,
						NULL);
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
}

int main(int argc, char *argv[])
{
	int serv_sock_fd;

	serv_sock_fd = init_server();
	accept_client(serv_sock_fd);

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

void response(int client_sock_fd, int status, char *title, char *mime_type,
		char *content)
{
	char header_buf[RESPONSE_MAX_SIZE];
	char buf[BUFFER_SIZE];
	char buf_all[RESPONSE_MAX_SIZE];

	if (mime_type == NULL)
	{
		response_header(header_buf, status, title, "text/html");
	}
	else
	{
		response_header(header_buf, status, title, mime_type);
	}
	fprintf(stdout, "%s", header_buf);
	write(client_sock_fd, header_buf, sizeof(header_buf));

	/* Code is ugly, now just let it run. Refactoring next time. */
	if (status == 404)
	{
		memset(buf, 0, sizeof(buf));
		sprintf(buf,
				"<html><head><title>404 File Not Found</title></head>404 File Not Found</html>");
		strcat(buf_all, buf);
		write(client_sock_fd, buf_all, sizeof(buf_all));
		return;
	}
	if (content == NULL)
	{
		return;
	}
	/* why does I delete next two line, then the output is wrong? */
	memset(buf, 0, sizeof(buf));
	sprintf(buf,
			"<html><head><title>404 File Not Found</title></head>404 File Not Found</html>");
	strcat(buf_all, content);

	write(client_sock_fd, buf_all, sizeof(buf_all));
}

int parse_path(char *path_raw, char *path, char *filename)
{
	char *pch;

	pch = strrchr(path_raw, '/');
	if (pch == NULL)
	{
		return -1;
	}
	strcpy(filename, pch + 1);
	strcpy(path, DEFAULT_WEB_DIR);
	strcat(path, path_raw);
	path[pch - path_raw + 1 + strlen(DEFAULT_WEB_DIR)] = '\0';
	if (strcasecmp(path_raw, "/") == 0)
	{
		strcpy(filename, DEFAULT_WEB_PAGE);
	}
}

int parse_request(const char *buf, struct request *request)
{
	char *first_line_request;
	char method[10], path_raw[256], protocol[20];
	char path[256], filename[256];

	assert(request != NULL);

	/* NOTE: Now just get the first line. */
	first_line_request = strtok((char*) buf, "\r\n");
	if (first_line_request == NULL)
	{
		return -1;
	}
	if (sscanf(first_line_request, "%s %s %s", method, path_raw, protocol) != 3)
	{
		return -1;
	}
	fprintf(stdout, "method: %s\n", method);
	fprintf(stdout, "path_raw: %s\n", path_raw);
	fprintf(stdout, "protocol: %s\n", protocol);

	strcpy(request->method, method);
	strcpy(request->path_raw, path_raw);
	strcpy(request->protocol, protocol);

	if (parse_path(request->path_raw, path, filename) == -1)
	{
		return -1;
	}
	strcpy(request->path, path);
	strcpy(request->filename, filename);
	fprintf(stdout, "path: %s\n", path);
	fprintf(stdout, "filename: %s\n", filename);
}

/*
 * Copy from micro_httpd.c.
 * Change the function name and strcmp() to strcasecmp().
 */
char *get_content_mime_type(char *name)
{
	char* dot;

	dot = strrchr(name, '.');
	if (dot == (char*) 0)
		return "text/plain; charset=iso-8859-1";
	if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0)
		return "text/html; charset=iso-8859-1";
	if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcasecmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcasecmp(dot, ".png") == 0)
		return "image/png";
	if (strcasecmp(dot, ".css") == 0)
		return "text/css";
	if (strcasecmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcasecmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcasecmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcasecmp(dot, ".mov") == 0 || strcasecmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcasecmp(dot, ".mpeg") == 0 || strcasecmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcasecmp(dot, ".vrml") == 0 || strcasecmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcasecmp(dot, ".midi") == 0 || strcasecmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcasecmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcasecmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcasecmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";
	return "text/plain; charset=iso-8859-1";
}

int check_file(char *path, char *filename)
{
	struct stat st;
	char file[256];
	strcpy(file, path);
	strcat(file, filename);
	fprintf(stdout, "file: %s\n", file);

	if (stat(file, &st) == -1)
	{
		return -1;
	}
	if (S_ISDIR(st.st_mode))
	{
		return -1;
	}

	return 0;
}

int get_content(char *path, char *filename, char *content)
{
	char file[256];
	FILE *fp;
	long file_size;
	size_t result;

	strcpy(file, path);
	strcat(file, filename);
	fprintf(stdout, "get content file: %s\n", file);
	fp = fopen(file, "r");
	if (fp == NULL)
	{
		fprintf(stdout, "open file error %s.\n", file);
		fclose(fp);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	rewind(fp);
	if (file_size > RESPONSE_MAX_SIZE)
	{
		fprintf(stdout, "file too big error %s.\n", file);
		fclose(fp);
		return -1;
	}
	result = fread(content, 1, file_size, fp);
	if (file_size != result)
	{
		fprintf(stdout, "read file error %s.\n", file);
		fclose(fp);
		return -1;
	}
	fprintf(stdout, "file content: %s\n", content);
	fclose(fp);
	return 1;
}

void handle_request(int client_sock_fd, const char *buf)
{
	struct request request;
	char path[REQUEST_MAX_SIZE];
	char mime_type[80];
	char content[RESPONSE_MAX_SIZE];

	fprintf(stdout, "handle_request\n%s\n", buf);
	if (parse_request(buf, &request) == -1)
	{
		response(client_sock_fd, 400, "Bad Request", NULL, NULL);
		return;
	}
	if (strcasecmp(request.method, "GET") != 0)
	{
		response(client_sock_fd, 501, "Not Implemented", NULL, NULL);
		return;
	}
	if (strcasecmp(request.protocol, "HTTP/1.1") != 0)
	{
		response(client_sock_fd, 501, "Not Implemented", NULL, NULL);
		return;
	}
	if (strcasecmp(request.method, "GET") == 0)
	{
		if (check_file(request.path, request.filename) == -1)
		{
			response(client_sock_fd, 404, "File Not Found", NULL, NULL);
			return;
		}
		strcpy(mime_type, get_content_mime_type(request.filename));
		if (get_content(request.path, request.filename, content) == -1)
		{
			response(client_sock_fd, 403, "Forbidden", NULL, NULL);
			return;
		}
		response(client_sock_fd, 200, "OK", mime_type, content);
		return;
	}
	response(client_sock_fd, 400, "Bad Request", NULL, NULL);
}

