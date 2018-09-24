#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Data received size
#define PACKET_SIZE 1024

// Frame size is arbitrary
#define FRAME_SIZE 128

// Window size is arbitrary
#define WINDOW_SIZE 5

/** Function prototypes **/
off_t file_size(FILE** f);
FILE* init_fstream(char* location);
char* read_fild(long bytes, FILE** f);
struct packet* construct_packet_transport_queue(off_t size, FILE* file_ptr);

#pragma pack(1)
struct packet {
  // For tracking packet order...
  int packet_number;
  // Holds the contents of the packet
  char data[PACKET_SIZE];
};


FILE* init_fstream(char* location) {
  FILE* fp;

  location[strlen(location) - 1] = 0;
  fp = fopen(location, "r");

  printf("searching...\n");
  if (fp) {
    fprintf(stdout, "file found!\n");
  } else {
    fprintf(stderr, "Error, could not find file: %s\n", location);
  }

  return fp;
}

off_t file_size(FILE** f) {
    fseek(*f, 0L, SEEK_END);
    long size = ftell(*f);
    fseek(*f, 0L, SEEK_SET);

    return size;
}

char* read_file(long bytes, FILE** f) {
    char* buffer = (char*) malloc(bytes* sizeof(char));
    fread(buffer, sizeof(char), bytes, *f);

    return buffer;
}

void read_file_by_packet_size(FILE** f, int pack_num, char* contents) {
    int offset = (pack_num * PACKET_SIZE) * sizeof(char);

    fseek(*f, offset, SEEK_SET);
    fread(contents, sizeof(char), PACKET_SIZE, *f);
}

struct packet* construct_packet_transport_queue(off_t size, FILE* file_ptr) {
    int num_packets = (size / PACKET_SIZE);

    // Set remaining packets to the number we have left
    int packets_remaining = num_packets + 1;

    int buffer_length = packets_remaining;

    // Queue up and tag the next packet
    struct packet* send_queue = (struct packet*) malloc (WINDOW_SIZE * sizeof(struct packet));

    // The packet to be sent
    struct packet current_packet;
    printf("I'M HERE\n");
    while (packets_remaining > 0) {
      if (packets_remaining > WINDOW_SIZE) {
        int i;
        for(i = 0; i < WINDOW_SIZE; i++) {
          // Tag our packets
          current_packet.packet_number = i;
          int offset = (i * PACKET_SIZE) * sizeof(char); // woo

          fseek(file_ptr, offset, SEEK_SET);
          fread(current_packet.data, sizeof(char), PACKET_SIZE, file_ptr);
          printf("Packet data: %s\n", current_packet.data);
          // Store the current packet to our packet queue
          send_queue[i] = current_packet;
        }
      } else { // Our final packet in the window
        // Add remaining data without overshooting and filling our files with trash...
        int i;
        printf("Buffer length: %d\n", buffer_length);
        for (i = 0; i < buffer_length; i++) {// If we're just a little bit over...
          if (size - (i * PACKET_SIZE) > PACKET_SIZE) {
            current_packet.packet_number = i;
            int offset = (i * PACKET_SIZE) * sizeof(char); // woo
            printf("Offset: %d | %d\n", offset, PACKET_SIZE);
            fseek(file_ptr, offset, SEEK_SET);
            fread(current_packet.data, sizeof(char), PACKET_SIZE, file_ptr);
            printf("Packet data: %s\n", current_packet.data);
            send_queue[i] = current_packet;
          } else {
            int diff = size - (num_packets * PACKET_SIZE);

            current_packet.packet_number = i;

            int offset = (i * PACKET_SIZE) * sizeof(char); // woo
            printf("Diff: %d | %d\n", diff, offset);
            fseek(file_ptr, offset, SEEK_SET);
            fread(current_packet.data, sizeof(char), diff, file_ptr);
            printf("Packet data: %s\n", current_packet.data);
            send_queue[i] = current_packet;
          }
        }
      }
      packets_remaining--;
    }

    return send_queue;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Invalid argument count. Usage: ./server port\n");
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    int port = atoi(argv[1]);
    char cli_req[PACKET_SIZE];

    struct sockaddr_storage client;
    struct sockaddr_in server;

    if (sockfd < 0) {
        perror("Failed to create udp socket.\n");
        return EXIT_FAILURE;
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0) {
        fprintf(stderr, "Error, could not bind to the specified port.\n");
        return EXIT_FAILURE;
    }

    while(1) {
      socklen_t len = sizeof client;

      int res = recvfrom(sockfd, (char*) cli_req, PACKET_SIZE, 0, (struct sockaddr*) &client, &len);

      if (res == -1) {
          fprintf(stderr, "Error, could not receive data from client.\n");
      }

      cli_req[res] = 0;

      fprintf(stdout, "Client file request: %s\n", cli_req);

      // Trying to be efficient, so we open the file stream once, and then close it once.
      FILE* file_ptr = init_fstream(cli_req);

      // Acquire file size for proper splitting
      off_t size = file_size(&file_ptr);
      fprintf(stdout, "File Size: %li bytes\n", size);

      // Store the data into a content buffer
      char* file_contents = read_file(size, &file_ptr);

      /**********************************
       * Begin sliding window transfer
       **********************************/

      // Begin the sliding window transfer of data
      int num_packets = (size / PACKET_SIZE);

      // Set remaining packets to the number we have left
      int packets_remaining = num_packets + 1;
      printf("Packet total: %d\n", packets_remaining);

      // Initialize our packets to be sent over the wire
      struct packet* packet_queue = construct_packet_transport_queue(size, file_ptr);

      int ack = 0;
      int buffer_length = WINDOW_SIZE;

      // Send number of packets to receive
      sendto(sockfd, &packets_remaining, sizeof(int), 0, (struct sockaddr*) &client, sizeof client);
      fprintf(stdout, "Client should receive %d packets.\n", packets_remaining);

      while (packets_remaining > 0) {
        int i;
        int packets_to_send = packets_remaining;
        socklen_t len = sizeof client;

        // Incrementally send each packet in the window
        for (i = 0; i < packets_to_send; i++) {
          sendto(sockfd, &packet_queue[i], sizeof(struct packet) + 1, MSG_CONFIRM, (struct sockaddr*) &client, len);
        }

        packets_remaining--;

        // Receive acks
        /* int received = recvfrom(sockfd, &ack, sizeof(int), MSG_WAITALL, (struct sockaddr*) &client, &len); */
        /* if (received == -1) { */
        /*   fprintf(stderr, "Failed to receive ack reply from client.\n"); */
        /*   printf("Packet dropped.\n"); */
        /* } */

        /* if (ack < buffer_length) { */
        /*   packets_remaining -= ack; */
        /* } else { */
        /*   fprintf(stdout, "\n\nAll packets sent successfully.\n"); */
        /*   packets_remaining -= buffer_length; */
        /* } */

        fprintf(stdout, "Remaining packets: %d\n", packets_remaining);
      }

      fclose(file_ptr);
      free(packet_queue);
    }

  return EXIT_SUCCESS;
}