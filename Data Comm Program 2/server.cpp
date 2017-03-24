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

#define SEQNUMAMOUNT 8
// argv0: server
// argv1: <emulatorName: hostname for the emulator>
// argv2: <receiveFromEmulator: UDP port number used by the server to receive data from the emulator>
// argv3: <sendToEumulator-Port: UDP port number used by the emulator to receive ACKs from the server>
// argv4: <fileName: name of the file into which the received data is written>
int main(int argc, const char ** argv) {
	////////////VARIABLE INITIALIZATIONvvvv
	int nextSequenceNumber = 0;
	bool transmissionKill = false;			//sequence variables
	int receiverSequenceNumber;
	int keepLooking;

	char fileContents[32];
	int typePacket;
	char* dataInPacket;
	packet incomingPacket(NULL, NULL, NULL, fileContents);	//das incoming packet buffer and info
	char incomingPacketWindow[128];
	char outgoingAcksWindow[128];

	///////////CONNECTION INFORMATIONvvvv
	struct hostent* clientIP = gethostbyname(argv[1]);		//command given for the IP of client
	if (clientIP == NULL) {
		perror("- error: hostname does not exist");
		exit(EXIT_FAILURE);
	}
	int receivingPort = atoi(argv[2]);	//port inits
	int sendingPort = atoi(argv[3]);
	int receivingSocket;	//socket inits
	int sendingSocket;

	//set up connection
	struct sockaddr_in destination;
	socklen_t sizeDestination = sizeof(destination);	//destination socket init
	sizeDestination = sizeof destination;
	memset((char *)&destination, 0, sizeDestination);
	destination.sin_family = AF_INET;
	bcopy((char *)clientIP->h_addr, (char *)&destination.sin_addr.s_addr, clientIP->h_length);
	destination.sin_port = htons(sendingPort);

	struct sockaddr_in receiver;
	socklen_t sizeReceival = sizeof receiver;			//receiving socket init
	memset((char *)&receiver, 0, sizeReceival);
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(receivingPort);
	receiver.sin_addr.s_addr = htonl(INADDR_ANY);

	sendingSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (sendingSocket <= -1) {
		perror("- error: couldn't open the socket\n");			//create the sending socket here
		exit(EXIT_FAILURE);
	}

	int socketCheck;
	receivingSocket = socket(AF_INET, SOCK_DGRAM, 0);
	socketCheck = bind(receivingSocket, (sockaddr *)&receiver, sizeReceival);	//create the receive socket here
	if (receivingSocket <= -1){
		perror("- error: could'nt open the ack socket");	//did socket get assigned?
		exit(EXIT_FAILURE);
	}
	else if(socketCheck <= -1){
		perror("- error: could'nt bind the ack socket");	//did socket bind correctly?
		exit(EXIT_FAILURE);
	}

//////////////QUICK FILE SETUPvvvv
	fstream fileStream, incomingPackets;
	fileStream.open(argv[4], fstream::out);	//open the file given in commands
	incomingPackets.open("incomingPackets.log", fstream::out);	//launch log for packets arriving

////////////"Oh man. What even-?!" GBN CALCULATION PARTvvvv
	while (true){
		memset(fileContents, 0, 32);
		memset(outgoingAcksWindow, 0, 128);		//init the current ack/incoming windows again
		memset(incomingPacketWindow, 0, 128);

		recvfrom(receivingSocket, incomingPacketWindow, sizeof incomingPacketWindow, 0, (struct sockaddr *)&receiver, &sizeReceival);
		incomingPacket.deserialize(incomingPacketWindow);		//unpack the receivedpacket and write to incomingPackets.log
		receiverSequenceNumber = incomingPacket.getSeqNum();
		incomingPackets << receiverSequenceNumber << "\n";

		typePacket = incomingPacket.getType();		//if the packet type is file contents, write to file. Else, it's EOT and must be ended
		if(typePacket == 1){
				if (receiverSequenceNumber == nextSequenceNumber){ 		//is this packet the one we need? Sweet, dude. Use it.
					packet correctAcknowledgementPacket(0, receiverSequenceNumber, 0, NULL);
					correctAcknowledgementPacket.serialize(outgoingAcksWindow);		//we liked that packet, pack up the ack and send it
					sendto(sendingSocket, outgoingAcksWindow, sizeof outgoingAcksWindow, 0, (struct sockaddr*)&destination, sizeDestination);

					dataInPacket = incomingPacket.getData();
					for(int n=0; n<(incomingPacket.getLength()); n++){	//write the data from the packet to file after ack is sent
						fileStream << dataInPacket[n];
					}
					nextSequenceNumber = (nextSequenceNumber + 1) % SEQNUMAMOUNT; //next correct sequence number please
				}
				else {
					/*FUUUUUUUUUUUUUUUUCK*/
					keepLooking = (nextSequenceNumber + 7) % 8;
					packet incorrectAcknowledgementPacket(0, keepLooking, 0, NULL);
					memset(outgoingAcksWindow, 0, 128);
					incorrectAcknowledgementPacket.serialize(outgoingAcksWindow);
					sendto(sendingSocket, outgoingAcksWindow, sizeof outgoingAcksWindow, 0, (struct sockaddr*)&destination, sizeDestination);
				}
				/*FUUUUUUUUUUUUUUUUUCK*/
			}
			else if(typePacket == 3){
				packet killSignal(2, nextSequenceNumber, 0, NULL);		//sending the transmissionKill packet for EOT because, basically anything other than fileContents
				memset(outgoingAcksWindow, 0, 64);
				killSignal.serialize(outgoingAcksWindow);
				sendto(sendingSocket, outgoingAcksWindow, sizeof outgoingAcksWindow, 0, (struct sockaddr*)&destination, sizeDestination);
				transmissionKill = true;		//set the flag to end the while loop
			}
			if (transmissionKill == true)
				break;	//self-explanatory, was EOT flag set? break.
		}

	close(receivingSocket);
	close(sendingSocket);
	incomingPackets.close();		//finishing sockets and fstreams
	fileStream.close();
	return 0;
}
