#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h> 
#include <errno.h>

#include "include/msg_struct.h"
#include "include/protocol.h"
#include "include/file_transfert.h"

// permet de trouver le nom de fichier à partir d'un chemin renvoie un pointeur vers le nom de fichier (après le dernier /)
const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');

    if (slash == NULL) {
        return path;
    }
    return slash + 1;
}

// verifie que le nom de fichier est correct (pas de /, pas vide, pas . ou ..)
static int is_safe_name(const char *name)
{
    if (name[0] == '\0') return 0;
    if (strchr(name, '/') != NULL) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    return 1;
}

// connexion
static int connect_to(const char *ip, const char *port)
{
    struct addrinfo hints, *result, *rp; // hints ce que je cherche, result la liste des résultats, rp le résultat courant
    int sfd = -1; // pas de socket

    memset(&hints, 0, sizeof(hints)); // on met à zéro la structure hints
    hints.ai_family = AF_UNSPEC; // IPV6 ou IPV4 on accepte les deux
    hints.ai_socktype = SOCK_STREAM; // TCP

    if (getaddrinfo(ip, port, &hints, &result) != 0) { // voici mes critères, je veux une liste de résultats dans result
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // en réalité on a souvent un seul résultat mais on c'est jamais
        if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
            continue;
        }
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  // success
        }
        close(sfd);
    }
    freeaddrinfo(result);
    if (rp == NULL) {
        return -1;
    }
    return sfd;
}

static int send_chunks(int fd, FILE *file, const char *name, const char *my_nick)
{
    struct message msg; // en-tête du message
    char chunk[CHUNK_SIZE]; // morceau de fichier
    size_t n;
    void *payload = NULL; // payload du message
    int is_ack; // 1 si le récepteur a confirmé, 0 sinon

    // config de lentete
    memset(&msg, 0, sizeof(msg));
    msg.type = FILE_SEND;
    strncpy(msg.nick_sender, my_nick, NICK_LEN - 1);
    strncpy(msg.infos, name, INFOS_LEN - 1);

    // boucle d'envoi des morceaux
    while ((n = fread(chunk, 1, CHUNK_SIZE, file)) > 0) { // fread car on travail avec FILE* et pas avec un fd
        msg.pld_len = n;
        if (protocol_send_message(fd, &msg, chunk) != 0) return -1;
    }
    if (ferror(file)) { perror("fread"); return -1; }

    // envoie du marqueur de fin
    msg.pld_len = 0;
    if (protocol_send_message(fd, &msg, NULL) != 0) return -1;

    // attente de l'ACK
    if (protocol_recv_message(fd, &msg, &payload) != 0) return -1;
    is_ack = (msg.type == FILE_ACK);
    free(payload);              // on libère le payload reçu (même si c'est NULL)
    return is_ack ? 0 : -1;
}


int file_send(const char *ip, const char *port, const char *path, const char *my_nick)
{
    const char *name = base_name(path);
    if (!is_safe_name(name)|| strlen(name) >= INFOS_LEN) { fprintf(stderr, "invalid file name\n"); return -1; } // le nom de fichier ne peut pas être vide
    FILE *file;
    int fd;
    int ret;

    file = fopen(path, "rb");  
    if (file == NULL) { perror("fopen"); return -1; }

    fd = connect_to(ip, port); 
    if (fd == -1) {
        fprintf(stderr, "cannot connect to %s:%s\n", ip, port);
        fclose(file);                                  
        return -1;
    }

    ret = send_chunks(fd, file, name, my_nick); // envoie des blocks

    close(fd);                                         // fin de la connexion
    fclose(file);
    if (ret == 0) printf("%s has been received.\n", name);
    return ret;
}



// ecrit les morceaux dans le fichier, libère le payload, renvoie 0 si OK, -1 sinon
static int write_chunks(int fd, FILE *file, struct message *msg, void *payload)
{
    while (1) {
        if (msg->pld_len == 0) {  // marqueur de fin 
            free(payload);
            return 0;
        }

        // écrire le morceau dans le fichier
        if (fwrite(payload, 1, msg->pld_len, file) != (size_t)msg->pld_len) {
            perror("fwrite");
            free(payload);
            return -1;
        }
        free(payload);
        payload = NULL;

        // recevoir le prochain message
        memset(msg, 0, sizeof(*msg));
        if (protocol_recv_message(fd, msg, &payload) != 0) return -1;  // vérifie le recepteur
        if (msg->type != FILE_SEND) {
            free(payload);
            return -1;
        }
    }
}

// ordonance la réception d'un fichier, renvoie 0 si OK, -1 sinon
static int receive_file(int fd, const char *my_nick)
{
    struct message msg;
    void *payload = NULL;
    char filename[INFOS_LEN];
    char out_path[sizeof(INBOX_DIR) + 1 + INFOS_LEN];   // "inbox" + '/' + nom 
    FILE *file;
    int ret;

    // initialisation des données 
    if (protocol_recv_message(fd, &msg, &payload) != 0) return -1;
    if (msg.type != FILE_SEND) { free(payload); return -1; }

    // vérification des données
    msg.infos[INFOS_LEN - 1] = '\0';   
    msg.nick_sender[NICK_LEN - 1] = '\0';       // sécurité
    if (!is_safe_name(msg.infos)) {
        fprintf(stderr, "unsafe file name refused\n");
        free(payload);
        return -1;
    }
    strcpy(filename, msg.infos); // copie du nom de fichier pour l'utiliser plus tard

    // préparer le terrain pour recevoir
    if (mkdir(INBOX_DIR, 0755) == -1 && errno != EEXIST) {
        perror("mkdir"); free(payload); return -1;
    }

    // créer le fichier de sortie
    snprintf(out_path, sizeof(out_path), "%s/%s", INBOX_DIR, filename); // chemin complet du fichier de sortie
    file = fopen(out_path, "wb");  // ouverture + ecriture 
    if (file == NULL) { perror("fopen"); free(payload); return -1; }

    printf("Receiving %s from %s...\n", filename, msg.nick_sender);

    // ecriture des morceaux dans le fichier
    ret = write_chunks(fd, file, &msg, payload);

    // finir le fichier et vérifier que tout s'est bien passé
    if (fclose(file) != 0) ret = -1;

    if (ret != 0) {// transfert échoué
        remove(out_path);
        fprintf(stderr, "transfer of %s failed\n", filename);
        return -1;
    }
    printf("%s saved in %s\n", filename, out_path);

    // envoyer l'ACK
    memset(&msg, 0, sizeof(msg));
    msg.type = FILE_ACK;
    strcpy(msg.infos, filename);
    strncpy(msg.nick_sender, my_nick, NICK_LEN - 1);
    return protocol_send_message(fd, &msg, NULL);
}

// accepte une connexion entrante et reçoit un fichier, renvoie 0 si OK, -1 sinon
int file_receive(int listen_fd, const char *my_nick)
{
    int fd;
    int ret;

    fd = accept(listen_fd, NULL, NULL);
    if (fd == -1) { perror("accept"); return -1; }

    ret = receive_file(fd, my_nick);

    close(fd);
    return ret;
}