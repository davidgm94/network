#define MAXINPUT 512
#define MAXRESPONSE 1024
void get_input(const char* prompt, char* buffer)
{
	printf("%s", prompt);
	buffer[0] = 0;
	fgets(buffer, MAXINPUT, stdin);
	const int read = strlen(buffer);
	if (read > 0)
		buffer[read - 1] = 0;
}

void send_format(i32 server, const char* text, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, text);
	vsprintf(buffer, text, args);
	va_end(args);
	
	send(server, buffer, strlen(buffer), 0);

	printf("C: %s", buffer);
}

int parse_response(const char* response)
{
	const char* k = response;
	if (!k[0] || !k[1] || !k[2]) return 0;

	for(; k[3]; ++k) {
		if (k == response || k[-1] == '\n') {
			if (isdigit(k[0]) && isdigit(k[1]) && isdigit(k[2])) {
				if (k[3] != '-') {
					if (strstr(k, "\r\n")) {
						return strtol(k, 0, 10);
					}
				}
			}
		}
	}
	return 0;
}

void wait_on_response(i32 server, int expecting)
{
	char response[MAXRESPONSE + 1];
	char* p = response;
	char* end = response + MAXRESPONSE;

	int code = 0;

	do {
		int bytes_received = recv(server, p, end - p, 0);
		if (bytes_received < 1) {
			logger(CRITICAL, "Connection dropped\n");
		}
		p += bytes_received;

		*p = 0;

		if (p == end) {
			logger(CRITICAL, "Server response too large:\n%s\n", response);
		}

		code = parse_response(response);

	} while (code == 0);

	if (code != expecting) {
		logger(CRITICAL, "Error from server:\n%s\n", response);
	}

	logger(INFO, "\n%s\n", response);
}

i32 connect_to_host_email(const char* hostname, const char* port)
{
	logger(INFO, "Configuring remote address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* peer_address;
	if (getaddrinfo(hostname, port, &hints, &peer_address)) {
		logger(CRITICAL, "getaddrinfo() failed. (%d)\n", errno);
	}

	logger(INFO, "Remote address is: ");
	char address_buffer[100];
	char service_buffer[100];
	getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
			address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer),
			NI_NUMERICHOST);
	logger(INFO, "%s %s\n", address_buffer, service_buffer);

	logger(INFO, "Creating socket...\n");
	i32 server = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
	if (!is_valid_socket(server)) {
		logger(CRITICAL, "socket() failed. (%d)\n", errno);
	}

	logger(INFO, "Connecting...\n");
	if (connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
		logger(CRITICAL, "connect() failed. (%d)\n", errno);
	}
	freeaddrinfo(peer_address);

	logger(INFO, "Connected.\n.\n");

	return server;
}

void email()
{
	char hostname[MAXINPUT];
	get_input("mail server: ", hostname);

	logger(INFO, "Connecting to host: %s:25\n", hostname);

	i32 server = connect_to_host_email(hostname, "25");

	wait_on_response(server, 220);

	send_format(server, "HELO HONPWC\r\n");

	wait_on_response(server, 250);

	char sender[MAXINPUT];
	get_input("from: ", sender);
	send_format(server, "MAIL FROM:<%s>\r\n", sender);
	wait_on_response(server, 250);

	char recipient[MAXINPUT];
	get_input("to: ", recipient);
	send_format(server, "RCPT TO:<%s>\r\n", recipient);
	wait_on_response(server, 250);

	send_format(server, "DATA\r\n");
	wait_on_response(server, 354);

	char subject[MAXINPUT];
	get_input("subject: ", subject);

	send_format(server, "From:<%s>\r\n", sender);
	send_format(server, "To:<%s>\r\n", recipient);
	send_format(server, "Subject:<%s>\r\n", subject);
	
	time_t timer;
	time(&timer);

	struct tm* timeinfo;
	timeinfo = gmtime(&timer);
	char date[128];

	strftime(date, 128, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);

	send_format(server, "Date:%s\r\n", date);

	send_format(server, "\r\n");

	logger(INFO, "Enter you email text, end with \".\" on a line by itself.\n");

	while (true) {
		char body[MAXINPUT];
		get_input("> ", body);
		send_format(server, "%s\r\n", body);
		if (strcmp(body, ".") == 0) {
			break;
		}
	}

	wait_on_response(server, 250);
	send_format(server, "QUIT\r\n");
	wait_on_response(server, 221);

	logger(INFO, "Closing socket...\n");
	close(server);
	logger(INFO, "Finished...\n");
}
