#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define INITIAL_BUFFER_SIZE 4096 // Initial buffer size
#define PORT 8080                // Server port
#define END_MARKER "##END##"     // Data end marker

// Error handling function
void error(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

// Function to dynamically receive data from socket
char *receive_dynamic_data(int sock) {
  size_t buffer_size = INITIAL_BUFFER_SIZE;
  char *buffer = malloc(buffer_size);
  if (!buffer)
    error("Memory allocation failed");

  size_t total_received = 0;
  ssize_t bytes_received;
  char *end_marker_pos = NULL;

  while (1) {
    // If we don't have enough space, expand the buffer
    if (total_received >= buffer_size - 1) {
      buffer_size *= 2;
      char *new_buffer = realloc(buffer, buffer_size);
      if (!new_buffer) {
        free(buffer);
        error("Memory reallocation failed");
      }
      buffer = new_buffer;
    }

    // Receive data from socket
    bytes_received = recv(sock, buffer + total_received,
                          buffer_size - total_received - 1, 0);

    if (bytes_received < 0) {
      perror("recv failed");
      free(buffer);
      return NULL;
    }
    if (bytes_received == 0) {
      printf("[!] Client disconnected\n");
      free(buffer);
      return NULL;
    }

    total_received += bytes_received;
    buffer[total_received] = '\0'; // Ensure string termination

    // Check for end marker
    end_marker_pos = strstr(buffer, END_MARKER);
    if (end_marker_pos) {
      *end_marker_pos = '\0'; // Remove end marker
      break;
    }
  }

  // Shrink buffer to actual data size
  char *final_buffer = realloc(buffer, strlen(buffer) + 1);
  return final_buffer ? final_buffer : buffer;
}

int main() {
  int server_fd, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  // Create server socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("ERROR opening socket");

  // Configure server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind socket to address
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    error("ERROR on binding");

  // Start listening for incoming connections
  if (listen(server_fd, 1) < 0)
    error("ERROR on listen");

  printf("[+] Server started on port %d\n", PORT);

  // Accept client connection
  if ((client_sock =
           accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0)
    error("ERROR on accept");

  // Display connected client's IP address
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  printf("[+] Client connected: %s\n", client_ip);
  printf("[!] Type 'exit' to quit\n");

  while (1) {
    printf("%s $ ", client_ip);
    fflush(stdout);

    char command[INITIAL_BUFFER_SIZE];
    if (!fgets(command, sizeof(command), stdin)) {
      printf("\n[!] Input error\n");
      break;
    }

    // Remove newline character
    command[strcspn(command, "\n")] = '\0';

    // Exit if command is 'exit'
    if (strcmp(command, "exit") == 0) {
      send(client_sock, command, strlen(command), 0);
      break;
    }

    // Send command to client
    if (send(client_sock, command, strlen(command), 0) < 0) {
      perror("send failed");
      break;
    }

    // Receive response from client dynamically
    printf("[Output]\n");
    char *response = receive_dynamic_data(client_sock);
    if (response) {
      printf("%s\n", response);
      free(response);
    } else {
      break;
    }
  }

  // Close sockets
  close(client_sock);
  close(server_fd);
  printf("[+] Server stopped\n");
  return EXIT_SUCCESS;
}