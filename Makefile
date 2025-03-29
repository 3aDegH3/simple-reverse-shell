CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SERVER_SRC = server.c
CLIENT_SRC = client.c
SERVER_EXE = server
CLIENT_EXE = client

all: $(SERVER_EXE) $(CLIENT_EXE)

$(SERVER_EXE): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_EXE) $(SERVER_SRC)

$(CLIENT_EXE): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_EXE) $(CLIENT_SRC)

run-server: $(SERVER_EXE)
	./$(SERVER_EXE)

run-client: $(CLIENT_EXE)
	./$(CLIENT_EXE)

run-both: $(SERVER_EXE) $(CLIENT_EXE)
	@echo "[*] Starting server..."
	@./$(SERVER_EXE) & sleep 2
	@echo "[*] Starting client..."
	@./$(CLIENT_EXE)

clean:
	rm -f $(SERVER_EXE) $(CLIENT_EXE)

.PHONY: all clean run-server run-client run-both
