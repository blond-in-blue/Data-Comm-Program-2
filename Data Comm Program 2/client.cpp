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
	int destinationSocket = socket(AF_INET, SOCK_DGRAM, 0); // destination socket is used sending packets
	if (destinationSocket <= -1) {
		perror("failed to open destination socket");
		exit(EXIT_FAILURE);
	}
	int retrievalSocket = socket(AF_INET, SOCK_DGRAM, 0);	// retrieval socket is used for acks
	if (retrievalSocket <= -1) {
		perror("failed to open retrieval socket");
		exit(EXIT_FAILURE);
	}
	
	int sendingPort = atoi(argv[2]);
	int receivingPort = atoi(argv[3]);	//port setup for those sweet sockets
	
	struct hostent *s;				//holds IP address
	s = gethostbyname(argv[1]);
	if (s == NULL) {
		fprintf(stderr, "- error: hostname does not exist");	//quick error checking
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in destinationServer;
	memset((char *)&destinationServer, 0, sizeof(destinationServer));	//setting up destination info
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
	
	int outstandingPackets = 0;		//keep up with those unacked packets, yeah?
	int nextSequenceNumber = 0;
	int ackNumber = 0;
	int expectedAckNumber = 0;
	int seekOffset = 0;
	int sequenceNumberOffset = 0;		//ack number and file offset vars
	int counter = 0;
	
	clock_t packetTimer = -1;
	float packetTimerRecordedTime = 0.00;	//packet timer and packet buffer info
	char data[32] = "0";
	char packet[128] = "0";
	int fileSize = 0;
	memset(data, 0, sizeof(data));
	class packet receivingPacket(0, 0, sizeof(data), data);
	class packet lastReceivingPacket(0, 0, sizeof(data), data);
	
	FILE * txtFile;
	txtFile = fopen((argv[4]), "r");		//open that file you wanna read (Who is Brick?)
	if (txtFile == NULL) {
		cout << "error: specified file couldn't be opened." << endl;
		exit(1);
	}
	fseek(txtFile, 0, SEEK_END);
	
	bool endOfFileHasBeenReached = false;
	int nextSequenceNumberTotal = 0;		//window vars and eof
	int sendSizeMin = 0;
	
	fstream ackLog, seqNumLog;
	ackLog.open("ackLog.log", fstream::out);		//open the files for logging
	seqNumLog.open("seqNumLog.log", fstream::out);
	int fileInput = getc(txtFile);
	
	while (true) {							//THE WHILE LOOP OF DOOM
		
		while (nextSequenceNumberTotal < (sendSizeMin + N)) {

			if (endOfFileHasBeenReached == true) {		//eof? breakout.
				break;
			}
			seekOffset = 30 * nextSequenceNumberTotal;
			fseek(txtFile, seekOffset, SEEK_SET);
			
			memset(data, 0, sizeof(data));
			fileSize = 0;
			
			for (int i = 0; i < 30; i++) {		//getting data for sending packet
				fileInput = getc(txtFile);
				if (fileInput == EOF) {
					endOfFileHasBeenReached = true;
					break;
				}
				else {
					data[i] = fileInput;
					endOfFileHasBeenReached = false;	// needs to be set at least once, so why not every time? :^)
					fileSize = fileSize + 1;
				}
			}
			
			class packet packetOutgoing(1, nextSequenceNumber, fileSize, data);		//sending the packets of data
			packetOutgoing.serialize(packet);
			if (sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr*)&destinationServer, destinationServerLength) <= -1) {
				perror("failed to send packet.\n");				//check if success
				exit(EXIT_FAILURE);
			}
			
			if (outstandingPackets == 0) {		//we just sent!!! start timer!!!
				packetTimer = clock();
			}
			outstandingPackets = outstandingPackets + 1;
			seqNumLog << nextSequenceNumber << '\n';			//log that seqnum and increment
			nextSequenceNumber = (nextSequenceNumber + 1) % (8);
			nextSequenceNumberTotal = nextSequenceNumberTotal + 1;
		}
		
		if (packetTimerRecordedTime > -1) {
			packetTimerRecordedTime = ((clock() - packetTimer) / (float)CLOCKS_PER_SEC) * 1000;
			
			if (packetTimerRecordedTime >= TIMEOUTLIMIT) {
				perror("timeout limit has been reached");
				packetTimer = clock();						//reinit clock
				
				for (int x = 0; x < N; x++) {
					memset(data, 0, 32);
					memset(packet, 0, 128);
					
					nextSequenceNumberTotal = nextSequenceNumberTotal - N;	// set back sequence number to resend previous N packets
					nextSequenceNumber = nextSequenceNumberTotal % (8);
					fseek(txtFile, sequenceNumberOffset, SEEK_SET);
					sequenceNumberOffset = 30 * nextSequenceNumberTotal;
					
					memset(data, 0, sizeof(data));
					fileSize = 0;
					
					for (int y = 0; y < 30; y++) {
						fileInput = getc(txtFile);				//fill in next packet's data
						if (fileInput == EOF) {
							endOfFileHasBeenReached = true;		//check EOF
							break;
						}
						else {
							data[y] = fileInput;
							endOfFileHasBeenReached = false;	// needs to be set at least once, so why not every time? :^)
							fileSize = fileSize + 1;
						}
					}
					
					class packet packetOutgoing(1, nextSequenceNumber, fileSize, data);		// needs to be set at least once, so why not every time? :^)
					packetOutgoing.serialize(packet);
					if (sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr*)&destinationServer, destinationServerLength) <= -1) {
						perror("failed to send packet.\n");
						exit(EXIT_FAILURE);
					}
					
					seqNumLog << nextSequenceNumber << '\n';			//log that new seqnum
					nextSequenceNumber = (nextSequenceNumber + 1) % (8);
					nextSequenceNumberTotal = nextSequenceNumberTotal + 1;
					sendSizeMin = sendSizeMin + 1;
				}
			}
		}
		if (endOfFileHasBeenReached) {		//EOT?? Kill transmission
			break;
		}
		
		struct timeval recvTimer;		//timer to deal with ack timeouts
		
		recvTimer.tv_sec = 2;		//seconds
		recvTimer.tv_usec = 0;
		setsockopt(retrievalSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recvTimer, sizeof(recvTimer));
		
		fprintf(stderr, " %d ", outstandingPackets);		//receiving packets/acks
		recvfrom(retrievalSocket, packet, sizeof(packet), 0, (struct sockaddr *)&retrievalServer, &retrievalServerLength);
		lastReceivingPacket.deserialize(packet);

		ackNumber = lastReceivingPacket.getSeqNum();
		ackLog << ackNumber << '\n';
		printf("%d", ackNumber);
		
		// if this sequence number is greater than what we're currently waiting for, then go ahead and confirm all packets through that number
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
			nextSequenceNumberTotal = counter;
			nextSequenceNumber = (ackNumber + 1) % (8);
			sendSizeMin = sendSizeMin - 1;
		}
	}
	
	class packet eotPacket(3, nextSequenceNumber, 0, NULL);		// end of transmission
	eotPacket.serialize(packet);
	sendto(destinationSocket, packet, sizeof(packet), 0, (struct sockaddr *)&destinationServer, destinationServerLength);
	
	seqNumLog << nextSequenceNumber << '\n';
	

	recvfrom(retrievalSocket, packet, sizeof(packet), 0, (struct sockaddr *)&retrievalServer, &retrievalServerLength);
	eotPacket.deserialize(packet);

	ackNumber = eotPacket.getSeqNum();
	ackLog << ackNumber << '\n';			//logging those acks and we're almost done
	
	fclose(txtFile);
	seqNumLog.close();
	ackLog.close();
	close(destinationSocket);
	close(retrievalSocket); //clean up socket, buffer, and stream
	return 0;
};
