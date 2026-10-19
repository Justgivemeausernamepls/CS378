# IMPORTS ._.
import socket
import select
import struct
import os
from datetime import datetime
import time
import sys
 
# GLOBAL VARIABLES ._.
check_interval = 3
select_timeout = 1
 
# HELPER FUNCTIONS ._.
class ConnectionClosedError(Exception):
    pass
 
def read_config_file(filename):
    data_lines = []
    with open(filename, "r") as file:
        for line in file:
            line = line.strip()
            if not line or line.startswith("#"): # Ignore these lines
                continue
            row = []
            for x in line.split():
                row.append(int(x))
            data_lines.append(row)
    return data_lines
 
def get_distances_from_data(data_lines):
    distances = []
    N = len(data_lines) + 1
    for i in range(N):
        row = [0]*N
        distances.append(row)
    for i in range(N-1):
        for j in range(len(data_lines[i])):
            distances[i][i+j+1] = data_lines[i][j]
            distances[i+j+1][i] = data_lines[i][j]
    return distances
 
def get_distances(filename):
    data_lines = read_config_file(filename)
    distances = get_distances_from_data(data_lines)
    return distances
 
def make_server_socket(port=5000):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('0.0.0.0', port)) # Listen to all IP addresses on the machine (loop back, wifi, etc.)
    server_socket.listen(10)
    return server_socket
 
def send_messages(N, server_socket, inputs, vn_connections, sockets, distances, filename):
    for s in inputs:
        if s == server_socket:
            continue
        node_num = vn_connections[s]["node_num"]
        node_char = vn_connections[s]["node_char"]
        node_ip = vn_connections[s]["udp_ip"]
        node_port = vn_connections[s]["udp_port"]
        tuples_to_send = []
        for j in range(N):
            if distances[node_num][j] != -1:
                target_socket = sockets[j]
                target_info = vn_connections[target_socket]
                cost = distances[node_num][j]
                tuples_to_send.append((target_info["node_char"], target_info["udp_ip"], target_info["udp_port"], cost))
        message_bytes = b"" # empty
        for t_char, t_ip, t_port, t_cost in tuples_to_send:
            ip_bytes = socket.inet_aton(t_ip)
            message_bytes += struct.pack("!c 4s H i", t_char.encode('ascii'), ip_bytes, t_port, t_cost) # char, short, H?, integer
        s.sendall(message_bytes)
 
def run_oracle_node(N, distances, filename, port=5000):
    server_socket = make_server_socket(port)
    inputs = [server_socket]
    vn_connections = {}
    connected_count = 0
    conn_msgs = 0
    sockets = []
    connections_finished = False
    last_sent = -1
    last_file_check = -1
    print("ON started on port " + str(port) + ". Expecting " + str(N) + " connections!!")
    while True:
        if connections_finished == False and conn_msgs == N: 
            connections_finished = True
            last_file_check = time.time() - check_interval
            print("Received the required " + str(N) + " connections. No more connections!" )
        if connections_finished == True:
            current_time = time.time()
            if current_time - last_file_check >= check_interval:
                last_file_check = current_time
                timestamp = os.path.getmtime(filename)
                if timestamp != last_sent:
                    last_sent = timestamp
                    distances = get_distances(filename)
                    send_messages(N, server_socket, inputs, vn_connections, sockets, distances, filename)
        readable, _, _ = select.select(inputs, [], [], select_timeout)
        for s in readable:
            if s is server_socket:
                # Case1:- New connection request 
                sock, addr = s.accept()
                if connected_count < N:
                    # Valid connection request 
                    inputs.append(sock)
                    connected_count += 1
                    vn_connections[sock] = {"node_char": chr(ord('A') + connected_count - 1), "node_num": connected_count - 1}
                    sockets.append(sock)
                else:
                    # Invalid connection request (required connections are done)
                    print("Already accepted the required number of connections. Goodbye!")
                    sock.close()     
            else:
                # Case2:- Connection messages incoming
                try:
                    data = s.recv(64)
                    if not data:
                        raise ConnectionClosedError("Something went wrong!?")
                    else:
                        ip_bytes, udp_port = struct.unpack("!4sH", data) # short, H?
                        udp_ip = socket.inet_ntoa(ip_bytes)
                        vn_connections[s]["udp_ip"] = udp_ip
                        vn_connections[s]["udp_port"] = udp_port
                        vn_connections[s]["connected"] = True
                        conn_msgs += 1
                except Exception as e:
                    inputs.remove(s)
                    s.close()
                    if s in vn_connections:
                        del vn_connections[s]               
    for s in inputs:
        s.close()
 
# MAIN CODE ._.
if len(sys.argv) != 2 :
    print("Expected <filename>. Goodbye!")
else :
    filename = sys.argv[1]
    distances = get_distances(filename)
    N = len(distances)
    run_oracle_node(N, distances, filename, 5000)