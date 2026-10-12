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
    ifstream input(file);

    while (getline(input, line)) {
        size_t firstNonSpace = line.find_first_not_of(" \t\r\n");
        if (firstNonSpace == string::npos || line[firstNonSpace] == '#') {
            continue;
        }
        stringstream ss(line);
        vector<int> row;
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

void send_data(vector<vector<int>> distances, vector<int> new_fd) {
    int N = new_fd.size();
    string msg;

    for (int i = 0; i < N; i++) {
        msg.clear();
        for (int j = 0; j < N; j++) {
            if (i < (int)distances.size() &&
                j < (int)distances[i].size() &&
                distances[i][j] != -1) {
                msg += to_string(j) + " " +
                       to_string(distances[i][j]) + "\n";
            }
        }

        if (!msg.empty()) {
            ssize_t sent = send(new_fd[i], msg.c_str(), msg.size(), 0);

            if (sent == -1) {
                cerr << "Error sending data to client " << i << ": "
                     << strerror(errno) << '\n';
            }
        }
    }
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cout << "Expected 1 argument: file name.\n";
        return -1;
    }

    string file = argv[1];
    vector<vector<int>> distances = getDistances(file);
    cout << distances[0][0] << "\n";

    if (!fs::exists(file)) {
        cerr << "File does not exist: " << file << '\n';
        return -1;
    }

    int N = distances.size() + 1;
    N = 1; // TO BE REMOVED LATER!!
    vector<int> new_fd;
    vector<int> sockfds;

    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;

    for (int i = 0; i < N; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd == -1) {
            perror("socket");
            return -1;
        }

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        my_addr.sin_port = htons(PORT + i);

        if (bind(sockfd, (struct sockaddr *)&my_addr,
                 sizeof(my_addr)) == -1) {
            perror("bind");
            close(sockfd);
            return -1;
        }

        if (listen(sockfd, BACKLOG) == -1) {
            perror("listen");
            close(sockfd);
            return -1;
        }

        socklen_t sin_size = sizeof(their_addr);

        int client_fd = accept(sockfd, (struct sockaddr *)&their_addr,
                               &sin_size);

        if (client_fd == -1) {
            perror("accept");
            close(sockfd);
            return -1;
        }

        sockfds.push_back(sockfd);
        new_fd.push_back(client_fd);
    }

    send_data(distances, new_fd);

    auto last_write_time = fs::last_write_time(file);
    auto poll_interval = chrono::milliseconds(FILE_CHECK_TIME);
    auto report_interval = chrono::seconds(PERIODIC_TIME);
    auto last_report_time = chrono::steady_clock::now();

    while (true) {
        this_thread::sleep_for(poll_interval);

        auto current_write_time = fs::last_write_time(file);

        if (current_write_time != last_write_time) {
            last_write_time = current_write_time;
            distances = getDistances(file);
            send_data(distances, new_fd);
        }

        auto now = chrono::steady_clock::now();

        if (now - last_report_time >= report_interval) {
            last_report_time = now;
            send_data(distances, new_fd);
        }
    }

    for (int fd : new_fd) {
        close(fd);
    }

    for (int fd : sockfds) {
        close(fd);
    }

    return 0;
}