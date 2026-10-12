#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include <arpa/inet.h>

#define ON_PORT 5000

int connect_tcp(char* ipaddr){
    int ON_FD=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in A;
    A.sin_family=PF_INET;
    A.sin_port=htons(ON_PORT);
    A.sin_addr.s_addr=inet_addr(ipaddr);
    memset(&(A.sin_zero), '\0', 8); 
    while(1){
        if( connect(ON_FD,(struct sockaddr *)&A,sizeof A)<0 ){continue;}
        break;
    }
    return ON_FD;
}

void send_connect(int fd){
    struct sockaddr_in A;
    socklen_t L=sizeof(A);
    if (getsockname(fd, (struct sockaddr *)&A, &L) == 0) {
        char my_ip[16];
        inet_ntop(AF_INET, (struct sockaddr *)&A.sin_addr, my_ip, sizeof(my_ip));
        char buffer[128];
        int bytes = snprintf(buffer, sizeof(buffer), "%s,%d\n", 
                             my_ip, ntohs(A.sin_port));
        
        send(fd, buffer, bytes, 0); 
    }
}

void receive_data(int fd,char* data) {
    // char data[1024]; 
    memset(data, 0, sizeof(data));
    while(1){
        int bytes_received=0;
        bytes_received = recv(fd, data, sizeof(data) - 1, 0);
        if(bytes_received<=0){continue;}
        data[bytes_received]='\0';
        break;
    }
}

int main(int argc, char* argv[]){
    if(argc!=3){
        printf("WRONG INPUT FORMAT\n");
        return 0;
    }
    char* ON_IP_Addr = argv[1];
    int UDP_PORT=atoi(argv[2]);

    int ON_FD=connect_tcp(ON_IP_Addr);
    printf("ON Connection Done\n");
    send_connect(ON_FD);
    printf("CONNECT Message Sent\n");
    char data[1024];
    while(1){
    receive_data(ON_FD,data);
    printf("%s\n",data);}


}