#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "include/common.h"
#include "include/client.h"
#include "include/protocol.h"
#include "include/msg_struct.h"

struct client {
	char nickname[NICK_LEN];
	int sockfd;
};

int client_connect(struct client *client, const char *host, const char *port) {
	client->nickname[0] = '\0';
	client->sockfd = -1;
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		return -1;
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			printf("------connected to %s------\n", host);
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		freeaddrinfo(result);
		return -1;
	}
	freeaddrinfo(result);
	client->sockfd = sfd;
	return 0;
}

// renvoie 0 si le client quitte
int client_run(struct client *client) {
	char buff[MSG_LEN];
	struct message msg;
	struct pollfd fds[2];
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[1].fd = client->sockfd;
	fds[1].events = POLLIN;

	while (1) {
		if (poll(fds, 2, -1) == -1) {
			perror("poll()");
			return -1;
		}

		// Data from keyboard
		if (fds[0].revents & POLLIN) {
			memset(&msg, 0, sizeof(msg)); // reset du struct message
			if (fgets(buff, MSG_LEN, stdin) == NULL) break; // on récupère la saisie clavier et on met '\0' à la fin
			buff[strcspn(buff, "\n")] = '\0';  // on enlève le '\n' de fin de saisie
			
			// on vérifie la nature de la commande			
			if (strcmp(buff, "/quit") == 0) {
				printf("client: quitting\n");
				break;
			}
			if (buff[0] == '\0') continue; // si l'user n'a rien entré

			char *pld = NULL; // préparation du payload éventuel
			strcpy(msg.nick_sender, client->nickname); // notre pseudo

			// on vérifie l'intitulé de la cmd ...
			if (strncmp(buff, "/nick ", 6) == 0) {
				msg.type = NICKNAME_NEW;
				strncpy(msg.infos, buff + 6, INFOS_LEN - 1);
			} else if (strcmp(buff, "/who") == 0) {
				msg.type = NICKNAME_LIST;
			} else if (strncmp(buff, "/whois ", 7)==0) {
				msg.type = NICKNAME_INFOS;
				strncpy(msg.infos, buff + 7, INFOS_LEN - 1);
			} else if (strncmp(buff, "/msgall ", 8) ==0) {
				msg.type = BROADCAST_SEND;
				pld = buff + 8;
			} else if (strncmp(buff, "/msg ", 5) ==0) {
				char *space = strchr(buff + 5, ' '); // premier espace après le pseudo
				if (space == NULL) {
					printf("usage: /msg <pseudo> <message>\n");
					continue;
				}
				*space = '\0'; // coupe buff en deux -> pseudo puis message
				msg.type = UNICAST_SEND;				
				strncpy(msg.infos, buff + 5, INFOS_LEN - 1); // pseudo du destinataire
				pld = space + 1; // message à envoyer
			} else if (buff[0] == '/') {
				printf("unknown command\n");
				continue;
			} else {
				msg.type = ECHO_SEND;
				pld = buff;
			}

			// ... et on envoie le message avec éventuellement le payload
			if (pld != NULL) msg.pld_len = strlen(pld);
			if (protocol_send_message(client->sockfd, &msg, pld) != 0) {
				printf("error while sending message\n");
				return -1;
			}
		}

		// Data from server
		if (fds[1].revents & (POLLIN | POLLHUP)) {
			void *payload = NULL;
			if (protocol_recv_message(client->sockfd, &msg, &payload) != 0) {
				printf("client: server left\n");
				return -1;
			}
			if (msg.type == NICKNAME_NEW && msg.infos[0] != '\0') {
				strcpy(client->nickname, msg.infos); // pseudo accepté par le serveur, donc on l'enregistre ici
			}
			if (payload != NULL) {
				printf("[%s] >> %s\n", msg.nick_sender, (char *)payload);
			}
			free(payload);
		}
	}
	return 0;
}

void client_close(struct client *client) {
	if (client->sockfd != -1) {
		close(client->sockfd);
		client->sockfd = -1;
	}

}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <server_name> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *domain_name = argv[1];
    char *port = argv[2];
	struct client client;
	if (client_connect(&client, domain_name, port) != 0) {
		fprintf(stderr, "Could not connect\n");
		return EXIT_FAILURE;
	}

	client_run(&client);
	client_close(&client);
	return EXIT_SUCCESS;
}

