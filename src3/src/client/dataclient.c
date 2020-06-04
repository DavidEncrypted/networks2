
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <alsa/asoundlib.h>

#include "../communication/asp/asp.h"

#define BIND_PORT 1235

#define SAMPLE_SIZE 4

#define LOCALPORT 1235
#define SERVERPORT 1234
#define MAXLINE 2048

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024

#define MAX_PACKET_SIZE 256

pthread_mutex_t lock;

static int sockfd = -1;

// Prototypes
int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr);
int send_startup(int sockfd, const struct sockaddr *server_addr, int p_server_addr_len);
void playbuffer(uint8_t** playbuffer, uint32_t number_samples);

struct threadargs {
    uint8_t** p_samplebuffer;
    uint32_t* p_lastsampleset;
    uint32_t* p_totalsamples;
};

void *liveplaybuffer(void *args){
  uint32_t* lastsampleset = ((struct threadargs*)args)->p_lastsampleset;
  uint32_t* totalsamples = ((struct threadargs*)args)->p_totalsamples;
  uint8_t* samplebuffer = *(((struct threadargs*)args)->p_samplebuffer);

  // Setup audio device
  // Open audio device
  snd_pcm_t *snd_handle;

  int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

  if (err < 0) {
      fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
      return NULL;
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
      return NULL;
  }
  uint32_t pollsampleset = 0;
  // Wait for buffer to fill to minimum level
  while (pollsampleset < 4096){
    pthread_mutex_lock(&lock);
    pollsampleset = *(lastsampleset);
    pthread_mutex_unlock(&lock);
  }
  printf("Minimum play buffer full! START PLAYING\n");
  // Total buffer size: totalsamples * 4
  // pbuf + 1024 = 256 samples
  uint8_t* pbuf = samplebuffer;

  bool done = false;

  uint32_t samplesuploaded = 0;

  do {
    pthread_mutex_lock(&lock);
    pollsampleset = *(lastsampleset);
    pthread_mutex_unlock(&lock);

    if (samplesuploaded + 256 < pollsampleset){
      snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle,
                      pbuf, 1024 / SAMPLE_SIZE);
      //printf("frames uploaded to audio player: %li\n", preframes);
      pbuf += preframes * 4;
      samplesuploaded += preframes;
      //if (samplesuploaded % 256 * 1000 == 0)
        //printf("samplesuploaded: %u\n", samplesuploaded);
    }
    else if (pollsampleset + 1 == *(totalsamples)){
      snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle,
                      pbuf, *(totalsamples) - samplesuploaded);
      //printf("frames uploaded to audio player: %li\n", preframes);
      pbuf += preframes * 4;
      samplesuploaded += preframes;
      if (samplesuploaded == *(totalsamples)){
        done = 1;
        printf("samplesuploaded: %u\n", samplesuploaded);
        printf("totalsamples: %u\n", *(totalsamples));
        printf("FINISHED PLAYING\n");
      }
    }


  } while (!done);


  //
  // printf("THREAD lastsampleset: %u\n", *(lastsampleset));
  // if (*(lastsampleset) > 0){
  //   printf("eerste data uit samplebuffer: %u\n", samplebuffer[0]);
  // }
  //





  //
  // for (int i = 0; i < number_samples*4 ; i += 1024){
  //   //printf("i: %i", i);
  //   snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle,
  //                   pbuf, 1024 / SAMPLE_SIZE);
  //   pbuf += 1024;
  //   //printf("preframes: %u", preframes);
  // }


  //
  // for (int i = 0; i < 10; i++){
  //   pthread_mutex_lock(&lock);
  //   printf("THREAD lastsampleset: %u\n", *(lastsampleset));
  //   if (*(lastsampleset) > 0){
  //     printf("eerste data uit samplebuffer: %u\n", samplebuffer[0]);
  //   }
  //   pthread_mutex_unlock(&lock);
  //   sleep(1);
  // }
  //printf("live playing\n");
  return NULL;
}

