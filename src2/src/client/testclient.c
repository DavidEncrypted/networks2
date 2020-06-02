
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <arpa/inet.h>
#include <netinet/in.h>

#include <alsa/asoundlib.h>

//#include "../communication/asp/asp.h"

#define BIND_PORT 1235



#define LOCALPORT 1235
#define SERVERPORT 1234
#define MAXLINE 2048



int main(){
  int sockfd;

  char *hello = "Setup Tubthumping";
  struct sockaddr_in     servaddr;
  struct sockaddr_in     cliaddr;

  // Creating socket file descriptor
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
  }

  size_t lenbuf;
  int i = sizeof(lenbuf);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &lenbuf, &i) < 0) {
    perror(": getsockopt");
  }

  printf("receive buffer size = %d\n", lenbuf);

  int b1 = 1000000;
  int b2 = sizeof(int);
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &b1, b2) < 0) {
      perror(": setsockopt");
  }

  lenbuf;
  i = sizeof(lenbuf);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &lenbuf, &i) < 0) {
    perror(": getsockopt");
  }

  printf("NEW receive buffer size = %d\n", lenbuf);


  size_t wtmbuf;
  int q = sizeof(wtmbuf);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &wtmbuf, &q) < 0) {
    perror(": getsockopt");
  }

  printf("rcv low watermark= %d\n", wtmbuf);

  int t1 = 150000;
  int t2 = sizeof(int);
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &t1, t2) < 0) {
      perror(": setsockopt");
  }

  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &wtmbuf, &q) < 0) {
    perror(": getsockopt");
  }

  printf("modified rcv low watermark= %d\n", wtmbuf);






  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));
  // Filling server information
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERVERPORT);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.2");
  cliaddr.sin_family = AF_INET;
  cliaddr.sin_port = htons(LOCALPORT);
  cliaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  //servaddr.sin_addr.s_addr = INADDR_ANY;
  // inet_pton(AF_INET, "127.0.0.2", &(servaddr.sin_addr));
  // inet_pton(AF_INET, "127.0.0.1", &(cliaddr.sin_addr));
  int n;
  socklen_t len;

  if (bind(sockfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) == -1) {
      perror("bind");
      close(sockfd);
      exit(1);
  }


  if (connect (sockfd, (const struct sockaddr *)&servaddr,
    sizeof(servaddr))){
      perror("connect");
      close(sockfd);
      exit(1);
  }

  sendto(sockfd, (const char *)hello, strlen(hello),
      MSG_CONFIRM, (const struct sockaddr *) &servaddr,
          sizeof(servaddr));
  printf("Setup message sent.\n");







  uint8_t *recvbuffer = malloc(4);
  uint8_t *sendbuffer = malloc(4);

  fd_set rfds, wfds;
  struct timeval tv;

  uint32_t num = 0;

  printf("first send\n");
  memcpy(sendbuffer, &num, 4);
  sendto(sockfd, sendbuffer, 4,
      MSG_CONFIRM, (const struct sockaddr *) &servaddr,
          sizeof(servaddr));
  uint32_t numdropped = 0;
  uint32_t prevnum = 0;
  printf("start recieve\n");
  do {

    // tv.tv_sec = 3;
    // tv.tv_usec = 0;
    //
    // FD_ZERO(&rfds);
    // FD_SET(sockfd, &rfds);
    // //
    // FD_ZERO(&wfds);
    // FD_SET(sockfd, &wfds);
    //
    // int preretval = select(sockfd + 1, &rfds, &wfds, NULL, &tv);
    // //printf("retval: %i\n", preretval);
    // if (preretval <= 0){
    //     printf("preretval <= 0: %i\n", preretval);
    //     continue;
    // }
    // else if (FD_ISSET(sockfd, &rfds)){

    n = recvfrom(sockfd, recvbuffer, 4,
                MSG_WAITALL, (struct sockaddr *) &servaddr,
                &len);
    //usleep(100);
    num = recvbuffer[0] | (recvbuffer[1] << 8) | (recvbuffer[2] << 16) | (recvbuffer[3] << 24);
    //printf("%u\n", num);

    if (num != prevnum + 1){
      numdropped += num - (prevnum + 1);
      printf("dropped: %u\n", num - (prevnum + 1));
    }
    prevnum = num;

    if (num % 1000 == 0){
      printf("num: %u\n", num);
    }

    //   FD_CLR(sockfd, &wfds);
    // }
    // else if (FD_ISSET(sockfd, &wfds)){
    //   printf("sending");
    //   memcpy(sendbuffer, &num, 4);
    //   sendto(sockfd, sendbuffer, 4,
    //       MSG_CONFIRM, (const struct sockaddr *) &servaddr,
    //           sizeof(servaddr));
    // }

  }while (num < 999100);
  printf("Num dropped: %u", numdropped);

}
