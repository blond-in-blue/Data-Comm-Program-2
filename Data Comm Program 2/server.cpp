//
//  server.cpp
//  Data Comm Program 2
//
//  Created by Hunter Holder and Chandler Dill on 3/10/17.
//

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


int main(int argc, const char * argv[]) {
	int mysocket = socket(AF_INET, SOCK_DGRAM, 0);	//setup UDP socket
	int negotPort = atoi(argv[1]);
	
	struct sockaddr_in server;
	memset((char *) &server, 0, sizeof(server));
	
	memset((char *) &server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(negotPort);		//UDP info and address handling
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	
	
	bind(mysocket, (struct sockaddr *)&server, sizeof(server));
	
	
	char textContents[512];
	struct sockaddr_in client;		//setting up payload buffer and client variables
	socklen_t clen = sizeof(client);
	
	
	FILE * output;
	output = fopen("output.txt", "w+");
	
	
	while(strcmp(textContents,"EOF") != 0){		//loop through all packets recieved, then promptly uppercases them and send them back as acknowledgement
		recvfrom(mysocket, textContents, 512, 0, (struct sockaddr *)&client, &clen);
		if(strcmp(textContents, "EOF") != 0){
			fputs(textContents, output);}
		for(int count = 0; count < 4; count = count + 1){
			textContents[count] = toupper(textContents[count]);
		}
		sendto(mysocket, textContents, 256, 0, (struct sockaddr *)&client, sizeof(client));
	}
	
	fclose(output);
	close(mysocket);
	return 0;
}
