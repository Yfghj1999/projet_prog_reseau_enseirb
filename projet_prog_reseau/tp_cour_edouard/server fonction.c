#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>


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

    // // first recv size msg
    // while (bytes_read != sizeof(int)){
    // ret = read(new_socket, (char *)&to_read+bytes_read,sizeof(int)-bytes_read);
    // die(ret,"On writing");
    // bytes_read += ret;
    // }

    // // read the msg itself
    // bytes_read = 0;
    // printf("Next msg size is recv(%d)\n",to_read);
    // char * msg = malloc(sizeof(char)*to_read);

    // while (bytes_read !=7){
    //     ret = read(new_socket, msg+bytes_read, 7-bytes_read);
    //     die(ret,"On reading");
    //     bytes_read += ret;
    // }
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
int recv_message(int sockfd, void * buf,int size){
    int ret = 0;
    int bytes_read = 0;
    while (bytes_read !=size){
        ret = read(sockfd, (char *)buf+bytes_read, size-bytes_read);
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

    //accept
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(client_addr);

    printf("will block on accept\n");
    int new_socket = accept(server,(struct sockaddr*)&client_addr,&size_addr);
    die(new_socket,"on accept");
    printf("New socket on fd (%d)\n",new_socket);

    int size = recv_message_size(new_socket);

    char * storage = malloc(size + 1);       // +1 : une case de plus pour le '\0'
    int msg = recv_message(new_socket, storage, size);

    if (msg > 0){
        storage[msg] = '\0';                 // on termine la chaîne nous-mêmes
        printf("Contenu : %s\n", storage);
    }
    return 0;
}



// connect

//send/recv

//close