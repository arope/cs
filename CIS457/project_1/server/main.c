#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PACKET_SIZE 1024

int init_fstream(char* location, FILE** f) {
    *f = fopen(location, "r");
    if (*f == NULL) {
        return errno;
    }
    return EXIT_SUCCESS;
}

int file_size(FILE** f) {
    fseek(*f, 0L, SEEK_END);
    int size = (int) ftell(*f);
    fseek(*f, 0L, SEEK_SET);
    return size;
}

void read_file_by_packet_size(FILE** f, int pack_num, char* contents) {
    int offset = (pack_num * PACKET_SIZE) * sizeof(char);
    fseek(*f, offset, SEEK_SET);
    fread(contents, sizeof(char), PACKET_SIZE, *f);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Invalid argument count. Usage: ./server port");
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    int port =atoi(argv[1]);
    char cli_req[PACKET_SIZE];

    struct sockaddr_storage client;
    struct sockaddr_in server;

    if (sockfd < 0) {
        perror("Failed to create udp socket.");
        return EXIT_FAILURE;
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0) {
        fprintf(stderr, "Error, could not bind to the specified port.");
        return EXIT_FAILURE;
    }

    while(1) {
      socklen_t len = sizeof client;

      int res = recvfrom(sockfd, (char*) cli_req, PACKET_SIZE, MSG_WAITALL, (struct sockaddr*) &client, &len);
      printf("%s", cli_req);

      if (res == -1) {
          fprintf(stderr, "Error, could not receive data from client.");
      } else {
          fprintf(stdout, "Client file request: %s", cli_req);
      }

      // Trying to be efficient, so we open the file stream once, and then close it once.
      FILE* file;
      int file_res = init_fstream(cli_req, &file);

      if (file_res < 0) {
          fprintf(stderr, "File [%s] does not exist.\n", cli_req);
      }

      int size = file_size(&file);
      fprintf(stdout, "File Size: %d", size);

      // Just an example.
      char packet[PACKET_SIZE];
      read_file_by_packet_size(&file, 0, packet);
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
