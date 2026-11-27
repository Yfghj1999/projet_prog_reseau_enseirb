#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"
#include "client.h"
#include "protocol.h"
#include "msg_struct.h"
#include "include/file_transfert.h"

// contient la socket de connexion au serveur et le pseudo du client 
struct client {
	char nickname[NICK_LEN];
	int sockfd;
	int send_pending;
	char send_to[NICK_LEN];
	char send_path[MSG_LEN];
};

// envoie une demande de connexion
int client_connect(struct client *client, const char *host, const char *port) {
	client->nickname[0] = '\0';
	client->send_pending = 0;
	client->send_to[0] = '\0';
	client->send_path[0] = '\0';
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
// accepte une connexion entrante et reçoit un fichier, renvoie 0 si OK, -1 sinon
static int open_listener(int server_fd, char *out, size_t out_size)
{
	struct sockaddr_in addr;
	socklen_t len;
	char ip[INET_ADDRSTRLEN];
	int fd;

	// socket TCP
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) { perror("socket"); return -1; }

	// bind sur le port 0 = "système, choisis-moi un port libre"
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);   // ecouter sur toutes les interfaces
	addr.sin_port = htons(0);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind"); close(fd); return -1;
	}

	// ecoute sur la socket
	if (listen(fd, 1) == -1) { perror("listen"); close(fd); return -1; }

	// port ?
	len = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *)&addr, &len) == -1) {
		perror("getsockname"); close(fd); return -1;
	}
	unsigned short port = ntohs(addr.sin_port); // converti dans l'ordre de la machine

	// ip ?
	len = sizeof(addr);
	if (getsockname(server_fd, (struct sockaddr *)&addr, &len) == -1) {
		perror("getsockname"); close(fd); return -1;
	}
	inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

	//fabriquer "ip:port" 
	snprintf(out, out_size, "%s:%hu", ip, port);
	return fd;
}

