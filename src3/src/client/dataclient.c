
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

#define SAMPLE_SIZE 4

#define LOCALPORT 1235
#define SERVERPORT 1234
#define MAXLINE 2048

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024


// prototype
int send_startup(int sockfd, const struct sockaddr *server_addr, int p_server_addr_len){
  char *hello = "Setup Tubthumping";

  int setuperr = sendto(sockfd, (const char *)hello, strlen(hello),
      MSG_CONFIRM, server_addr,
          p_server_addr_len);
  printf("Setup message sent.\n");

  if (setuperr <= 0){

    perror("sendto setup");
    //return setuperr;
  }
  return setuperr;
}

void playbuffer(uint8_t** playbuffer, uint32_t number_samples){

  // Open audio device
  snd_pcm_t *snd_handle;

  int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

  if (err < 0) {
      fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
      return;
  }

  // Configure parameters of PCM output
  err = snd_pcm_set_params(snd_handle,
                           SND_PCM_FORMAT_S16_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           NUM_CHANNELS,
                           SAMPLE_RATE,
                           0,              // Allow software resampling
                           500000);        // 0.5 seconds latency
  if (err < 0) {
      printf("couldnt configure audio device: %s\n", snd_strerror(err));
      return;
  }

  printf("SENDING FRAMES\n");
  uint8_t* pbuf = *(playbuffer);
  for (int i = 0; i < number_samples*4 ; i += 1024){
    //printf("i: %i", i);
    snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle,
                    pbuf, 1024 / SAMPLE_SIZE);
    pbuf += 1024;
    //printf("preframes: %u", preframes);
  }


}

int main(){
  int sockfd;


  struct sockaddr_in     servaddr;
  struct sockaddr_in     cliaddr;

  // Creating socket file descriptor
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
  }

  // Setting recieve buffer
  size_t lenbuf;
  unsigned int i = sizeof(lenbuf);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &lenbuf, &i) < 0) {
    perror(": getsockopt");
  }

  printf("receive buffer size = %ld\n", lenbuf);

  int b1 = 2000000;
  int b2 = sizeof(int);
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &b1, b2) < 0) {
      perror(": setsockopt");
  }

  i = sizeof(lenbuf);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &lenbuf, &i) < 0) {
    perror(": getsockopt");
  }

  printf("NEW receive buffer size = %ld\n", lenbuf);

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));
  // Filling server information
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERVERPORT);
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.2");
  cliaddr.sin_family = AF_INET;
  cliaddr.sin_port = htons(LOCALPORT);
  cliaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  int n;

  if (bind(sockfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) == -1) {
      perror("bind");
      close(sockfd);
      exit(1);
  }

  int server_addr_len = sizeof(servaddr);

  send_startup(sockfd, (const struct sockaddr *) &servaddr, server_addr_len);


  //uint8_t* fullsamplebuffer = malloc(num_samples * 4);

  uint8_t *recvbuffer = malloc(128);
  uint8_t *sendbuffer = malloc(128);

  uint32_t num = 0;

  uint32_t number_samples;
  uint32_t firstlocnum;
  printf("postmalloc\n");
  fd_set readfds, masterfds;
  struct timeval timeout;
  bool firstrecieve = 0;
  socklen_t len = sizeof(servaddr);
  do {
    printf("loop\n");
    timeout.tv_sec = 0;                    /*set the timeout to 10 seconds*/
    timeout.tv_usec = 100000;


    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);

    memcpy(&readfds, &masterfds, sizeof(fd_set));

    if (select(sockfd+1, &readfds, NULL, NULL, &timeout) < 0)
    {
      perror("on select");
      exit(1);
    }

    if (FD_ISSET(sockfd, &readfds))
    {
      printf("recieving...\n");
      int fn = recvfrom(sockfd, recvbuffer, 128,
             MSG_WAITALL, (struct sockaddr *) &servaddr,
             &len);
      if (fn <= 0){
        perror("recv");
        exit(-1);
      }
      else if (fn >= 8){
        number_samples = recvbuffer[0] | (recvbuffer[1] << 8) |
                              (recvbuffer[2] << 16) | (recvbuffer[3] << 24);
        firstlocnum = recvbuffer[4] | (recvbuffer[5] << 8) |
                              (recvbuffer[6] << 16) | (recvbuffer[7] << 24);
        printf("First recv size: %i\n", fn);
        printf("First recv num, samples: %u, %u\n", firstlocnum, number_samples);


        firstrecieve = 1;
      }


    }
    else
    {
      send_startup(sockfd, (const struct sockaddr *) &servaddr, server_addr_len);
    }
  } while (!firstrecieve);
  printf("allocating: %u", number_samples);
  uint8_t *fullsamplebuffer = malloc(number_samples*SAMPLE_SIZE);

  memset(fullsamplebuffer, 0, number_samples);

  memcpy(fullsamplebuffer+(firstlocnum*SAMPLE_SIZE), recvbuffer+10, 4);

  printf("POST SETUP\n");


  uint32_t nullbytes = 0;
  uint32_t prevnum = -1;

  bool recieving = true;
  uint32_t locnum;
  //int prevnum;
  printf("start RECIEVE\n");
  do {

    n = recvfrom(sockfd, recvbuffer, 128,
                MSG_WAITALL, (struct sockaddr *) &servaddr,
                &len);
    if (n <= 0){
      perror("MAIN RECV\n");
      printf("probably disconnected end loop\n");
      break;
    }
    if (n == 1){
      // Recieved finished packet
      if (*(recvbuffer) == 1){
        // Success
        printf("Recieved finish packet\n");
        recieving = false;
        break;
      }
      else {
        printf("Error: 1 byte recieved -> not 1\n");
        exit(-1);
      }
    }
    else {
      // | num_samples   | sample_number | data_bytes | databytes...
      // | 4 bytes       | 4 bytes       | 2 bytes    | 4 bytes per sample

      locnum = recvbuffer[4] | (recvbuffer[5] << 8) |
                            (recvbuffer[6] << 16) | (recvbuffer[7] << 24);

      uint16_t data_bytes = recvbuffer[8] | (recvbuffer[9] << 8);
      //printf("data_bytes 24? %u", data_bytes);

      memcpy(fullsamplebuffer+(locnum*SAMPLE_SIZE), recvbuffer + 10, data_bytes);

      for (int y = 0; y < (data_bytes / SAMPLE_SIZE); y++){
        if (recvbuffer[10 + y] == 0) nullbytes++;
      }

    }
  }while (recieving);
  printf("FINISHED\n");
  printf("final num: %u\n", locnum);
  //printf("Num dropped guess: %u * 4 = %u\n", numdropped, numdropped * 4);
  int correct = 0;
  int incorrect = 0;
  for (int b = 0; b < number_samples * SAMPLE_SIZE; b++){
    if (fullsamplebuffer[b] != 0){
      correct++;
      //printf("%u", fullsamplebuffer[b]);
    }
    else incorrect++;
    //printf("%u", fullsamplebuffer[b]);
  }
  printf("c/i/t/n/c+n: %i, %i, %i, %i, %i\n", correct, incorrect,number_samples, nullbytes, correct + nullbytes);
  printf("correct: %i\n", correct + nullbytes);
  printf("incorrect: %i\n", incorrect - nullbytes);


  playbuffer(&fullsamplebuffer, number_samples);

}
