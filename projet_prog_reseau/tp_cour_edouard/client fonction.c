#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


#define SERVER_PORT 8080


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

int send_next_message_size(int socket, int msg_size){
    int ret = 0;
    int bytes_sent = 0;
    while (bytes_sent != sizeof(int)){
        ret = write(socket, (char *)&msg_size+bytes_sent,sizeof(msg_size)-bytes_sent);
        die(ret,"On writing");
        bytes_sent += ret;
    }
    printf("Taille envoyé \n");
    return 0;
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


int main (){
    // socket
    int client = socket(AF_INET, SOCK_STREAM, 0);
    die(client, "socket creation");

    //config connect
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    printf("%d %d \n",server_addr.sin_port,SERVER_PORT);
    int ret = inet_aton("127.0.0.1", &(server_addr.sin_addr));
    die(client,"fill socket");

    //connect
    ret = connect(client,(struct sockaddr*)&server_addr, sizeof(server_addr));
    die(ret, "connecting");

    char msg[1000000]= {0};
    strcpy(msg,"bonjour");
    int size = sizeof(msg);
    send_next_message_size(client,size);
    send_message(client,&msg,size);



    return 1;
}



// connect

//send/recv

//close