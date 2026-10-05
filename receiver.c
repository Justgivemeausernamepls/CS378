#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<stdio.h>

#define MY_IP "10.130.154.62"
#define MYPORT 6700

int main(){
int sockfd;
struct sockaddr_in my_addr;
sockfd = socket(PF_INET, SOCK_DGRAM, 0);

my_addr.sin_family = AF_INET; 
my_addr.sin_port = htons(MYPORT); 
my_addr.sin_addr.s_addr = inet_addr(MY_IP);
memset(&(my_addr.sin_zero), '\0', 8);

bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));

struct sockaddr_in dst_addr;

socklen_t addr_len = sizeof(struct sockaddr); 
    
    char A[501];
    
    while(1) {
        int bytes_received=0;
        bytes_received = recvfrom(sockfd, (void*)A, 500, 0, (struct sockaddr *)&dst_addr, &addr_len);
        printf("%d\n",bytes_received);
        if (bytes_received > 0) {
            A[bytes_received] = '\0'; 
            printf("%s\n", A);
        }
    }
}