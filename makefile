all: server client

server: server.cpp ZMQ.h
	g++ server.cpp -g -pthread -lzmq -o server

client: client.cpp ZMQ.h
	g++ client.cpp -g -pthread -lzmq -o client

clean:
	rm -rf *.o calculation control