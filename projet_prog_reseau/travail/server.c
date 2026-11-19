#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "include/common.h"

#define MAX_CONN 20

struct client_node {
    int fd;
    struct sockaddr_in addr;
    struct client_node *next;
};

int add_client(struct client_node **head, int fd, struct sockaddr_in addr) {
    struct client_node *new_client_node = malloc(sizeof(struct client_node));
    if (new_client_node == NULL) return -1;
    new_client_node->fd = fd;
    new_client_node->addr = addr;
    new_client_node->next = *head; 
    *head = new_client_node;
    return 0;
}

void remove_client(struct client_node **head, int fd) {
    struct client_node *current = *head;
    struct client_node *previous = NULL;
    while (current != NULL){
        if (current->fd == fd){
            if (previous == NULL) *head = current->next;
            else previous->next = current->next;
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

void free_client_list(struct client_node **head) {
    struct client_node *current = *head;
    while (current != NULL){
        struct client_node *next = current->next;
        close(current->fd);
        free(current);
        current = next;
    }
    *head = NULL;
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
    //printf("server: size received\n");
    return size;
}

int send_next_message_size(int socket, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    while (bytes_sent != sizeof(int)){
        ret = write(socket, (char *)&msg_size+bytes_sent,sizeof(msg_size)-bytes_sent);
        if (ret <= 0) return -1;
		bytes_sent += ret;
    }
    //printf("server: size sent\n");
    return 0;
}

int send_message(int socket, void * buf, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    //printf("server: sending message\n");
    while (bytes_sent != msg_size){
        ret = write(socket, (char *)buf+bytes_sent, msg_size-bytes_sent);
		if (ret <= 0) return -1;
        bytes_sent += ret;
    }
    //printf("server: message sent\n");
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
    //printf("server: message received\n");
    return 0;
}

int handle_bind(char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
    int opt = 1;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
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
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
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

	struct sockaddr_in cli;
	int welcoming_sfd;
	socklen_t len;
    struct client_node *clients = NULL;
    signal(SIGPIPE, SIG_IGN); // so we can write to a dead client without killing the server itself
	welcoming_sfd = handle_bind(port);
	if ((listen(welcoming_sfd, SOMAXCONN)) != 0) {
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}

    struct pollfd fds[MAX_CONN] = {0};
    for (int i = 1; i < MAX_CONN; i++) {
        fds[i].fd = -1;
    }
    fds[0].fd = welcoming_sfd;
    fds[0].events = POLLIN;
    int client_count = 0;

    while (1) {
        if (poll(fds, MAX_CONN, -1) == -1) {
            perror("poll()");
            break;
        }
        for (int i = 0; i < MAX_CONN; i++) {
            if (fds[i].fd == welcoming_sfd && (fds[i].revents & POLLIN)) {
                // ACCEPT -> will create new fd
	            len = sizeof(cli);
                int connection_fd = accept(welcoming_sfd, (struct sockaddr*) &cli, &len);
                if (connection_fd == -1) {
                    perror("accept()");
                    continue;
                }
                // should insert new fd into fds
                int j;
                for (j = 1; j < MAX_CONN; j++) {
                    if (fds[j].fd == -1) break; // we found a free slot
                }
                if (j == MAX_CONN) {
                    // no free slot found
                    close(connection_fd);
                    printf("too many clients\n");
                    continue; // next i loop
                }
                if (add_client(&clients, connection_fd, cli) != 0) {
                    perror("malloc()");
                    close(connection_fd);
                    continue;
                }
                fds[j].fd = connection_fd;
                fds[j].events = POLLHUP | POLLIN;
                fds[j].revents = 0;
                client_count++;
                printf("%d client connected\n", client_count);
                
            } else if (fds[i].fd != welcoming_sfd && (fds[i].revents & (POLLIN | POLLHUP))) {
                char buf[MSG_LEN] = {0};
                int close_connection_flag = 0;
                int msg_size = recv_message_size(fds[i].fd);
                if (msg_size <= 0 || msg_size >= MSG_LEN) close_connection_flag = 1; //client left
                else if (recv_message(fds[i].fd, buf, msg_size) != 0) close_connection_flag = 1; 
                else if (strcmp(buf, "/quit\n") == 0) close_connection_flag = 1;
                else {
                    printf("%s", buf);
                    // we echo size then message to client
                    if (send_next_message_size(fds[i].fd, msg_size) != 0
                        || send_message(fds[i].fd, buf, msg_size) != 0) {
                        close_connection_flag = 1;
                    }
                }
                if (close_connection_flag) {
                    printf("server: closing fd (%d)\n", fds[i].fd);
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                    remove_client(&clients, fds[i].fd);
                    client_count--;
                    printf("1 client connected, %d remaining\n", client_count);
                }
            } 
            // else if (fds[i].fd != welcoming_sfd && (fds[i].revents & POLLHUP)) {
            //     // close fd
            //     close(fds[i].fd);
            //     // free entry in fds
            //     fds[i].fd = -1;
            //     fds[i].events = 0;
            //     fds[i].revents = 0;
            // }
        }
    }
	printf("server: shutting down\n");
    free_client_list(&clients);
	close(welcoming_sfd);
	return EXIT_SUCCESS;
}