int main(){
//  int sockfd;

  struct sockaddr_in servaddr, cliaddr;

  sockfd = setup_socket(&servaddr, &cliaddr);

  int server_addr_len = sizeof(servaddr);

  send_startup(sockfd, (const struct sockaddr *) &servaddr, server_addr_len);

  struct s_packet *packet = malloc(MAX_PACKET_SIZE);
  //uint8_t *recvbuffer = malloc(128);
  uint8_t *sendbuffer = malloc(MAX_PACKET_SIZE);

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
      int fn = recvfrom(sockfd, packet, 256,
             MSG_WAITALL, (struct sockaddr *) &servaddr,
             &len);
      if (fn <= 0){
        perror("recv");
        exit(-1);
      }
      else if (fn >= 8){
        number_samples = packet->num_samples;
        printf("First recv size: %i\n", fn);
        printf("First recv num, samples: %u, %u\n", packet->sample_number, number_samples);

        firstrecieve = 1;

      }
    }
    else
    {
      send_startup(sockfd, (const struct sockaddr *) &servaddr, server_addr_len);
    }
  } while (!firstrecieve);

  // Allocating full sample buffer
  printf("allocating: %u\n", number_samples);
  uint8_t *fullsamplebuffer = malloc(number_samples*SAMPLE_SIZE);
  memset(fullsamplebuffer, 0, number_samples);
  // Copying first data into it
  memcpy(fullsamplebuffer+(packet->sample_number*SAMPLE_SIZE), packet->data, packet->data_bytes);
  printf("Correct first data: %u\n", fullsamplebuffer[0]);
  // Allocating full data recieved bitmap

  uint32_t lastsampleset = 0;

  // Setup multithreading
  if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        perror("mutex");
        return 1;
    }
  struct threadargs *t_args = (struct threadargs *)malloc(sizeof(struct threadargs));

  t_args->p_lastsampleset = &lastsampleset;
  t_args->p_samplebuffer = &fullsamplebuffer;
  t_args->p_totalsamples = &number_samples;

  pthread_t audiothread;
  int pthreaderr = pthread_create(&audiothread, NULL, &liveplaybuffer, (void*) t_args);
        if (pthreaderr != 0)
            printf("\ncan't create thread :[%s]", strerror(pthreaderr));


  printf("POST SETUP\n");

  // Start recieving all data;
  uint32_t totalbytesrecieved = 0;
  uint32_t prevnum = 0;
  bool recieving = true;

  uint32_t locnum = 0;

  // Quality check variables
  uint32_t last_check_num = 0;
  int last_check_num_bytes_dropped = 0;
  uint32_t highest_sample_number = 0;

  printf("start RECIEVE\n");
  int n;
  do {
    n = recvfrom(sockfd, packet, 256,
                MSG_WAITALL, (struct sockaddr *) &servaddr,
                &len);
    if (n <= 0){
      perror("MAIN RECV\n");
      printf("probably disconnected end loop\n");
      break;
    }
    if (n == 4){
      // Recieved finish packet
      if (packet->num_samples == 1){
        // Success
        printf("Recieved finish packet\n");
        recieving = false;
        break;
      }
      else {
        printf("Error: 4 byte recieved -> not 1\n");
        exit(-1);
      }
    }
    else {
      // | num_samples   | sample_number | data_bytes | quality_level | databytes...
      // | 4 bytes       | 4 bytes       | 2 bytes    | 2 bytes       | 4 bytes per sample ...

      memcpy(fullsamplebuffer+(packet->sample_number*SAMPLE_SIZE), packet->data, packet->data_bytes);

      // Get lock
      pthread_mutex_lock(&lock);
      lastsampleset = packet->sample_number;
      pthread_mutex_unlock(&lock);
      // Release lock


      // Quality Check
      if (packet->sample_number > highest_sample_number) highest_sample_number = packet->sample_number;

      totalbytesrecieved += packet->data_bytes;

      if (last_check_num + 1024 > packet->sample_number){
          // Do quality check
          uint32_t expected_bytes_recv = highest_sample_number * SAMPLE_SIZE;
          int current_num_bytes_dropped = (int) expected_bytes_recv - totalbytesrecieved;
          //printf("current num bytes dropped: %u\n", current_num_bytes_dropped);
          //printf("last Num bytes dropped: %u\n", last_check_num_bytes_dropped);
          if (current_num_bytes_dropped - last_check_num_bytes_dropped != 0)
            printf("last 1024 bytes num dropped: %i\n", current_num_bytes_dropped - last_check_num_bytes_dropped);
          last_check_num_bytes_dropped = current_num_bytes_dropped;
          last_check_num = packet->sample_number;
      }


      // for (int y = 0; y < (packet->data_bytes / SAMPLE_SIZE); y++){
      //   if (recvbuffer[10 + y] == 0) nullbytes++;
      // }

    }
  }while (recieving);
  printf("FINISHED\n");

  // Setting lastsampleset to final sample;

  pthread_mutex_lock(&lock);
  if (lastsampleset != number_samples - 1)
    lastsampleset = number_samples - 1;
  pthread_mutex_unlock(&lock);

  //printf("final num: %u\n", locnum);
  printf("Total samples: %u\n", number_samples);
  printf("Total number of bytes dropped: %u\n", last_check_num_bytes_dropped);




  pthread_join(audiothread, NULL);

  pthread_mutex_destroy(&lock);

  //playbuffer(&fullsamplebuffer, number_samples);

}

int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr){
  int sockfd;
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


  memset(p_servaddr, 0, sizeof(*(p_servaddr)));
  memset(p_cliaddr, 0, sizeof(*(p_cliaddr)));

  // Filling server information
  p_servaddr->sin_family = AF_INET; // IPv4
  //servaddr.sin_addr.s_addr = INADDR_ANY;
  p_servaddr->sin_port = htons(SERVERPORT);
  p_servaddr->sin_addr.s_addr = inet_addr("127.0.0.2");

  p_cliaddr->sin_family = AF_INET; // IPv4
  p_cliaddr->sin_port = htons(LOCALPORT);
  p_cliaddr->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Bind the socket with the server address
  if ( bind(sockfd, (const struct sockaddr *)p_cliaddr,
          sizeof(*(p_cliaddr))) < 0 )
  {
      perror("bind failed");
      exit(EXIT_FAILURE);
  }
  return sockfd;
}

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

// void playbuffer(uint8_t** playbuffer, uint32_t number_samples){
//
//   // Open audio device
//   snd_pcm_t *snd_handle;
//
//   int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
//
//   if (err < 0) {
//       fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
//       return;
//   }
//
//   // Configure parameters of PCM output
//   err = snd_pcm_set_params(snd_handle,
//                            SND_PCM_FORMAT_S16_LE,
//                            SND_PCM_ACCESS_RW_INTERLEAVED,
//                            NUM_CHANNELS,
//                            SAMPLE_RATE,
//                            0,              // Allow software resampling
//                            500000);        // 0.5 seconds latency
//   if (err < 0) {
//       printf("couldnt configure audio device: %s\n", snd_strerror(err));
//       return;
//   }
//
//   printf("SENDING FRAMES\n");
//   uint8_t* pbuf = *(playbuffer);
//   for (int i = 0; i < number_samples*4 ; i += 1024){
//     //printf("i: %i", i);
//     snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle,
//                     pbuf, 1024 / SAMPLE_SIZE);
//     pbuf += 1024;
//     //printf("preframes: %u", preframes);
//   }
// }
