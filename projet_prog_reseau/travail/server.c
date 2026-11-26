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
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "include/protocol.h"
#include "include/common.h"
#include "include/msg_struct.h"

#define MAX_CONN 20
#define WELCOME "Welcome on the chat "
#define NICK_MAX 32
#define INVALID_NICKNAME "Invalid nickname"
#define NICKNAME_TAKEN "Nickname already taken"
#define WHO_LIST "Online users are\n"
#define NO_NICKNAME "Need a nickname"

struct client_node {
    int fd;
    char nickname[NICK_LEN];
    struct sockaddr_in addr;
    time_t connection_time;
    struct client_node *next;
};

int add_client(struct client_node **head, int fd, struct sockaddr_in addr) {
    struct client_node *new_client_node = malloc(sizeof(struct client_node));
    if (new_client_node == NULL) return -1;
    new_client_node->fd = fd;
    new_client_node->nickname[0] = '\0';
    new_client_node->addr = addr;
    new_client_node->connection_time = time(NULL);
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

void add_nickname_client(struct client_node **head, int fd, char *nickname) {
    struct client_node *current = *head;
    while (current != NULL) {
        if (current->fd == fd) {
            strcpy(current->nickname, nickname);
            //printf("successfully added %s (nickname): %s (client.nickname)\n", nickname, current->nickname);
            break;
        }
        current = current->next;
    }
}

// return 0 if valid
int is_valid_nickname(char *nickname) {
    size_t len = strlen(nickname);
    if ((len == 0) || len > NICK_MAX ) return 1;
    while (*nickname != '\0') {
        if (isalnum((unsigned char)*nickname) == 0) return 1;
        nickname++;
    }
    return 0;
}

// return 0 if free
int is_nickname_free(struct client_node **head, char *nickname) {
    struct client_node *current = *head;
    while (current != NULL) {
        if (strcmp(current->nickname, nickname) == 0) return 1;
        current = current->next;
    }
    return 0;
}
struct client_node *find_client_by_fd(struct client_node **head, int fd){
    struct client_node *current = *head;
    while (current != NULL) {
        if (current->fd==fd ) return current;
        current = current->next;
    }
    return NULL;
}
struct client_node *find_client_by_nick(struct client_node **head, char *nickname){
    struct client_node *current = *head;
    while (current != NULL) {
        if (strcmp(nickname,current->nickname)==0 ) return current;
        current = current->next;
    }
    return NULL;

}

int send_server_text(int fd, enum msg_type type, const char *payload, const char *infos) {
    struct message reply;
    memset(&reply, 0, sizeof(reply));
    strcpy(reply.nick_sender, "Server");
    reply.type = type;
    if (infos != NULL) {
        strncpy(reply.infos, infos, INFOS_LEN - 1);
    }
    reply.pld_len = strlen(payload);
    return protocol_send_message(fd, &reply, payload);
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
        fprintf(stderr, "usage: %s <server_port>\n", argv[0]);
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
		perror("listen()");
		exit(EXIT_FAILURE);
	}

    struct pollfd fds[MAX_CONN] = {0};
    for (int i = 1; i < MAX_CONN; i++) {
        fds[i].fd = -1;
    }
    fds[0].fd = welcoming_sfd;
    fds[0].events = POLLIN;

    struct message msg;

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
                
            } else if (fds[i].fd != welcoming_sfd && (fds[i].revents & (POLLIN | POLLHUP))) {
                int close_connection_flag = 0;
                void *payload = NULL;
                struct client_node *current = clients;

                // on recoit la struct
                if (protocol_recv_message(fds[i].fd, &msg, &payload) != 0) close_connection_flag = 1;
                else {
                    msg.infos[INFOS_LEN - 1] = '\0';
                    switch (msg.type) {
                        case NICKNAME_NEW:
                            // on ajoute le pseudo au client concerné
                            if (is_valid_nickname(msg.infos) != 0) {
                                if (send_server_text(fds[i].fd, NICKNAME_NEW, INVALID_NICKNAME, NULL) != 0) {
                                    close_connection_flag = 1;
                                }
                                break;
                            }
                            if (is_nickname_free(&clients, msg.infos) != 0) {
                                if (send_server_text(fds[i].fd, NICKNAME_NEW, NICKNAME_TAKEN, NULL) != 0) {
                                    close_connection_flag = 1;
                                }
                                break;
                            }
                            else {
                                add_nickname_client(&clients, fds[i].fd, msg.infos); // penser à vérifier l'ajout du nickname
                                printf("%s is connected\n", msg.infos);
                                // on lui répond
                                char welcome_buff[sizeof(WELCOME) + INFOS_LEN];
                                snprintf(welcome_buff, sizeof(welcome_buff), "%s%s", WELCOME, msg.infos);
                                if (send_server_text(fds[i].fd, NICKNAME_NEW, welcome_buff, msg.infos) != 0) close_connection_flag = 1;
                                break;
                            }
                        case NICKNAME_LIST: {
                            char who_list_buff[MSG_LEN] = {0};
                            strcpy(who_list_buff, WHO_LIST);
                            while (current != NULL) {
                                if (current->nickname[0] != '\0') {              // ignore les clients sans pseudo
                                    strcat(who_list_buff, "\t- ");
                                    strcat(who_list_buff, current->nickname);
                                    strcat(who_list_buff, "\n");
                                }
                                current = current->next;
                            }
                            if (send_server_text(fds[i].fd, NICKNAME_LIST, who_list_buff, NULL) != 0) close_connection_flag = 1;
                            break;
                        }
                        case NICKNAME_INFOS: {
                            char who_is_buff[MSG_LEN] = {0};
                            char date_str[32];
                            while (current != NULL) {
                                if (strcmp(current->nickname, msg.infos) == 0) {
                                    strftime(date_str, sizeof(date_str), "%Y/%m/%d@%H:%M", localtime(&current->connection_time));
                                    snprintf(who_is_buff, sizeof(who_is_buff),
                                        "%s connected since %s with IP address %s and port number %hu",
                                        current->nickname,
                                        date_str,
                                        inet_ntoa(current->addr.sin_addr),
                                        ntohs(current->addr.sin_port));
                                }
                                current = current->next;
                            }
                            if (who_is_buff[0] == '\0') {
                                snprintf(who_is_buff, sizeof(who_is_buff), "Unknown user %s", msg.infos);
                            }
                            if (send_server_text(fds[i].fd, NICKNAME_INFOS, who_is_buff, NULL) != 0) close_connection_flag = 1;
                            break;
                        }
                        case BROADCAST_SEND: {
                            struct client_node *sender = find_client_by_fd(&clients,fds[i].fd);
                            struct message fwd;

                            if (sender == NULL || sender->nickname[0] == '\0') {
                                send_server_text(fds[i].fd,BROADCAST_SEND,NO_NICKNAME,NULL);
                                break;
                            }

                            memset(&fwd, 0, sizeof(fwd));
                            fwd.type = BROADCAST_SEND;
                            strcpy(fwd.nick_sender,sender->nickname);
                            fwd.pld_len = msg.pld_len;

                            for (current = clients; current != NULL; current = current->next) {
                                if (strcmp(current->nickname,sender->nickname)!=0&&current->nickname[0]!='\0') {
                                    protocol_send_message(current->fd, &fwd, payload);
                                }
                            }
                            break;
                        }
                        case UNICAST_SEND: {
                            struct client_node *sender = find_client_by_fd(&clients,fds[i].fd);
                            
                            struct message fwd;
                            // trouver la fd avec le non du client
                            char *target = msg.infos;
                            if (sender == NULL || sender->nickname[0] == '\0') {
                                send_server_text(fds[i].fd,UNICAST_SEND,NO_NICKNAME,NULL);
                                break;
                            }
                            struct client_node *dest = find_client_by_nick(&clients,target);
                            if (dest == NULL||strcmp(dest->nickname,"") == 0){
                                char err_buff[MSG_LEN];
                                snprintf(err_buff, sizeof(err_buff), "Unknown user %s", target);
                                send_server_text(fds[i].fd,UNICAST_SEND,err_buff,NULL);
                                break;
                            }
                            memset(&fwd, 0, sizeof(fwd));
                            fwd.type = UNICAST_SEND;
                            strcpy(fwd.nick_sender,sender->nickname);
                            fwd.pld_len = msg.pld_len;
                            protocol_send_message(dest->fd, &fwd, payload);
                            break;


                        }
                        default:
                            printf("switch default\n");
                    }
                }
                free(payload);
                if (close_connection_flag) {
                    printf("server: closing fd (%d)\n", fds[i].fd);
                    remove_client(&clients, fds[i].fd);
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0;
                }
            }
        }
    }
	printf("server: shutting down\n");
    free_client_list(&clients);
	close(welcoming_sfd);
	return EXIT_SUCCESS;
}