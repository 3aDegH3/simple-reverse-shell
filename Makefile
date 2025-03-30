CC = gcc
SERVER_SRC = server.c
CLIENT_SRC = client.c
ADV_SERVER_SRC = advanced_server.c
SERVER_EXE = server
CLIENT_EXE = client
ADV_SERVER_EXE = advanced_server

all: $(SERVER_EXE) $(CLIENT_EXE) $(ADV_SERVER_EXE)

$(SERVER_EXE): $(SERVER_SRC)
	$(CC) -o $(SERVER_EXE) $(SERVER_SRC)

$(CLIENT_EXE): $(CLIENT_SRC)
	$(CC) -o $(CLIENT_EXE) $(CLIENT_SRC)

$(ADV_SERVER_EXE): $(ADV_SERVER_SRC)
	$(CC) -o $(ADV_SERVER_EXE) $(ADV_SERVER_SRC)

run-server: $(SERVER_EXE)
	./$(SERVER_EXE)

run-client: $(CLIENT_EXE)
	./$(CLIENT_EXE)

run-adv-server: $(ADV_SERVER_EXE)
	./$(ADV_SERVER_EXE)

run-both: $(SERVER_EXE) $(CLIENT_EXE)
	@echo "[*] Starting server..."
	@./$(SERVER_EXE) & sleep 2
	@echo "[*] Starting client..."
	@./$(CLIENT_EXE)

clean:
	rm -f $(SERVER_EXE) $(CLIENT_EXE) $(ADV_SERVER_EXE)

.PHONY: all clean run-server run-client run-adv-server run-both
