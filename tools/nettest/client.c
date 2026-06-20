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

void print_help() {
    printf("Usage: client [options] <server_ip>\n");
    printf("Network bandwidth test client.\n\n");
    printf("Options:\n");
    printf("  -p, --port <port>    Specify server port (default: 10280)\n");
    printf("  -h, --help           Display this help message\n\n");
    printf("Example:\n");
    printf("  client -p 8080 192.168.1.100\n");
    printf("  client 192.168.1.100\n");
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    char *server_ip = NULL;
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
    
    if (optind < argc) {
        server_ip = argv[optind];
    } else {
        fprintf(stderr, "Error: Server IP address is required\n\n");
        print_help();
        return 1;
    }
    
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *buffer = (char *)malloc(BUFFER_SIZE);
    
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        exit(EXIT_FAILURE);
    }
    
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("invalid address/ address not supported");
        exit(EXIT_FAILURE);
    }
    
    printf("Connecting to server %s:%d...\n", server_ip, port);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server. Starting bandwidth test...\n");
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    long long total_bytes = 0;
    double elapsed = 0.0;
    
    while (1) {
        ssize_t received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            break;
        }
        total_bytes += received;
        
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    }
    
    double bandwidth = (total_bytes * 8.0) / (elapsed * 1024 * 1024);
    printf("\nTest completed:\n");
    printf("  Total data received: %.2f MB\n", total_bytes / (1024.0 * 1024.0));
    printf("  Duration: %.2f seconds\n", elapsed);
    printf("  Bandwidth: %.2f Mbps\n", bandwidth);
    
    free(buffer);
    close(sock);
    
    return 0;
}
