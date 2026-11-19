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

    struct bank account1;
    int bytes_read = sizeof(struct bank);
    ret = read(new_socket, &account1, bytes_read);
    printf("%d byte read \n",ret);
    printf("Account: %d, Name: %s\n", account1.account, account1.name);

    // char msg[128]={0};
    // int bytes_read = 7;
    // ret = read(new_socket, msg,7);
    // printf("%d byte read \n",ret);

    // char msg[1000000]= {0};
    // int bytes_read=0;
    // int to_read=0;


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



    return 1;
}



// connect

//send/recv

//close