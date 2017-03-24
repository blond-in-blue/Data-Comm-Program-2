//
//  client.cpp
//  Data Comm Program 2
//
//  Created by Hunter Holder and Chandler Dill on 3/10/17.
//

#include "packet.cpp"
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <string>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <ostream>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#define N 7 // window size
#define TIMEOUTLIMIT 2000

// arg0: client
// arg1: <emulatorName: host address of the emulator>
// arg2: <sendToEmulator: UDP port number used by the emulator to receive data from the client>
// arg3: <receiveFromEmulator: UDP port number used by the client to receive ACKs from the emulator>
// arg4: <fileName: name of the file to be transferred>
int main(int argc, char ** argv) {
	// socket to me, baby
	// destination socket is used sending packets
	int destinationSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (destinationSocket <= -1) {
		perror("failed to open destination socket");
		exit(EXIT_FAILURE);
	}
	// retrieval socket is used for acks
	int retrievalSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (retrievalSocket <= -1) {
		perror("failed to open retrieval socket");
		exit(EXIT_FAILURE);
	}
	
	// ports
	int sendingPort = atoi(argv[2]);
	int receivingPort = atoi(argv[3]);
	
	struct hostent *s;			//holds IP address
	s = gethostbyname(argv[1]);
	if (s == NULL) {
		fprintf(stderr, "- error: hostname does not exist");	//quick error checking
		exit(EXIT_FAILURE);
	}
	
	// set up connection
	struct sockaddr_in destinationServer;
	// setting destination info
	memset((char *)&destinationServer, 0, sizeof(destinationServer));
	destinationServer.sin_family = AF_INET;
	bcopy((char *)s->h_addr, (char *)&destinationServer.sin_addr.s_addr, s->h_length);
	destinationServer.sin_port = htons(sendingPort);
	socklen_t destinationServerLength = sizeof(destinationServer);
	
	struct sockaddr_in retrievalServer;
	memset((char *)&retrievalServer, 0, sizeof(retrievalServer));		//setting ack retrieval info
	retrievalServer.sin_family = AF_INET;
	retrievalServer.sin_port = htons(receivingPort);
	retrievalServer.sin_addr.s_addr = htonl(INADDR_ANY);
	socklen_t retrievalServerLength = sizeof(retrievalServerLength);
	
	// variables to keep track of GBN
	int outstandingPackets = 0;
	int nextSequenceNumber = 0;
	int ackNumber = 0;
	int expectedAckNumber = 0;
	int seekOffset = 0;
	int sequenceNumberOffset = 0;
	int counter = 0;
	
	
	// packet setup
	clock_t packetTimer = -1;
	float packetTimerRecordedTime = 0.00;
	char data[32] = "0";
	char packet[128] = "0";
	int fileSize = 0;
	memset(data, 0, sizeof(data));
	class packet receivingPacket(0, 0, 0, data);
	class packet lastReceivingPacket(0, 0, 0, data);
	
	
	// file stuff
	char * chunksArray;
	FILE * txtFile;
	txtFile = fopen((argv[4]), "r");
	if (txtFile == NULL) {
		cout << "error: specified file couldn't be opened." << endl;
		exit(1);
	}
	fseek(txtFile, 0, SEEK_END);
	
	bool endOfFileHasBeenReached = false;
	
	// sliding window stuff
	int nextSequenceNumberSliding = 0;
	int sendSizeMin = 0;
	
	// logging
	fstream ackLog, seqNumLog;
	ackLog.open("ackLog.log", fstream::out);
	seqNumLog.open("seqNumLog.log", fstream::out);
	
	// file input character
	int fileInput = getc(txtFile);
	
	// never-ending while loop
	while (true) {
		
		
		while (nextSequenceNumberSliding < (sendSizeMin + N)) {
			// break out of while if EOF
			if (endOfFileHasBeenReached == true) {
				break;
			}
			seekOffset = 30 * nextSequenceNumberSliding;
			fseek(txtFile, seekOffset, SEEK_SET);
			memset(data, 0, sizeof(data));
			fileSize = 0;
			for (int i = 0; i < 30; i++) {
				
				fileInput = getc(txtFile);
				
				if (fileInput == EOF) {
					endOfFileHasBeenReached = true;
					break;
				}
				else {
					data[i] = fileInput;
					endOfFileHasBeenReached = false;
					fileSize = fileSize + 1;
				}
			}
			
			// attempt to send packet
			class packet packetOutgoing(1, nextSequenceNumber, fileSize, data);
			packetOutgoing.serialize(packet);
			if (sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr*)&destinationServer, destinationServerLength) <= -1) {
				perror("failed to send packet.\n");
				exit(EXIT_FAILURE);
			}
			
			// start timer
			if (outstandingPackets == 0) {
				packetTimer = clock();
			}
			outstandingPackets = outstandingPackets + 1;
			
			// logging
			seqNumLog << nextSequenceNumber << '\n';
			nextSequenceNumber = (nextSequenceNumber + 1) % (8);
			sendSizeMin = sendSizeMin + 1;
		}
		
		if (packetTimerRecordedTime > -1) {
			packetTimerRecordedTime = ((clock() - packetTimer) / (float)CLOCKS_PER_SEC) * 1000;
			
			if (packetTimerRecordedTime >= TIMEOUTLIMIT) {
				perror("timeout limit has been reached");
				// reinitialize timer
				packetTimer = clock();
				
				for (int x = 0; x < N; x++) {
					memset(data, 0, 32);
					memset(packet, 0, 128);
					
					// set back sequence number to resend previous N packets
					nextSequenceNumberSliding = nextSequenceNumberSliding - N;
					nextSequenceNumber = nextSequenceNumberSliding % (8);
					fseek(txtFile, sequenceNumberOffset, SEEK_SET);
					sequenceNumberOffset = 30 * nextSequenceNumberSliding;
					
					memset(data, 0, sizeof(data));
					fileSize = 0;
					for (int y = 0; y < 30; y++) {
						fileInput = getc(txtFile);
						if (fileInput == EOF) {
							endOfFileHasBeenReached = true;
							break;
						}
						else {
							data[y] = fileInput;
							// needs to be set at least once, so why not every time? :^)
							endOfFileHasBeenReached = false;
							fileSize = fileSize + 1;
						}
					}
					
					// attempt to send packet
					class packet packetOutgoing(1, nextSequenceNumber, fileSize, data);
					packetOutgoing.serialize(packet);
					if (sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr*)&destinationServer, destinationServerLength) <= -1) {
						perror("failed to send packet.\n");
						exit(EXIT_FAILURE);
					}
					
					// logging
					seqNumLog << nextSequenceNumber << '\n';
					nextSequenceNumber = (nextSequenceNumber + 1) % (8);
					nextSequenceNumberSliding = nextSequenceNumberSliding + 1;
					
				}
			}
		}
		
		// check if transmission is complete
		if (endOfFileHasBeenReached == true && outstandingPackets <= 0) {
			break;
		}
		
		// create timer to deal with ack time-outs
		struct timeval recvTimer;
		
		// in seconds
		recvTimer.tv_sec = 2;
		recvTimer.tv_usec = 0;
		setsockopt(retrievalSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimer, sizeof(recvTimer));
		
		
		// packet receival
		memset(packet, 0, 64);
		
		recvfrom(retrievalSocket, packet, sizeof(packet), 0, (struct sockaddr *)&retrievalServer, &retrievalServerLength);
		lastReceivingPacket.deserialize(packet);

		ackNumber = lastReceivingPacket.getSeqNum();
		ackLog << ackNumber << '\n';
		
		if (ackNumber == expectedAckNumber) {
			expectedAckNumber = (expectedAckNumber + 1) % (8);
			outstandingPackets = outstandingPackets - 1;
			sendSizeMin = sendSizeMin + 1;
			if (outstandingPackets > 0) {
				packetTimer = clock();
			}
			counter = counter + 1;
		}
		else {
			nextSequenceNumberSliding = counter;
			nextSequenceNumber = (ackNumber + 1) % (8);
			sendSizeMin = sendSizeMin - 1;
			outstandingPackets = 0;
		}
	}
	
	// end of transmission
	class packet eotPacket(3, nextSequenceNumber, 0, NULL);
	eotPacket.serialize(packet);
	sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr *)&destinationServer, destinationServerLength);
	
	seqNumLog << nextSequenceNumber << '\n';
	
	recvfrom(retrievalSocket, packet, sizeof(packet), 0, (struct sockaddr *)&retrievalServer, &retrievalServerLength);
	eotPacket.deserialize(packet);
	
	ackNumber = eotPacket.getSeqNum();
	ackLog << ackNumber << '\n';
	
	// finishing up
	fclose(txtFile);
	seqNumLog.close();
	ackLog.close();
	close(destinationSocket);
	close(retrievalSocket);
	free(chunksArray);	//clean up of socket, buffer, and stream
	return 0;
};
