#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>


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

    // char msg[1000000]= {0};
    // strcpy(msg,"bonjour");
    // int size = sizeof(msg);
    // send_next_message_size(client,size);
    // send_message(client,&msg,size);

    // poll pour stdin ou socket de reception
    struct pollfd fds[2]={0};
    fds[0].fd = client;
    fds[0].events=POLLIN;
    fds[0].revents=0;
    
    fds[1].fd = 0;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    while(1){
        printf("Will poll ... \n");
        int nfds = poll(fds, 2, -1);
        printf ("%d fd active \n", nfds);
        for (int i = 0 ; i<2;i++){
                    // If fd == listen && est ce que activité
                    if (fds[i].fd == client && (fds[i].revents & POLLIN)){
                        // on m'a répondu ici
                        struct bank tmp;    
                        ret = recv_message(client, &tmp, sizeof(tmp));
                        printf("Le message reçu est : %s\n",tmp.name);  
                        fds[i].revents=0;
                    }
                        // should set back revents to 0
                    else if (fds[i].fd == 0 && (fds[i].revents & POLLIN)){
                        // on me demande d'envoyer le message ecrit au clavier
                        char buf[128];
                        read(0,buf,sizeof(buf));
                        struct bank msg_to_send;
                        strcpy(msg_to_send.name, buf);
                        fds[i].revents=0;
                        ret = send_message(client, (void *)&msg_to_send,sizeof(struct bank));
                    }
                    else if (fds[i].fd == client && (fds[i].revents & POLLHUP)){
                        printf("on ferme suite appel server\n");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        fds[i].events = 0;
                        fds[i].revents = 0;        
                    }

    }

    
}
return 1;
}



// connect

//send/recv

//close