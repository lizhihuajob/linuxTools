#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <getopt.h>

#define DEFAULT_PORT 10280
#define BUFFER_SIZE 1024 * 1024
#define TEST_DURATION 10

void print_help() {
    printf("Usage: server [options]\n");
    printf("Network bandwidth test server.\n\n");
    printf("Options:\n");
    printf("  -p, --port <port>    Specify port to listen on (default: 10280)\n");
    printf("  -h, --help           Display this help message\n\n");
    printf("Example:\n");
    printf("  server -p 8080\n");
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int opt;
    
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "p:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                print_help();
                return 0;
            case '?':
                print_help();
                return 1;
            default:
                break;
        }
    }
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int optval = 1;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", port);
    printf("Waiting for client connection...\n");
    
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    printf("Client connected: %s\n", inet_ntoa(address.sin_addr));
    
    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 'A', BUFFER_SIZE);
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    long long total_bytes = 0;
    double elapsed = 0.0;
    
    while (elapsed < TEST_DURATION) {
        ssize_t sent = send(new_socket, buffer, BUFFER_SIZE, 0);
        if (sent <= 0) {
            perror("send failed");
            break;
        }
        total_bytes += sent;
        
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    }
    
    double bandwidth = (total_bytes * 8.0) / (elapsed * 1024 * 1024);
    printf("\nTest completed:\n");
    printf("  Total data sent: %.2f MB\n", total_bytes / (1024.0 * 1024.0));
    printf("  Duration: %.2f seconds\n", elapsed);
    printf("  Bandwidth: %.2f Mbps\n", bandwidth);
    
    free(buffer);
    close(new_socket);
    close(server_fd);
    
    return 0;
}
