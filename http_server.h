#pragma once

static const char *get_content_type(const char* path) {
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }
    return "application/octet-stream";
}

static i32 create_server_socket(const char* host, const char* port) {
    logger(INFO, "Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    logger(INFO, "Creating socket...\n");
    i32 socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!is_valid_socket(socket_listen)) {
        logger(CRITICAL, "socket() failed. (%d)\n", errno);
    }

    logger(INFO, "Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        logger(CRITICAL, "bind() failed. (%d)\n", errno);
    }
    freeaddrinfo(bind_address);

    logger(INFO, "Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        logger(CRITICAL, "listen() failed. (%d)\n", errno);
    }

    return socket_listen;
}

#define MAX_REQUEST_SIZE 2047
typedef struct client_info {
    socklen_t address_len;
    struct sockaddr_storage address;
    i32 socket;
    char request[MAX_REQUEST_SIZE + 1];
    i32 received;
    struct client_info* next;
} ClientInfo;

struct {
    ClientInfo* head;
    u32 size;
} g_client_list;

ClientInfo* get_client(i32 socket) {
    ClientInfo* client_info = g_client_list.head;
    while (client_info) {
        if (client_info->socket == socket) break;
        client_info = client_info->next;
    }

    if (client_info) return client_info;
    ClientInfo* n = (ClientInfo*) calloc(1, sizeof(ClientInfo));

    if (!n)
        logger(CRITICAL, "Out of memory\n");

    n->address_len = sizeof(n->address);
    n->next = g_client_list.head;
    g_client_list.head = n;
    ++g_client_list.size;
    return n;
}

void drop_client(ClientInfo* client) {
	close(client->socket);
	
	ClientInfo** p = &g_client_list.head;

	while (*p) {
		if (*p == client) {
			*p = client->next;
			free(client);
			return;
		}
		p = &(*p)->next;
	}

	logger(CRITICAL, "drop_client() failed. Client not found\n");
	exit(1);
}

const char* get_client_address(ClientInfo* client) {
	static char address_buffer[100];
	getnameinfo((struct sockaddr*)&client->address, client->address_len,
			address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
	return address_buffer;
}

fd_set wait_on_clients(i32 server) {
	fd_set reads;
	FD_ZERO(&reads);
	FD_SET(server, &reads);
	i32 max_socket = server;

	ClientInfo* client = g_client_list.head;

	while (client) {
		FD_SET(client->socket, &reads);
		if (client->socket > max_socket)
			max_socket = client->socket;
		client = client->next;
	}

	if (select(max_socket + 1, &reads, 0, 0, 0) < 0)
		logger(CRITICAL, "select() failed. (%d)\n", errno);

	return reads;
}

void send_400(ClientInfo* client) {
	char c400[] = "HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"Content-Length: 11\r\n\r\nBad request";
	send(client->socket, c400, COUNTOF(c400), 0);
	drop_client(client);
}

void send_404(ClientInfo* client) {
	const char* c404 = "HTTP/1.1 404 Not Found\r\n"
		"Connection: close\r\n"
		"Content-Length: 9\r\n\r\nNot Found";
	send(client->socket, c404, COUNTOF(c404), 0);
	drop_client(client);
}

void serve_resource(ClientInfo* client, const char* path) {
	printf("serve_resource %s %s\n", get_client_address(client), path);
	if (strcmp(path, "/") == 0)
		path = "/index.html";
	if (strlen(path) > 100) {
		send_400(client);
		return;
	}

	if (strstr(path, "..")) {
		send_404(client);
		return;
	}

	char full_path[128];
	sprintf(full_path, "public%s", path);

	FILE* file = fopen(full_path, "rb");
	if (!file) {
		send_404(client);
		return;
	}

	fseek(file, 0L, SEEK_END);
	size_t cl = ftell(file);
	rewind(file);

	const char* ct = get_content_type(full_path);
#define BUFFER_SIZE 1024
	char buffer[BUFFER_SIZE];

	sprintf(buffer, "HTTP/1.1 200 OK\r\n");
	send(client->socket, buffer, strlen(buffer), 0);

	sprintf(buffer, "Connection: close\r\n");
	send(client->socket, buffer, strlen(buffer), 0);

	sprintf(buffer, "Content-Length: %u\r\n", cl);
	send(client->socket, buffer, strlen(buffer), 0);

	sprintf(buffer, "Content-Type: %s\r\n", ct);
	send(client->socket, buffer, strlen(buffer), 0);

	sprintf(buffer, "\r\n");
	send(client->socket, buffer, strlen(buffer), 0);

	int r = fread(buffer, 1, BUFFER_SIZE, file);
	while (r) {
		send(client->socket, buffer, r, 0);
		r = fread(buffer, 1, BUFFER_SIZE, file);
	}

	fclose(file);
	drop_client(client);
}

void tcp_http_client(void) {
	i32 server = create_server_socket("127.0.0.1", "8080");
	while (true) {
		fd_set reads = wait_on_clients(server);
		if (FD_ISSET(server, &reads)) {
			ClientInfo* client = get_client(-1);
			client->socket = accept(server, (struct sockaddr*) &(client->address),
					&(client->address_len));
			if (!is_valid_socket(client->socket)) {
				logger(CRITICAL, "accept() failed. (%d)\n", errno);
			}
			
			logger(INFO, "New connection from %s.\n", get_client_address(client));
		}

		ClientInfo* client = g_client_list.head;
		while (client) {
			ClientInfo* next = client->next;
			if (FD_ISSET(client->socket, &reads)) {
				if (MAX_REQUEST_SIZE == client->received) {
					send_400(client);
					continue;
				}

				int r = recv(client->socket, client->request + client->received, MAX_REQUEST_SIZE - client->received, 0);

				if (r < 1) {
					logger(ERROR, "Unexpected disconnect from %s.\n", get_client_address(client));
					drop_client(client);
				} else {
					client->received += r;
					client->request[client->received] = 0;

					char* q = strstr(client->request, "\r\n\r\n");
					if (q) {
						if (strncmp("GET /", client->request, 5)) {
							send_400(client);
						} else {
							char* path = client->request + 4;
							char* end_path = strstr(path, " ");
							if (!end_path) {
								send_400(client);
							} else {
								*end_path = 0;
								serve_resource(client, path);
							}
						}
					}
				}
			}

			client = next;
		}
	}

	logger(INFO, "Closing socket...\n");
	close(server);
	logger(INFO, "Finished\n");
}
