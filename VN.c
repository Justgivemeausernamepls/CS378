#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<unordered_map>
#include<string>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/select.h>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

#define ON_PORT 5000

int connect_tcp(char* ipaddr){
    int ON_FD=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in A;
    A.sin_family=PF_INET;
    A.sin_port=htons(ON_PORT);
    A.sin_addr.s_addr=inet_addr(ipaddr);
    memset(&(A.sin_zero), '\0', 8); 
    while(1){
        if(connect(ON_FD,(struct sockaddr *)&A,sizeof A)<0 ){continue;}
        break;
    }
    return ON_FD;
}

void send_connect(int fd,int UDP_PORT){
    struct sockaddr_in A;
    socklen_t L=sizeof(A);
    if (getsockname(fd, (struct sockaddr *)&A, &L) == 0) {

        char buffer[6];
memcpy(buffer, &A.sin_addr.s_addr, 4);      // already network order
uint16_t p = htons(UDP_PORT);
memcpy(buffer + 4, &p, 2);
send(fd, buffer, 6, 0);

    }
}

int receive_initial_data(int fd,char* data) {
    memset(data, 0, sizeof(data));
    while(1){
        int bytes_received=0;
        bytes_received = recv(fd, data, 1000, 0);
        if(bytes_received<=0){continue;}
        data[bytes_received]='\0';
        return bytes_received;
    }
    return 0;
}

struct node_info{
    char ip_addr[4];
    int udp_port;
    int cost;
};
unordered_map<int,node_info>neighbour_info;
int my_id=-1;

string ip_to_string(char* ip) {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, ip, buf, sizeof(buf));
    return string(buf);
}

void process_data(char* data, int total_bytes){
    int N=total_bytes/11;
    for(int i=0;i<N;i++){
        int offset=11*i;
        char* t = data+offset;
        char c = *((char *)(data+offset));
        struct node_info A;
        memcpy(A.ip_addr,t+1,4);
        uint16_t p;  
        memcpy(&p,t+5,2);
        A.udp_port=ntohs(p);
        uint32_t cr;
        memcpy(&cr,t+7,4);
        A.cost=(int)ntohl(cr);
        if(A.cost==0){my_id=c-'A';}
        else{
        neighbour_info[c-'A']=A;
        }
    }
}
void display_neighbour_info(){
    printf("------------------\n");
    printf("I am Node = %c \n",(char)('A'+my_id));
    for(auto [x,y] : neighbour_info){
        printf("Node =  %c IP-Addr =  %s UDP-Port = %d Cost = %d \n",(char)(x+'A'),ip_to_string(y.ip_addr).c_str(),y.udp_port,y.cost);
    }
    printf("------------------\n");

}

#define LSP_PERIOD 6  
#define LSP_COPIES 3 
int udp_fd=-1;
uint32_t curr_seq_no=0;
map<int, map<int,int>> graph;
map<int, uint32_t> seq_storage; 

// remove every edge touching 'id', in both directions
void clear_node(int id){
    graph[id].clear();
    for(auto& [n, row] : graph)
        row.erase(id);
}

void update_my_graph(){
    clear_node(my_id);

    for(auto& [x, y] : neighbour_info){
        graph[my_id][x] = y.cost;
    }
}

int make_udp_socket(int port){
    int fd=socket(AF_INET,SOCK_DGRAM,0);
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a,0,sizeof a);
    a.sin_family=AF_INET;
    a.sin_addr.s_addr=htonl(INADDR_ANY);
    a.sin_port=htons(port);
    if(bind(fd, (struct sockaddr*)&a, sizeof a) < 0){
        fprintf(stderr, "Failed to bind UDP port %d: ", port);
        perror("");
        exit(1);
    }
    return fd;
}

void send_to_neighbours(const uint8_t* buf, int len, int exclude_id){
    for(auto [x,y] : neighbour_info){
        if(x == exclude_id) continue;
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        memcpy(&a.sin_addr.s_addr, y.ip_addr, 4);
        a.sin_port = htons(y.udp_port);
        for(int r = 0; r < LSP_COPIES; r++)
            sendto(udp_fd, buf, len, 0, (struct sockaddr*)&a, sizeof a);
    }
}

