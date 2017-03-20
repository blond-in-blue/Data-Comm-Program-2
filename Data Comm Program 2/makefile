all: client server packet
	
client: client.o
	g++ client.cpp -o client
server: server.o
	g++ server.cpp -o server
packet: packet.o
	g++ packet.cpp -o packet
clean:
	\rm *.o client server packet
