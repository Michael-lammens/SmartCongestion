/*
 * Michael lammens
 * XLW945
 * 11335630
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <protocol.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <list.h>
#include <signal.h>
#define emulator_port 32582

/*
typedef struct {
	int pack_id;
        int source_port;
        int num_bytes;
        char start_time[30];
}Segment;
*/



void sendToClient(char* data, int port, int bytes_received){
	 // Create a new socket for sending response
        int response_sockfd;
        if ((response_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
        // Prepare response address structure
        struct sockaddr_in response_addr;
        response_addr.sin_family = AF_INET;
        response_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        response_addr.sin_port = htons(port);

        // Send the packet back to the client
        if (sendto(response_sockfd, data, bytes_received, 0, (struct sockaddr *)&response_addr,sizeof(response_addr)) < 0) {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
	printf("\nsent to port:%d",port);
	close(response_sockfd);
}


int main(int argc, char *argv[]) {
        if (argc != 2) {
                fprintf(stderr, "Usage: %s"
                                "<our port>\n",
                                argv[0]);
                exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[1]);/*Our Port*/
    
    char data[512] = {'\0'};
    int sock_fd_us = socket(AF_INET, SOCK_DGRAM, 0); 
    int sock_fd_em = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_us == -1||sock_fd_em == -1) {
        perror("Socket create failed");
    }
    


    char buffer_out_good[512] = {'\0'};

    int enqueue_status;
    struct sockaddr_in server_addr, emulator_addr;
	    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(server_port);
    
    emulator_addr.sin_family = AF_INET;
    emulator_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    emulator_addr.sin_port = htons(emulator_port);
    /*Bind to server*/
    if (bind(sock_fd_us, (struct sockaddr *)&server_addr, sizeof(server_addr))
                    == -1) {
        perror("Bind failed");
        close(sock_fd_us);
        printf("\nRec and proc failed\n");
        exit(-1);
    }


    /*Packet we receive from emulator */
    Segment* temp = (Segment*)malloc(sizeof(Segment));
    while (1) {
        memset(data, '\0', sizeof(data));

        socklen_t addr_len = sizeof(server_addr);
	/*Receive from emulator*/
	printf("\nWaiting to receive\n");

	int bytes_received = recvfrom(sock_fd_us, data, sizeof(data), 0,
                                      (struct sockaddr *)&server_addr,
                                      &addr_len);
        if (bytes_received == -1) {
                perror("recvfrom");
                exit(EXIT_FAILURE);
        }
	deserialize(data, temp);
	char times_[30];
	strcpy(times_,temp->start_time); 
	printf("\nSERVER_RECEIVED --    source_port: %d\n, start time %s",ntohs(temp->source_port),times_);
	
	sendToClient(data, ntohs(temp->source_port), 512);	
	
	memset(data, '\0', sizeof(data));
    	
	strcpy(temp->start_time,"");
	temp->source_port=0;
    
    }
    return 0;
}

