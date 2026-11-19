#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "include/common.h"

int send_message(int socket, void * buf, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    printf("envoie du msg\n");
    while (bytes_sent != msg_size){
        ret = write(socket, buf+bytes_sent, msg_size-bytes_sent);
        bytes_sent += ret;
    }
    if (bytes_sent == 0) {
        return 1;
    }
    printf("Msg envoyé \n");
    return 0;
}

int recv_message(int sockfd, void * buf,int size){
    int ret = 0;
    int bytes_read = 0;
    while (bytes_read !=size){
        ret = read(sockfd, (char *)buf+bytes_read, size-bytes_read);
        bytes_read += ret;
    }
    printf("Message reçu \n");
    return bytes_read;
}

void echo_client(int sockfd) {
	char buff[MSG_LEN];
	int n;
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Getting message from client
		printf("Message: ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n') {} // trailing '\n' will be sent
		// Sending message (ECHO)
		if (send_message(sockfd, buff, strlen(buff)) != 0) {
            printf("nothing sent\n");
			break;
		}
		printf("Message sent!\n");
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv_message(sockfd, buff, MSG_LEN) <= 0) {
            printf("nothing read\n");
			break;
		}
		printf("Received: %s", buff);
	}
}

int handle_connect(char *domain_name, char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(domain_name, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <server_name> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *domain_name = argv[1];
    char *port = argv[2];
    
	int sfd;
	sfd = handle_connect(domain_name, port);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}

