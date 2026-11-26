#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "include/common.h"
#include "include/msg_struct.h"

int send_next_message_size(int socket, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    while (bytes_sent != sizeof(int)){
        ret = write(socket, (char *)&msg_size+bytes_sent,sizeof(msg_size)-bytes_sent);
        if (ret <= 0) return -1;
		bytes_sent += ret;
    }
    return 0;
}

int recv_message_size(int sockfd){
    int ret = 0;
    int bytes_read = 0;
    int size = 0 ;
    while (bytes_read != sizeof(int)){
        ret = read(sockfd, (char *)&size+bytes_read,sizeof(int)-bytes_read);
        if (ret <= 0) return -1;
		bytes_read += ret;
    }
    return size;
}

int send_message(int socket, void * buf, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    while (bytes_sent != msg_size){
        ret = write(socket, (char *)buf+bytes_sent, msg_size-bytes_sent);
		if (ret <= 0) return -1;
        bytes_sent += ret;
    }
    return 0;
}

int recv_message(int sockfd, void * buf, int size){
    int ret = 0;
    int bytes_read = 0;
    while (bytes_read != size){
        ret = read(sockfd, (char *)buf+bytes_read, size-bytes_read);
		if (ret <= 0) return -1;
        bytes_read += ret;
    }
    return 0;
}

void echo_client(int sockfd) {
	char buff[MSG_LEN];
	int msg_size;
	int i = 0;
	struct pollfd fds[2];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[1].fd = sockfd;
	fds[1].events = POLLIN;

	struct message msg;

	fflush(stdout);
	while (1) {
		if (poll(fds, 2, -1) == -1) {
			perror("poll()");
			break;
		}
		// Data from keyboard
		if (fds[0].revents & POLLIN) {
			memset(buff, 0, MSG_LEN);
			memset(&msg, 0, sizeof(msg));
			if (fgets(buff, MSG_LEN, stdin) == NULL) break; // Ctrl-D
			buff[strcspn(buff, "\n")] = '\0';  // on enlève le '\n' de fin
			// on vérifie la nature de la commande
			if (strncmp(buff, "/nick ", 6) == 0) {
				msg.type = NICKNAME_NEW;
				strncpy(msg.infos, buff + 6, INFOS_LEN - 1);
				msg.infos[INFOS_LEN - 1] = '\0';
				msg.pld_len = 0;
				// on envoie le struct message configuré
				if (send_next_message_size(sockfd, sizeof(struct message)) != 0) break;
				if (send_message(sockfd, (void *)&msg, sizeof(struct message)) != 0) break;
			} else if (strncmp(buff, "/who ", 5) == 0) {
				msg.type = NICKNAME_LIST;
				// on envoie le struct message configuré
				send_next_message_size(sockfd, sizeof(struct message));
				send_message(sockfd, (void *)&msg, sizeof(struct message));
			} else if (strncmp(buff, "/whois ", 7)==0){
				msg.type = NICKNAME_INFOS;
				strncpy(msg.infos, buff + 7, INFOS_LEN - 1);
				msg.infos[INFOS_LEN - 1] = '\0';
				msg.pld_len = 0;
				send_next_message_size(sockfd, sizeof(struct message));
				send_message(sockfd, (void *)&msg, sizeof(struct message));
			}

			if (strcmp(buff, "/quit") == 0) {
				printf("client: quitting\n");
				break;
			}
		}
		// Data from server
		if (fds[1].revents & (POLLIN | POLLHUP)) {
			msg_size = recv_message_size(sockfd);
			if (msg_size <= 0 || msg_size >= MSG_LEN) {
				printf("client: server left\n");
				break;
			}
			if (recv_message(sockfd, buff, msg_size) != 0) {
				printf("client: nothing read\n");
				break;
			}
			buff[msg_size] = '\0';
			printf("Received: %s\n", buff);
			fflush(stdout);
		}
	}
}

int handle_connect(char *domain_name, char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
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
			printf("------connected to %s------\n", domain_name);
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

