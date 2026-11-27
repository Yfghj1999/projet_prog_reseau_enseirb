#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "include/msg_struct.h"
#include "include/protocol.h"

#define MAX_RECV 1024


int protocol_send_all(int socket_fd, const void *buffer, size_t length)
{
    size_t bytes_sent = 0;
    ssize_t ret;

    while (bytes_sent < length) {
        ret = write(socket_fd, (char *)buffer + bytes_sent,
                    length - bytes_sent);
        if (ret <= 0) return -1;
        bytes_sent += (size_t)ret;
    }
    return 0;
}
int protocol_recv_all(int socket_fd, void *buffer, size_t length)
{
    size_t bytes_read = 0;
    ssize_t ret;

    while (bytes_read < length) {
        ret = read(socket_fd, (char *)buffer + bytes_read,
                    length - bytes_read);
        if (ret <= 0) return -1;
        bytes_read += (size_t)ret;
    }
    return 0;
}
int protocol_send_message(int socket_fd, const struct message *message,
                          const void *payload)
{
    // Struct
    if (protocol_send_all(socket_fd,message ,sizeof(struct message)) != 0) {
        return -1;
    }

    // Msg
    if (message->pld_len >0) {
        if (protocol_send_all(socket_fd, payload, message->pld_len) != 0) {
            return -1;
        }
    }

    return 0;
}
int protocol_recv_message(int socket_fd, struct message *message,
                          void **payload)
{
    char *buffer;

    *payload = NULL;   /* par défaut : pas de payload */

    // Struct
    if (protocol_recv_all(socket_fd, message, sizeof(struct message)) != 0) {
        return -1;
    }

    // msg
    if (message->pld_len <0 || message->pld_len>MAX_RECV ) {
        return -1;
    }
    if (message->pld_len == 0) {
        return 0;
    }
    buffer = malloc(message->pld_len+1);
    if (buffer == NULL) {
        return -1;
    }
    if (protocol_recv_all(socket_fd,buffer, message->pld_len) != 0) {
        free(buffer);
        return -1;
    }
    buffer[message->pld_len] = '\0';
    *payload = buffer;

    return 0;
}