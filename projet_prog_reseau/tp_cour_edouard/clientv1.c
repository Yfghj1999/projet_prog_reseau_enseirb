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

    // char * msg = "Bonjour";
    // int msg_size = strlen(msg);
    // char msg[1000000]={0};
    // int msg_size = 1000000;
    int bytes_sent = 0;
    struct bank account0 = {123456,"John Doe"};
    struct bank * msg = &account0;
    int msg_size = sizeof(struct bank);
    while (bytes_sent != msg_size){
        ret = write(client, msg+bytes_sent, msg_size-bytes_sent);
        die(ret,"On writing");
        bytes_sent += ret;
    }

    // // first send next message size
    // while (bytes_sent != sizeof(int)){
    // ret = write(client, (char *)&msg_size+bytes_sent,sizeof(msg_size)-bytes_sent);
    // die(ret,"On writing");
    // bytes_sent += ret;
    // }
    // bytes_sent =0;
    // printf("Next msg size is SENT(%d)\n",msg_size);


    // // the send msg itself

    // // ret = write(client, msg, msg_size); // ecrit dans la socket 
    // // printf("%d bytes send \n",ret);
    // while (bytes_sent != msg_size){
    //     ret = write(client, msg+bytes_sent, msg_size-bytes_sent);
    //     die(ret,"On writing");
    //     bytes_sent += ret;
    // }
    
    


    return 1;
}



// connect

//send/recv

//close