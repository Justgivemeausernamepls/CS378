#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#define MYIP "10.130.154.63"
#define DEST_IP "10.130.154.62"
#define MYPORT 6700
#define DEST_PORT 6700

int main() {

int sockfd;
struct sockaddr_in my_addr;
sockfd = socket(PF_INET, SOCK_DGRAM, 0);

my_addr.sin_family = AF_INET; // host byte order
my_addr.sin_port = htons(MYPORT); // short, network byte order
my_addr.sin_addr.s_addr = inet_addr(MYIP);
memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct
bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));


struct sockaddr_in dest_addr;
dest_addr.sin_family = AF_INET; // host byte order
dest_addr.sin_port = htons(DEST_PORT); // short, network byte order
dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);
memset(&(dest_addr.sin_zero), '\0', 8); // zero the rest of the struct

char* msg = "Networks is definitely my favorite course.";

int bytes_sent = sendto(sockfd, (void*)msg, strlen(msg), 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
printf("%d\n", bytes_sent);

}