void handle_lsp(const uint8_t* buf, int len, const struct sockaddr_in* src){
    int origin = buf[0] - 'A';
    if(origin < 0 || origin >= 26) return;
    uint32_t seq; 
    memcpy(&seq, buf + 1, 4); 
    seq = ntohl(seq);
    int count = buf[5];
    if(len < 6 + 5 * count) return;               
    if(origin == my_id) return;                    // my own LSP came back
 
    auto it = seq_storage.find(origin);
    if(it != seq_storage.end() && seq <= it->second) return;   // old and duplicate
    seq_storage[origin] = seq;
    clear_node(origin);

for(int i = 0; i < count; i++){
    const uint8_t* e = buf + 6 + 5 * i;
    uint32_t c; memcpy(&c, e + 1, 4);
    int v = e[0] - 'A';
    graph[origin][v] = (int)ntohl(c);
    graph[v][origin] = (int)ntohl(c);
}

 
    int from = -1;
    for(auto [x,y] : neighbour_info){
        if(memcmp(y.ip_addr, &src->sin_addr.s_addr, 4) == 0 &&
           y.udp_port == ntohs(src->sin_port)){ from = x; break; }
    }
    send_to_neighbours(buf, len, from);
}

void send_lsp(){
    vector<uint8_t> buf(6+5*neighbour_info.size());
    buf[0] = (uint8_t)('A'+my_id);
    uint32_t seq = htonl(++curr_seq_no);
    memcpy(&buf[1], &seq, 4);
    buf[5] = (uint8_t)neighbour_info.size();
    graph[my_id].clear();
    size_t off = 6;
    for(auto& [x,y] : neighbour_info){
        buf[off] = (uint8_t)('A' + x);
        uint32_t c = htonl((uint32_t)y.cost);
        memcpy(&buf[off + 1], &c, 4);
        off += 5;
        graph[my_id][x] = y.cost;
        graph[x][my_id] = y.cost;
    }
    send_to_neighbours(buf.data(), buf.size(), -1);
}


int find_cost(map<int, map<int,int>>& graph, int i, int j){
    auto a = graph.find(i);
        if(a != graph.end()){ auto b = a->second.find(j); if(b != a->second.end()) return b->second; }
        auto c = graph.find(j);
        if(c != graph.end()){ auto d = c->second.find(i); if(d != c->second.end()) return d->second; }
        return -1;
}

void show_graph(){

    int maxid = my_id;
    for(auto& g : graph){
        maxid = max(maxid, g.first);
        for(auto& e : g.second) maxid = max(maxid, e.first);
    }
    int N = maxid + 1;
    printf("---- Known graph (%d nodes) ----\n", N);

    for(int i = 0; i < N - 1; i++){
        for(int j = i + 1; j < N; j++)
             printf("%d ", find_cost(graph,i, j));
        printf("\n");
    }

    printf("--------------------------------\n");
}


int main(int argc, char* argv[]){

    if(argc!=3){
        printf("Expected Input Format : ./VN ON-IP-Address UDP-PORT\n");
        return 0;
    }
    char* ON_IP_Addr = argv[1];
    int UDP_PORT=atoi(argv[2]);
    udp_fd=make_udp_socket(UDP_PORT);
    int ON_FD=connect_tcp(ON_IP_Addr);

    printf("Connected to ON\n");
    send_connect(ON_FD,UDP_PORT);
    printf("CONNECT Message Sent\n");
    char data[1024];
    int bytes_received=receive_initial_data(ON_FD,data);
    
    process_data(data,bytes_received);
    display_neighbour_info();
    update_my_graph();
    time_t last_lsp=0;
    while(1){

                fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ON_FD, &rfds);
        FD_SET(udp_fd, &rfds);
        int maxfd = max(ON_FD, udp_fd) + 1;
        struct timeval tv = {1, 0};
 
        if(select(maxfd, &rfds, NULL, NULL, &tv) > 0){
            // 2) new LINK-STATE from ON 
            if(FD_ISSET(ON_FD, &rfds)){
                char buf[1024];
                int n = recv(ON_FD, buf, sizeof buf, 0);
                if(n <= 0){ }
                else{
                           // drop stale neighbours
                
                for(auto& [x, y] : neighbour_info){
                    graph[x][my_id] = -1;
                    graph[my_id][x] = -1;
                }

                neighbour_info.clear(); 
                process_data(buf, n);
                display_neighbour_info();
                update_my_graph();
                curr_seq_no+=5;
                
                send_lsp();                        // send immediately on change
                last_lsp = time(NULL);
            }
            }
            // 3) incoming LSP from a neighbour
            if(FD_ISSET(udp_fd, &rfds)){
                uint8_t buf[2048];
                struct sockaddr_in src;
                socklen_t sl = sizeof src;
                ssize_t n = recvfrom(udp_fd, buf, sizeof buf, 0, (struct sockaddr*)&src, &sl);
                if(n > 0) handle_lsp(buf, (size_t)n, &src);
            }
        }

        if(time(NULL) - last_lsp >= LSP_PERIOD){
            send_lsp();
            last_lsp = time(NULL);
            show_graph();
        }


    }



}