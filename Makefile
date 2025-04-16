server: server.c messages.h
	gcc -o3 -o server server.c -lssl -lcrypto -lpthread