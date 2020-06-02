#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>



static int asp_socket_fd = -1;


#define SERVERPORT 1234
#define LOCALPORT 1235
#define MAXLINE 1024
#define BUFFER_SIZE 1024


// /* prototypes */
// void die(const char *);
// void pdie(const char *);


int main(){

    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Setup ACK, Session: 1";
    struct sockaddr_in servaddr, cliaddr;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    //servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(SERVERPORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.2");

    cliaddr.sin_family = AF_INET; // IPv4
    cliaddr.sin_port = htons(LOCALPORT);
    cliaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    char ser_str[128];
    // now get it back and print it
    inet_ntop(AF_INET, &(servaddr.sin_addr), ser_str, INET_ADDRSTRLEN);
    printf(" ServerAddr: %s\n", ser_str); // prints "192.0.2.33"


    int n;
    socklen_t len = sizeof(cliaddr);  //len is value/resuslt

    if (connect (sockfd, (const struct sockaddr *)&cliaddr,
            sizeof(cliaddr))){
              perror("connect");
              close(sockfd);
              exit(1);
            }

    fd_set rfds, wfds;
    struct timeval tv;


    n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                &len);
    buffer[n] = '\0';

    printf("Client : %s\n", buffer);



    char sin_str[124];
    // now get it back and print it
    inet_ntop(AF_INET, &(cliaddr.sin_addr), sin_str, INET_ADDRSTRLEN);
    printf(" Clientaddr: %s\n", sin_str); // prints "192.0.2.33"


    uint8_t* packetbuffer = malloc(4);
    uint8_t* recievebuffer = malloc(4);

    printf("Start send wait\n");

    size_t recievelen;
    int rn;
    uint32_t i = 0;
    while (i < 1000000){

      //
      // tv.tv_sec = 3;
      // tv.tv_usec = 0;
      //
      // FD_ZERO(&rfds);
      // FD_SET(sockfd, &rfds);
      //
      // FD_ZERO(&wfds);
      // FD_SET(sockfd, &wfds);
      //
      // int preretval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
      // //printf("retval: %i\n", preretval);
      // if (preretval <= 0){
      //     printf("preretval <= 0: %i\n", preretval);
      //     continue;
      // }
      // else if (FD_ISSET(sockfd, &rfds)){
      //   rn = recvfrom(sockfd, (char *)recievebuffer, 4,
      //               MSG_WAITALL, ( struct sockaddr *) &cliaddr,
      //               &recievelen);
      //   if (rn <= 0){
      //     printf("RECIEVEERROR: %i\n", rn);
      //   }
      //
      //   uint32_t num = recievebuffer[0] | (recievebuffer[1] << 8) | (recievebuffer[2] << 16) | (recievebuffer[3] << 24);
      //   printf("Returnednum: %u\n", num);
      //   if (i != num+1){
      //     printf("setting i = num+1; i: %u, num: %u\n", i, num);
      //     i = num+1;
      //   }
      //   FD_SET(sockfd, &wfds);
      // }
      // if (FD_ISSET(sockfd, &wfds)) {
      //   printf("SOCKET WRITABLE, sending: %u\n", i);
      //
      //   // Start packet burst
      //   const int packetburstsize = 1000;
      //   for (int c = 0; c < packetburstsize && i < 1000000; c++){

      memcpy(packetbuffer, &i, sizeof(i));
      i++;
      //usleep(10);
      int err = sendto(sockfd, packetbuffer, 4,
          MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
              len);
      if (err <= 0){
        printf("ERROR: %i\n", err);
      }
      if (i % 10000 == 0){
        printf("i: %u", i);
      }
        // }
        // printf("Finished burst: %u\n", i);
        //
        //
        // FD_CLR(sockfd, &wfds);
      //}
    }
    uint32_t finished = 2000000;
    memcpy(packetbuffer, &finished, 4);
    int err = sendto(sockfd, packetbuffer, 4,
        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
            len);
    if (err <= 0){
      printf("ERROR: %i\n", err);
    }

    



}
