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

int recv_http(int sockfd, char *buf, int size){
    int ret = 0;
    int bytes_read = 0;
    while (1){
        int place = size - 1 - bytes_read;   // -1 pour garder la place du '\0'
        if (place == 0){
            return -1;                       // buffer plein, pas de \r\n\r\n
        }
        ret = read(sockfd, buf + bytes_read, place);
        if (ret == 0){
            return 0;                        // client fermé
        }
        die(ret, "On reading");
        bytes_read += ret;
        buf[bytes_read] = '\0';
        if (strstr(buf, "\r\n\r\n") != NULL){
            break;                           // fin des en-têtes trouvée
        }
    }
    printf("Requete HTTP reçue \n");
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
    int opt = 1;
    setsockopt(server, SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

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
        poll(fds, 20, -1);
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
                char buf[4096]= {0};    
                ret = recv_http(fds[i].fd, buf, sizeof(buf));
                if (ret !=0){
                    printf("Requete \n %s\n",buf);
                    // renvoyer 200 au client
                    char *body = 
                    "<!DOCTYPE html>"
                    "<html>"
                    "<body>"
                    "<h1>HELLO !</h1>"
                    "</body>"
                    "</html>";

                    char reponse[512];
                    int len = snprintf(reponse, sizeof(reponse),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "%s",
                        strlen(body), body);

                    send_message(fds[i].fd, reponse, len);
                }
                else{
                    printf("on ferme suite mort client\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    fds[i].revents = 0; 
                    // close et maj tab
                }
            }
        }
    }

    return 0;
}



// connect

//send/recv

//close