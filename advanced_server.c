/*
 * advanced_server.c
 *
 * Advanced multi-user server using a thread pool.
 *
 * Features:
 * - Accepts multiple clients simultaneously.
 * - Manages a task queue for incoming connections.
 * - Lists connected clients.
 * - Allows admin to switch and send commands to a specific client.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080            // Port number the server will listen on
#define MAX_CLIENTS 100      // Maximum number of clients that can connect
#define BUFFER_SIZE 4096     // Buffer size for receiving messages
#define THREAD_POOL_SIZE 4   // Number of worker threads in the pool
#define END_MARKER "##END##" // End Mesage

// Structure to store client information
typedef struct {
  int sock;                // Socket for the client connection
  struct sockaddr_in addr; // Client's address
  int id;                  // Unique client ID
} client_t;

// Structure for task queue
typedef struct task {
  int client_sock;                // Socket of the client
  struct sockaddr_in client_addr; // Client's address
  struct task *next;              // Pointer to the next task in the queue
} task_t;

// Global variables for task queue
static task_t *task_queue_head = NULL; // Head of the task queue
static task_t *task_queue_tail = NULL; // Tail of the task queue
static pthread_mutex_t task_mutex =
    PTHREAD_MUTEX_INITIALIZER; // Mutex for task queue
static pthread_cond_t task_cond =
    PTHREAD_COND_INITIALIZER; // Condition variable for task queue

// Client list and related mutex
static client_t *clients[MAX_CLIENTS]; // Array of connected clients
static int client_count = 0;           // Number of currently connected clients
static pthread_mutex_t clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;   // Mutex for clients array
static int global_client_id = 1; // Global client ID counter

// Add a new client to the global array
void add_client(client_t *cl) {
  pthread_mutex_lock(&clients_mutex);
  if (client_count < MAX_CLIENTS) {
    clients[client_count++] = cl;
  } else {
    fprintf(stderr, "Maximum number of clients reached.\n");
    free(cl);
  }
  pthread_mutex_unlock(&clients_mutex);
}

// Remove a client from the global array
void remove_client(int id) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < client_count; i++) {
    if (clients[i]->id == id) {
      close(clients[i]->sock); // Close the client's socket
      free(clients[i]);        // Free the client object
      for (int j = i; j < client_count - 1; j++) {
        clients[j] = clients[j + 1]; // Shift the clients in the array
      }
      client_count--;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

// List all connected clients
void list_clients() {
  pthread_mutex_lock(&clients_mutex);
  printf("Connected clients:\n");
  for (int i = 0; i < client_count; i++) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clients[i]->addr.sin_addr), client_ip,
              INET_ADDRSTRLEN);
    printf("ID: %d - IP: %s - Socket: %d\n", clients[i]->id, client_ip,
           clients[i]->sock);
  }
  pthread_mutex_unlock(&clients_mutex);
}

// Handle client communication

void handle_client(client_t *cl) {
  char buffer[BUFFER_SIZE];
  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_received = recv(cl->sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
      printf("Client ID %d disconnected\n", cl->id);
      break;
    }
    buffer[bytes_received] = '\0';

    // حذف END_MARKER قبل از نمایش
    char *end_pos = strstr(buffer, END_MARKER);
    if (end_pos)
      *end_pos = '\0';

    printf("[Client ID %d]: %s\n", cl->id, buffer);
  }
  remove_client(cl->id);
}

// Worker thread function
void *worker_thread(void *arg) {
  (void)arg;
  while (1) {
    task_t *task = NULL;
    pthread_mutex_lock(&task_mutex);
    while (task_queue_head == NULL) {
      pthread_cond_wait(&task_cond, &task_mutex); // Wait for new tasks
    }
    task = task_queue_head;
    task_queue_head = task->next;
    if (task_queue_head == NULL) {
      task_queue_tail = NULL;
    }
    pthread_mutex_unlock(&task_mutex);

    // Create client object and assign the task's socket
    client_t *cl = malloc(sizeof(client_t));
    if (!cl) {
      perror("malloc");
      free(task);
      continue;
    }
    cl->sock = task->client_sock;
    cl->addr = task->client_addr;
    cl->id = global_client_id++;
    free(task);

    add_client(cl); // Add the client to the list of connected clients
    printf("New client connected with ID %d\n", cl->id);
    handle_client(cl); // Handle communication with the client
  }
  return NULL;
}

// Add a task to the queue
void enqueue_task(int client_sock, struct sockaddr_in client_addr) {
  task_t *new_task = malloc(sizeof(task_t));
  if (!new_task) {
    perror("malloc");
    return;
  }
  new_task->client_sock = client_sock;
  new_task->client_addr = client_addr;
  new_task->next = NULL;

  pthread_mutex_lock(&task_mutex);
  if (task_queue_tail == NULL) {
    task_queue_head = task_queue_tail = new_task; // Add task to empty queue
  } else {
    task_queue_tail->next = new_task; // Append task to the end of the queue
    task_queue_tail = new_task;
  }
  pthread_cond_signal(
      &task_cond); // Signal worker threads that a new task is available
  pthread_mutex_unlock(&task_mutex);
}

// Admin console for managing clients
void *admin_console(void *arg) {
  (void)arg;
  char cmd[BUFFER_SIZE];
  int current_client_id = -1;

  while (1) {
    if (current_client_id == -1) {
      printf("admin> ");
    } else {
      printf("client-%d> ", current_client_id);
    }
    fflush(stdout);

    if (!fgets(cmd, BUFFER_SIZE, stdin))
      continue;
    cmd[strcspn(cmd, "\n")] = '\0';

    if (strcmp(cmd, "list") == 0) {
      list_clients();
      continue;
    } else if (strcmp(cmd, "exit") == 0) {
      exit(0);
    }

    if (current_client_id == -1) {
      if (strncmp(cmd, "switch ", 7) == 0) {
        int target_id;
        if (sscanf(cmd + 7, "%d", &target_id) != 1) {
          printf("Usage: switch <id>\n");
          continue;
        }

        pthread_mutex_lock(&clients_mutex);
        client_t *target = NULL;
        for (int i = 0; i < client_count; i++) {
          if (clients[i]->id == target_id) {
            target = clients[i];
            break;
          }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!target) {
          printf("Client ID %d not found\n", target_id);
          continue;
        }

        current_client_id = target_id;
        printf("[+] Switched to client %d. Commands:\n", target_id);
        printf("  'back' - Return to admin mode\n");
        printf("  'switch <id>' - Switch to another client\n");
      } else {
        printf("Invalid command. Available: list, switch <id>, exit\n");
      }
    }

    else {
      if (strcmp(cmd, "back") == 0) {
        current_client_id = -1;
      } else if (strncmp(cmd, "switch ", 7) == 0) {
        int target_id;
        if (sscanf(cmd + 7, "%d", &target_id) != 1) {
          printf("Usage: switch <id>\n");
          continue;
        }

        if (target_id == current_client_id) {
          printf("Already connected to client %d\n", target_id);
          continue;
        }

        pthread_mutex_lock(&clients_mutex);
        client_t *target = NULL;
        for (int i = 0; i < client_count; i++) {
          if (clients[i]->id == target_id) {
            target = clients[i];
            break;
          }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!target) {
          printf("Client ID %d not found\n", target_id);
        } else {
          current_client_id = target_id;
          printf("[+] Switched to client %d\n", target_id);
        }
      } else {
        pthread_mutex_lock(&clients_mutex);
        client_t *target = NULL;
        for (int i = 0; i < client_count; i++) {
          if (clients[i]->id == current_client_id) {
            target = clients[i];
            break;
          }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!target) {
          printf("Client ID %d disconnected\n", current_client_id);
          current_client_id = -1;
        } else {

          char full_cmd[BUFFER_SIZE];
          snprintf(full_cmd, BUFFER_SIZE, "%s\n", cmd);
          if (send(target->sock, full_cmd, strlen(full_cmd), 0) < 0) {
            perror("send failed");
            current_client_id = -1;
          }
        }
      }
    }
  }
  return NULL;
}

// Main function to start the server
int main() {
  int server_sock;
  struct sockaddr_in server_addr;

  // Create server socket
  if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation failed");
    return -1;
  }

  // Setup server address structure
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind the socket to the port
  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Bind failed");
    return -1;
  }

  // Listen for incoming connections
  if (listen(server_sock, MAX_CLIENTS) < 0) {
    perror("Listen failed");
    return -1;
  }

  printf("[+] Server started on port %d\n", PORT);

  // Create worker threads to handle incoming tasks
  pthread_t workers[THREAD_POOL_SIZE];
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    if (pthread_create(&workers[i], NULL, worker_thread, NULL) != 0) {
      perror("Thread creation failed");
      return -1;
    }
  }

  // Admin console thread for managing clients
  pthread_t admin_thread;
  if (pthread_create(&admin_thread, NULL, admin_console, NULL) != 0) {
    perror("Admin thread creation failed");
    return -1;
  }

  // Main server loop for accepting client connections
  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_sock =
        accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sock < 0) {
      perror("Accept failed");
      continue;
    }

    // Add client connection to task queue for processing
    enqueue_task(client_sock, client_addr);
  }

  // Close the server socket
  close(server_sock);
  return 0;
}
