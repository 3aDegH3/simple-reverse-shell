#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define END_MARKER "##END##"
#define PORT 8080

void error(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int main() {
  int server_fd, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  char buffer[BUFFER_SIZE];

  // Create socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("ERROR opening socket");

  // Configure server
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind and listen
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    error("ERROR on binding");

  if (listen(server_fd, 1) < 0)
    error("ERROR on listen");

  printf("[+] Server started on port %d\n", PORT);

  // Accept client
  if ((client_sock =
           accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0)
    error("ERROR on accept");

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  printf("[+] Client connected: %s\n", client_ip);

  // Main loop
  while (1) {
    printf("%s $ ", client_ip);
    fflush(stdout);

    char command[BUFFER_SIZE];
    if (!fgets(command, BUFFER_SIZE, stdin)) {
      printf("\n[!] Input error\n");
      break;
    }

    command[strcspn(command, "\n")] = '\0';

    if (strcmp(command, "exit") == 0) {
      send(client_sock, command, strlen(command), 0);
      break;
    }

    // Send command
    if (send(client_sock, command, strlen(command), 0) < 0) {
      perror("send failed");
      break;
    }

    // Receive output
    printf("[Output]\n");
    char response[BUFFER_SIZE * 10] = {
        0}; // حافظه‌ای بزرگتر برای پاسخ
    size_t total_received = 0;

    while (1) {
      memset(buffer, 0, BUFFER_SIZE);
      ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);

      if (bytes_received < 0) {
        perror("recv failed");
        break;
      }
      if (bytes_received == 0) {
        printf("[!] Client disconnected\n");
        goto cleanup;
      }

      buffer[bytes_received] = '\0';
      strcat(response, buffer);
      total_received += bytes_received;

      // اگر `END_MARKER` در انتها باشد، پایان را مشخص کن
      if (strstr(buffer, END_MARKER)) {
        char *end = strstr(response, END_MARKER);
        if (end)
          *end = '\0';
        break;
      }
    }

    printf("%s\n", response);
  }

cleanup:
  close(client_sock);
  close(server_fd);
  printf("[+] Server stopped\n");
  return EXIT_SUCCESS;
}
