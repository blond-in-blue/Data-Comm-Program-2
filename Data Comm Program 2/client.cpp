//
//  client.cpp
//  Data Comm Program 2
//
//  Created by Hunter Holder and Chandler Dill on 3/10/17.
//

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


int main(int argc, char ** argv){
	int mysocket = socket(AF_INET, SOCK_STREAM, 0);	//declares socket
	int negotPort = atoi(argv[2]);
	struct hostent *s;			//holds IP address
	s = gethostbyname(argv[1]);
	struct sockaddr_in server;
	memset((char *) &server, 0, sizeof(server));		//setting Destination info
	server.sin_family = AF_INET;
	server.sin_port = htons(negotPort);
	bcopy((char *)s->h_addr, (char *)&server.sin_addr.s_addr, s->h_length);
	
	
	connect(mysocket, (struct sockaddr *)&server, sizeof(server));
	
	
	char * chunksArray;
	int fileSize;				//open the file, grab the total length
	FILE * txtFile;
	txtFile = fopen((argv[3]), "rb");
	if (txtFile == NULL){
		cout << "Error, check file." << endl;
		exit(1);
	}
	fseek(txtFile, 0, SEEK_END);
	fileSize = ftell(txtFile);		//then close the file, to be opened again
	fclose(txtFile);
	
	
	txtFile = fopen((argv[3]), "rb");
	if (txtFile == NULL){				//open file for parsing the payload
		cout << "Error, check file." << endl;
		exit(1);
	}
	
	
	chunksArray = (char*) malloc (sizeof(char)*fileSize);
	size_t results = fread(chunksArray, 4, 1, txtFile);
	socklen_t slen = sizeof(server);		//create the buffers, grab the first payload
	char ACK[256];
	char endOfArray[512] = "EOF";
	
	
	while (!feof(txtFile)){
		sendto(mysocket, chunksArray, 512, 0, (struct sockaddr *)&server, slen);
		recvfrom(mysocket, ACK, 256, 0, (struct sockaddr *)&server, &slen);
		cout << ACK << endl;		//loop to handle sending payloads and receiving acknowledgements from server
		memset(chunksArray, '\0', sizeof(chunksArray));
		results = fread(chunksArray, 4, 1, txtFile);
	}
	
	
	sendto(mysocket, chunksArray, 512, 0, (struct sockaddr *)&server, slen);
	cout << chunksArray << std::flush;
	sendto(mysocket, endOfArray, 512, 0, (struct sockaddr *)&server, slen);	//send the end of file signal
	
	
	close(mysocket);
	free(chunksArray);	//clean up of socket, buffer, and stream
	fclose(txtFile);
	return 0;
};



