//
//  server.cpp
//  Data Comm Program 2
//
//  Created by Hunter Holder and Chandler Dill on 3/10/17.
//

#include "packet.cpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#define ALLSEQNUMS 8

// argv0: server
// argv1: <emulatorName: hostname for the emulator>
// argv2: <receiveFromEmulator: UDP port number used by the server to receive data from the emulator>
// argv3: <sendToEumulator-Port: UDP port number used by the emulator to receive ACKs from the server>
// argv4: <fileName: name of the file into which the received data is written>
int main(int argc, const char ** argv) {
	
	// destination IP
	struct hostent* destinationIP = gethostbyname(argv[1]);
	if (destinationIP == NULL) {
		fprintf(stderr, "- error: hostname does not exist");
		exit(EXIT_FAILURE);
	}
	
	// set up ports
	int receivingPort = atoi(argv[2]);
	int sendingPort = atoi(argv[3]);
	
	// sockets
	int receivingSocket;
	int sendingSocket;
	
	// set up connection
	struct sockaddr_in destination;
	memset((char *)&destination, 0, sizeof(destination));
	destination.sin_family = AF_INET;
	bcopy((char *)destinationIP->h_addr, (char *)&destination.sin_addr.s_addr, destinationIP->h_length);
	destination.sin_port = htons(sendingPort);
	socklen_t destination_len = sizeof(destination);
	destination_len = sizeof destination;
	
	
	struct sockaddr_in incoming;
	memset((char *)&incoming, 0, sizeof(incoming));
	socklen_t incoming_len;
	incoming_len = sizeof incoming;
	incoming.sin_family = AF_INET;
	incoming.sin_port = htons(receivingPort);
	incoming.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//Create Sockets
	int errorCheck;
	
	sendingSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (sendingSocket <= -1) {
		perror("Could not open socket for sending file!\n");
		exit(EXIT_FAILURE);
	}
	receivingSocket = socket(AF_INET, SOCK_DGRAM, 0);
	errorCheck = bind(receivingSocket, (sockaddr *)&incoming, incoming_len);
	if (receivingSocket <= -1 || errorCheck <= -1){
		perror("Could not open socket for receiving acknowledgements!");
		exit(EXIT_FAILURE);
	}
	
	//File Stuff
	fstream arrival;
	arrival.open("arrival.log", fstream::out);
	fstream textfile;
	textfile.open(argv[4], fstream::out);
	
	//Transmission Variables
	int expected_seqnum = 0;
	bool endTransmission = false;
	int packet_type;
	char* packet_data;
	int incoming_seqnum;
	int highest_inorder;
	
	//Packet Containers
	char data[32];
	packet incoming_packet(0, 0, 0, data);
	char packet_buffer[128];
	char ack_buffer[128];
	
	
	while (true){
		
		memset(ack_buffer, 0, 128);
		memset(packet_buffer, 0, 128);
		memset(data, 0, 32);
		
		recvfrom(receivingSocket, packet_buffer, sizeof packet_buffer, 0, (struct sockaddr *)&incoming, &incoming_len);
		incoming_packet.deserialize(packet_buffer);
		
		//Record the sequence number of the received packet in arrival.log
		incoming_seqnum = incoming_packet.getSeqNum();
		arrival << incoming_seqnum << "\n";
		
		//Check the type. Write to file if it's data. End transmission if it's EOT.
		packet_type = incoming_packet.getType();
		switch (packet_type){
				
			case 1: {
				//Incoming Data Packet
				if (incoming_seqnum == expected_seqnum){
					
					//ACK packet
					packet ack_packet(0, incoming_seqnum, 0, 0);
					ack_packet.serialize(ack_buffer);
					sendto(sendingSocket, ack_buffer, sizeof ack_buffer, 0, (struct sockaddr*)&destination, destination_len);
					
					//write the incoming data to the file
					packet_data = incoming_packet.getData();
					for(int n=0; n<(incoming_packet.getLength()); n++){
						textfile << packet_data[n];
					}
					
					//increment variables
					expected_seqnum = (expected_seqnum + 1) % ALLSEQNUMS;
				}
				
				else{
					
					highest_inorder = (expected_seqnum + 7) % 8;
					packet ack_packet(0, highest_inorder, 0, 0);
					memset(packet_buffer, 0, 128);
					ack_packet.serialize(packet_buffer);
					sendto(sendingSocket, packet_buffer, sizeof packet_buffer, 0, (struct sockaddr*)&destination, destination_len);
				}
				
				endTransmission = false;
				break;
			}
				
			case 3: {
				//Client to Server EOT detected
				packet eot_packet(2, expected_seqnum, 0, 0);
				memset(packet_buffer, 0, 64);
				eot_packet.serialize(packet_buffer);
				sendto(sendingSocket, packet_buffer, sizeof packet_buffer, 0, (struct sockaddr*)&destination, destination_len);
				endTransmission = true;
				break;
			}
			default: {
				//Type is either 0 (ACK) or 2 (server to client EOT). Neither make sense.
				break;
			}
		}
		
		//Check if the EOT packet was received
		if (endTransmission) break;
	}
	
	//Done. Close things up and return.
	arrival.close();
	textfile.close();
	close(receivingSocket);
	close(sendingSocket);
	return 0;
}
