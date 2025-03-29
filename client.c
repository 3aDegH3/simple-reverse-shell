#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define END_MARKER "ــENDــ"
#define PORT 8080 // Fixed port for local execution

// Function to handle and print error messages
void error(const char *msg) {
  perror(msg);
  exit(1);
}

// Function to execute a command and send output back through socket
void execute_command(int sock, const char *command) {
  // Open a pipe to execute the command
  FILE *fp = popen(command, "r");
  if (!fp) {
    send(sock, "Command failed\n", 15, 0);
    return;
  }

  char buffer[BUFFER_SIZE];
  // Read command output line by line
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
      perror("send failed");
      break;
    }
  }

  pclose(fp);
  // Send end marker to indicate completion
  send(sock, END_MARKER, strlen(END_MARKER), 0);
}

int main() {
  int sock;
  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE];

  // Create socket
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    error("ERROR opening socket");

  // Configure server address (always localhost)
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Connect to server
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR connecting");

  printf("[+] Connected to server on port %d.\n", PORT);

  // Command receiving loop
  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0)
      error("ERROR receiving command");
    if (bytes_received == 0)
      break; // Server disconnected

    buffer[bytes_received] = '\0';
    execute_command(sock, buffer);
  }

  close(sock);
  printf("[+] Connection closed.\n");
  return 0;
}