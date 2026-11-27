#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#define CHUNK_SIZE 4096          /* taille d'un morceau : doit être ≤ la limite du protocole */
#define INBOX_DIR  "inbox"       /* dossier où le récepteur range les fichiers */

/* Émetteur : se connecte à ip:port, envoie le fichier par morceaux,
   attend le FILE_ACK. Renvoie 0 si le récepteur a confirmé, -1 sinon. */
int file_send(const char *ip, const char *port, const char *path, const char *my_nick);

/* Récepteur : accepte UNE connexion sur listen_fd, reçoit le fichier,
   l'écrit dans INBOX_DIR, envoie FILE_ACK. Renvoie 0 si OK, -1 sinon. */
int file_receive(int listen_fd, const char *my_nick);

#endif