#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#define CHUNK_SIZE 4096
#define INBOX_DIR "inbox"

int file_send(const char *ip, const char *port, const char *path, const char *my_nick);

int file_receive(int listen_fd, const char *my_nick);

const char *base_name(const char *path);

#endif