// gere le polling de connexion et renvoie 0 si le client quitte
int client_run(struct client *client) {
	char buff[MSG_LEN]; // ce buffer sert a stocker les entrees clavier de l'utilisateur
	struct message msg; // struct msg pour envoyer ou recevoir 
	struct pollfd fds[2];
	fds[0].fd = 0; // entrée standard
	fds[0].events = POLLIN; 
	fds[1].fd = client->sockfd; // socket de communication avec le serveur
	fds[1].events = POLLIN; 

	while (1) {
		if (poll(fds, 2, -1) == -1) {
			perror("poll()");
			return -1;
		}

		// Data from keyboard
		if (fds[0].revents & POLLIN) {
			memset(&msg, 0, sizeof(msg)); // reset du struct message
			if (fgets(buff, MSG_LEN, stdin) == NULL) break; // on recupere la saisie clavier et on met '\0' a la fin
			buff[strcspn(buff, "\n")] = '\0';  // on enleve le '\n' de fin de saisie
			
			// on verifie la nature de la commande			
			if (strcmp(buff, "/quit") == 0) {
				printf("client: quitting\n");
				break;
			}
			if (buff[0] == '\0') continue; // si l'user n'a rien saisi

			char *pld = NULL; // preparation du payload eventuel
			strcpy(msg.nick_sender, client->nickname); // on copie notre pseudo dans les informations à envoyer

			// on verifie l'intitule de la cmd ...
			if (strncmp(buff, "/nick ", 6) == 0) { // l'utilisateur choisit un pseudo
				msg.type = NICKNAME_NEW;
				copy_str(msg.infos, sizeof(msg.infos), buff + 6);
			} else if (strcmp(buff, "/who") == 0) { // l'utilisateur veut connaître les utilisateurs connectés
				msg.type = NICKNAME_LIST;
			} else if (strncmp(buff, "/whois ", 7) == 0) { // l'utilisateur souhaite des informations sur un utilisateur
				msg.type = NICKNAME_INFOS;
				copy_str(msg.infos, sizeof(msg.infos), buff + 7);
			} else if (strncmp(buff, "/msgall ", 8) == 0) { // l'utilisateur souhaite envoyer un message a tout le monde
				msg.type = BROADCAST_SEND;
				pld = buff + 8;
			} else if (strncmp(buff, "/msg ", 5) == 0) { // l'utilisateur souhaite envoyer un message a un utilisateur
				char *space = strchr(buff + 5, ' '); // on recupere l'emplacement du premier espace apres le pseudo
				if (space == NULL) {
					printf("usage: /msg <pseudo> <message>\n");
					continue;
				}
				*space = '\0'; // coupe buff en deux pour separer le pseudo puis le message
				msg.type = UNICAST_SEND;
				copy_str(msg.infos, sizeof(msg.infos), buff + 5); // on recupere le pseudo du destinataire saisi par l'utilisateur
				pld = space + 1; // message a envoyer
			} else if (strncmp(buff, "/send ", 6) == 0) { // l'utilisateur souhaite envoyer un fichier a un autre utilisateur
				char *space = strchr(buff + 6, ' '); // on recupere l'emplacement du premier espace apres le pseudo
				if (space == NULL) {
					printf("usage: /send <pseudo> </chemin/nom_du_fichier>\n");
					continue;
				}
				*space = '\0'; // coupe buff en deux : pseudo / chemin
				char *dest = buff + 6;      // le pseudo du destinataire
				char *path = space + 1;     // le chemin complet du fichier

				// check pseudo
				if (client->nickname[0] == '\0') {
					printf("choose a nickname first with /nick\n");
					continue;
				}
				// check si un transfert est deja en cours
				if (client->send_pending) {
					printf("a file transfer is already pending\n");
					continue;
				}
				// check si le fichier existe
				FILE *test = fopen(path, "rb");
				if (test == NULL) {
					perror("fopen");
					continue;
				}
				fclose(test);

				// on prepare le transfert de fichier
				client->send_pending = 1;
				copy_str(client->send_to, sizeof(client->send_to), dest);
				copy_str(client->send_path, sizeof(client->send_path), path);

				// on envoie la demande de transfert de fichier au destinataire
				msg.type = FILE_REQUEST;
				copy_str(msg.infos, sizeof(msg.infos), dest);
				pld = (char *)base_name(path); // on envoie le nom du fichier (pas le chemin complet)
			} else if (buff[0] == '/') { // l'utilisateur a saisi une commande inconnue
				printf("unknown command\n");
				continue;
			} else { // l'utilisateur n'a saisi aucune commande
				msg.type = ECHO_SEND;
				pld = buff;
			}

			// ... et on envoie le message avec eventuellement le payload
			if (pld != NULL) msg.pld_len = strlen(pld);
			if (protocol_send_message(client->sockfd, &msg, pld) != 0) {
				printf("error while sending message\n");
				return -1;
			}
		}

		// Data from server
		if (fds[1].revents & (POLLIN | POLLHUP)) {
			struct message msg_decision;
			char buff_decision[MSG_LEN];
			void *payload = NULL;
			if (protocol_recv_message(client->sockfd, &msg, &payload) != 0) {
				printf("client: server left\n");
				return -1;
			}
			if (msg.type == NICKNAME_NEW && msg.infos[0] != '\0') { // le serveur a accepte notre demande de pseudo
				strcpy(client->nickname, msg.infos); // on l'enregistre ici
			}
			if (msg.type == FILE_REQUEST && msg.infos[0] != '\0' && payload != NULL) { // un utilisateur souhaite nous envoyer un fichier
				printf("%s wants you to accept the transfer of the file named \"%s\". Do you accept? [Y/N]", msg.nick_sender, (char *)payload);
				fflush(stdout);
				/* lire la réponse : tout ce qui n'est pas Y vaut N */
				char answer = 'n';
				if (fgets(buff_decision, sizeof(buff_decision), stdin) != NULL) {
					answer = tolower((unsigned char)buff_decision[0]);
				}

				memset(&msg_decision, 0, sizeof(msg_decision));
				copy_str(msg_decision.nick_sender, sizeof(msg_decision.nick_sender), client->nickname);
				copy_str(msg_decision.infos, sizeof(msg_decision.infos), msg.nick_sender);

				if (answer != 'y') {
					// N ou autre : on refuse le transfert
					msg_decision.type = FILE_REJECT;
					if (protocol_send_message(client->sockfd, &msg_decision, NULL) != 0) return -1;
				} else {
					// y
					char addr_str[64];
					int listen_fd = open_listener(client->sockfd, addr_str, sizeof(addr_str));

					if (listen_fd == -1) {          /* pas de porte ouverte → on refuse */
						msg_decision.type = FILE_REJECT;
						protocol_send_message(client->sockfd, &msg_decision, NULL);
					} else {
						msg_decision.type = FILE_ACCEPT;
						msg_decision.pld_len = strlen(addr_str);
						if (protocol_send_message(client->sockfd, &msg_decision, addr_str) != 0) {
							close(listen_fd);
							return -1;
						}
						file_receive(listen_fd, client->nickname); 
						close(listen_fd); 
					}
				}
			}
			
			if (msg.type == FILE_ACCEPT) {
				printf("%s accepted file transfer\n", msg.nick_sender);
				// verifier que c'est bien le destinataire du fichier qui a accepté
				if (client->send_pending == 0
				    || payload == NULL
				    || strcmp(msg.nick_sender, client->send_to) != 0) {
					printf("unexpected file acceptance ignored\n");
				} else {
					// recuperer l'adresse ip et le port du destinataire
					char *ip = (char *)payload;
					char *colon = strrchr(ip, ':'); // chercher le DERNIER ':' dans ip

					if (colon == NULL) {
						printf("invalid address received\n");
					} else {
						*colon = '\0';              // "127.0.0.1:45678" → "127.0.0.1" \0 "45678" 
						char *port = colon + 1;

						// sur le bon destinataire, on envoie le fichier
						printf("Connecting to %s and sending the file...\n", msg.nick_sender);
						if (file_send(ip, port,client->send_path,client->nickname) != 0) {
							printf("file transfer to %s failed\n", msg.nick_sender);
						}
					}
					// fin
					client->send_pending = 0;
				}
				}
			if (msg.type == FILE_REJECT) {
				printf("%s refused file transfer\n", msg.nick_sender);
				client->send_pending = 0; // on remet send_pending à 0 pour pouvoir envoyer un autre fichier
				
			}
			if (payload != NULL && msg.type != FILE_REQUEST && msg.type != FILE_ACCEPT) {
				printf("[%s] >> %s\n", msg.nick_sender, (char *)payload);
			}
			free(payload);
		}
	}
	return 0;
}

// ferme le socket du client passe en parametre
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

