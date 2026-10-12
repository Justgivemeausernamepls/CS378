// FILE IMPORTS
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>

// MACROS
#define PERIODIC_TIME 20 // Seconds
#define FILE_CHECK_TIME 100 // Milliseconds
#define PORT 5000
#define BACKLOG 5

using namespace std;
namespace fs = filesystem;

vector<vector<int>> getDistances(string file) {
    string line;
    vector<vector<int>> distances;
    while (getline(file, line)) {
        size_t firstNonSpace = line.find_first_not_of(" \t\r\n");
        if (firstNonSpace == string::npos || line[firstNonSpace] == "#") {
            continue;
        }
        stringstream ss(line);
        std::vector<int> row;
        int num;
        while (ss >> num) {
            row.push_back(num);
        }
        if (!row.empty()) {
            distances.push_back(row);
        }
    }
    return distances;
}

send_data(vector<vector<int>> distances, vector<int> new_fd){
    // int N = new_fd.size();
    // string msg = "";
    // for (int i = 0; i < N; i++) {
    //     for (int j = 0; j < i; j++) {
    //         if (distances[j][i] != -1) {
    //             msg += ((char)('a' + j), new_fd.ip, new_fd.port, distances[j][i]);
    //         }
    //     }
    //     for (int j = 0; j < new_fd[i].size(); j++) {
    //         if (distances[i][j] != -1) {
    //             msg += ((char)('a' + j + i), new_fd.ip, new_fd.port, distances[i][j]);
    //         }
    //     }
    //     send(new_fd[i] , msg, strlen(msg), 0);
    // }

    return {};
}

int main(int argc, char* argv[] ) {

    if (argc != 1) {
        cout << "Expected 1 argument: file name.";
        return -1;
    }

    string file = argv[0];
    vector<vector<int>> distances = getDistances(file);

    int N = distances.size();
    int sockfd;
    vector<int> new_fd;
    struct sockaddr_in my_addr; // my address information
    struct sockaddr_in their_addr; // connector's address information
    int sin_size;
    my_addr.sin_family = AF_INET; // host byte order
    my_addr.sin_port = htons(PORT); // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // auto-fill with my IP
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

    for (int i = 0; i < N; i++) {
        sockfd = socket(PF_INET, SOCK_STREAM, 0); 
        my_addr.sin_port = htons(PORT + i);
        bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
        listen(sockfd, BACKLOG);
        sin_size = sizeof(struct sockaddr_in);
        new_fd.push_back(accept(sockfd, (struct sockaddr *)&their_addr, &sin_size));
    }

    send_data(distances, new_fd);

    auto last_write_time = fs::last_write_time(file);
    auto poll_interval = chrono::milliseconds(FILE_CHECK_TIME);
    auto report_interval = chrono::seconds(PERIODIC_TIME);
    auto last_report_time = chrono::steady_clock::now();
    auto now = last_report_time;

    while (true) {
        this_thread::sleep_for(poll_interval);
        auto current_write_time = fs::last_write_time(file);
        if (current_write_time != last_write_time) {
            last_write_time = current_write_time;
            distances = getDistances(file);
            send_data(distances, new_fd);
        }
        now = chrono::steady_clock::now();
        if (now - last_report_time >= report_interval) {
            last_report_time = now;
            send_data(distances, new_fd);
        }

    }

}