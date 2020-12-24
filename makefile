all: server client

server: server.cpp ZMQ.h
	g++ server.cpp -pthread -lzmq -o server

client: client.cpp ZMQ.h
	g++ client.cpp -pthread -lzmq -o client

clean:
	rm -rf *.o calculation control