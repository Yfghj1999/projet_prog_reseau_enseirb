#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>


#define SERVER_PORT 8080
#define BACKLOG 20


struct bank
{
    int account;
    char name[20];
};


int die(int ret_value,char * msg){
    if(ret_value<0){
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return 0;
}

int recv_message_size(int sockfd){
    int ret = 0;
    int bytes_read = 0;
    int size = 0 ;
    while (bytes_read != sizeof(int)){
        ret = read(sockfd, (char *)&size+bytes_read,sizeof(int)-bytes_read);
        die(ret,"On writing");
        bytes_read += ret;
        }
    printf("Taille reçu \n");
    return size;

}
int send_message(int socket, void * buf, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    printf("envoie du msg\n");
    while (bytes_sent != msg_size){
        ret = write(socket, buf+bytes_sent, msg_size-bytes_sent);
        die(ret,"On writing");
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
        die(ret,"On reading");
        bytes_read += ret;
    }
    printf("Message reçu \n");
    return bytes_read;


}

int main (){
    // socket
    int server = socket(AF_INET, SOCK_STREAM, 0);
    die(server, "socket creation");

    //config bind
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(SERVER_PORT);
    listen_addr.sin_addr.s_addr =  INADDR_ANY;
    //  bind associe port/ip 
    int ret = bind(server,(struct sockaddr*)&listen_addr,sizeof(listen_addr));
    die(ret,"on binding");

    // listen
    listen(server, BACKLOG);
    die(ret,"on listening");

    struct pollfd fds[20]={0};
    for (int i = 1; i<20;i++){
        fds[i].fd = -1;
        fds[i].events=0;
        fds[i].revents=0;
    }
    fds[0].fd = server;
    fds[0].events = POLLIN;
    fds[0].revents = 0;


    while (1){
        printf("Will poll ... \n");
        int nfds = poll(fds, 20, -1);
        printf ("%d fd active \n", nfds);
        for (int i = 0 ; i<20;i++){
            // If fd == listen && est ce que activité
            if (fds[i].fd == server && (fds[i].revents & POLLIN)){
                // ACCEPT will creat new fd // should insert new fd into fds
                    // //accept
                struct sockaddr_in client_addr;
                socklen_t size_addr = sizeof(client_addr);

                printf("will block on accept\n");
                int new_fd = accept(server,(struct sockaddr*)&client_addr,&size_addr);

                for (int i = 1 ; i<20;i++){
                    if(fds[i].fd == -1){
                        fds[i].fd = new_fd;
                        fds[i].events = POLLIN | POLLHUP;
                        fds[i].revents = 0;
                        printf("maj tab ok \n");
                        break;
                    }
                }
                // should set back revents to 0
            } else if (fds[i].fd != server && (fds[i].revents & POLLIN)){
                // READ user requests // IF user is 0 bytes should close
                // printf
                // respond to user
                struct bank tmp;    
                ret = recv_message(fds[i].fd, &tmp, sizeof(tmp));
                if (ret !=0){
                    printf("New struct bank is %d %s\n",tmp.account,tmp.name);
                    struct bank account0;
                    strcpy(account0.name, "Bien reçu");
                    account0.account = 00000;
                    ret = send_message(fds[i].fd, (void *)&account0,sizeof(struct bank));
                }
                else{
                    printf("on ferme suite mort client\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0; 
                    // close et maj tab
                }
            } else if (fds[i].fd != server && (fds[i].revents & POLLHUP)){
                printf("on ferme suite appel client\n");
                close(fds[i].fd);
                fds[i].fd = -1;
                fds[i].events = 0;
                fds[i].revents = 0;

                // close fd
                // free entry in fds
            }
        }
    }



    // //accept
    // struct sockaddr_in client_addr;
    // socklen_t size_addr = sizeof(client_addr);

    // printf("will block on accept\n");
    // int new_socket = accept(server,(struct sockaddr*)&client_addr,&size_addr);
    // die(new_socket,"on accept");
    // printf("New socket on fd (%d)\n",new_socket);

    // // int size = recv_message_size(new_socket);

    // // char * storage = malloc(size + 1);       // +1 : une case de plus pour le '\0'
    // // int msg = recv_message(new_socket, storage, size);


    // // if (msg > 0){
    // //     storage[msg] = '\0';                 // on termine la chaîne nous-mêmes
    // //     printf("Contenu : %s\n", storage);
    // // }
    // struct bank tmp;
    // recv_message(new_socket, &tmp, sizeof(tmp));
    // printf("New struct bank is %d %s\n",tmp.account,tmp.name);
    return 0;
}



// connect

//send/recv

//close