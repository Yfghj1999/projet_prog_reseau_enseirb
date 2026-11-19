#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "include/common.h"

#define MAX_CONN 20

int send_message(int socket, void * buf, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    printf("envoie du msg\n");
    while (bytes_sent != msg_size){
        ret = write(socket, buf+bytes_sent, msg_size-bytes_sent);
        bytes_sent += ret;
    }
    printf("Msg envoyé \n");
    return 0;
}

int recv_message(int sockfd, void * buf,int size){
    int ret = 0;
    int bytes_read = 0;
    while (bytes_read !=size){
        ret = read(sockfd, (char *)buf+bytes_read, size-bytes_read);
        if (ret == 0){
            return 0;
        }
        bytes_read += ret;
    }
    printf("Message reçu \n");
    return bytes_read;
}

void echo_server(int sockfd) {
	char buff[MSG_LEN];
	while (1) {
		// Cleaning memory
		memset(buff, 0, MSG_LEN);
		// Receiving message
		if (recv(sockfd, buff, MSG_LEN, 0) <= 0) {
			break;
		}
		printf("Received: %s\n", buff);
		// Sending message (ECHO)
		if (send(sockfd, buff, strlen(buff), 0) <= 0) {
			break;
		}
		printf("Message sent!\n");
	}
}

int handle_bind(char *port) {
    //printf("port : %s\n", port);
	struct addrinfo hints, *result, *rp;
	int sfd;
    int opt = 1;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
        int opt_ret = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s<server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *port = argv[1];

	struct sockaddr cli;
	int welcoming_sfd;
	socklen_t len;
	welcoming_sfd = handle_bind(port);
	if ((listen(welcoming_sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}
	len = sizeof(cli);

    struct pollfd fds[MAX_CONN] = {0};
    for (int i = 1; i < MAX_CONN; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }
    fds[0].fd = welcoming_sfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    while (1) {
        printf("will poll...\n");
        int nfds = poll(fds, MAX_CONN, -1);
        printf("%d fd active\n", nfds);
        for (int i = 0; i < MAX_CONN; i++) {
            if (fds[i].fd == welcoming_sfd && (fds[i].revents & POLLIN)) {
                // ACCEPT -> will create new fd 
                int connection_fd = accept(welcoming_sfd, (struct sockaddr*) &cli, &len);
                // should insert new fd into fds
                int j;
                for (j = 1; j < MAX_CONN; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = connection_fd;
                        fds[j].events = POLLHUP | POLLIN;
                        fds[j].revents = 0;
                        break;
                    } 
                }
                if (j == MAX_CONN) {// no free slot found
                    close(connection_fd);
                    printf("too many clients\n");
                }
            } else if (fds[i].fd != welcoming_sfd && (fds[i].revents & POLLIN)) {
                char buf[128] = {0};
                int ret = recv_message(fds[i].fd, buf, sizeof(buf));
                printf("ret: %d\n", ret);
                if (ret != 0) {
                    printf("on rentre dans le if du send\n");
                    ret = send_message(fds[i].fd, buf, sizeof(buf));
                }
                else{
                    printf("on ferme suite mort client\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                }
                //echo_server(fds[i].fd);
            } else if (fds[i].fd != welcoming_sfd && (fds[i].revents & POLLHUP)) {
                // close fd
                close(fds[i].fd);
                // free entry in fds
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;
            }
            
        }
    }

	//echo_server(connection_fd);
	close(welcoming_sfd);
	return EXIT_SUCCESS;
}

