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
    log(INFO, "Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    log(INFO, "Creating socket...\n");
    i32 socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!is_valid_socket(socket_listen)) {
        log(CRITICAL, "socket() failed. (%d)\n", errno);
    }

    log(INFO, "Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        log(CRITICAL, "bind() failed. (%d)\n", errno);
    }
    freeaddrinfo(bind_address);

    log(INFO, "Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        log(CRITICAL, "listen() failed. (%d)\n", errno);
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
        log(CRITICAL, "Out of memory\n");

    n->address_len = sizeof(n->address);
    n->next = g_client_list.head;
    g_client_list.head = n;
    ++g_client_list.size;
    return n;
}
