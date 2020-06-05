
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <alsa/asoundlib.h>

#include "../communication/asp/asp.h"
#include "../progressbar/progressbar.h"

#define OPTSTR "b:"

#define BIND_PORT 1235

#define SAMPLE_SIZE 4

#define LOCALPORT 1235
#define SERVERPORT 1234
#define MAXLINE 2048

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024

#define MAX_PACKET_SIZE 256

#define COMPRESSION_UPDATING_ENABLED 1 // If 1 the client will tell the server what level of compression it wants according to the packet droprate
#define ROLLING_AVERAGE_LENGTH 4
const float compression_thresholds[4] = {5000.0f,15000.0f,30000.0f, 40000.0f}; // 4 thresholds, 5 compression levels


pthread_mutex_t lock;

static int sockfd = -1;

// Prototypes
int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr);
int send_startup(int sockfd, const struct sockaddr *server_addr, int p_server_addr_len);
uint8_t calc_compr_level(float droppedavg, uint8_t current_compression_level);

struct threadargs {
    uint8_t** p_samplebuffer;
    uint32_t* p_lastsampleset;
    uint32_t* p_totalsamples;
    int p_minbuffer;
};

void *liveplaybuffer(void *args){
  uint32_t* lastsampleset = ((struct threadargs*)args)->p_lastsampleset;
  uint32_t* totalsamples = ((struct threadargs*)args)->p_totalsamples;
  uint8_t* samplebuffer = *(((struct threadargs*)args)->p_samplebuffer);
  int minbuffer = ((struct threadargs*)args)->p_minbuffer;

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
  //printf("minbuffer: %i", minbuffer);
  while (pollsampleset < minbuffer){
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
  return NULL;
}

int main(int argc, char *argv[]){
  int opt;
  int min_buffer = 65536;
  opterr = 0;
  while ((opt = getopt(argc, argv, OPTSTR)) != EOF)
     switch(opt) {
         case 'b':
            min_buffer = (int)strtol(optarg, NULL, 10);
            printf("Minimum buffer fill required to start playing set to: %i\n", min_buffer);
            break;
         default:
            break;
     }

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
  printf("Waiting for server to respond...\n");
  fd_set readfds, masterfds;
  struct timeval timeout;
  bool firstrecieve = 0;
  socklen_t len = sizeof(servaddr);

  do {
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
  uint8_t *fullsamplebuffer = calloc(number_samples, SAMPLE_SIZE);
  //memset(fullsamplebuffer, 0, number_samples);
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
  t_args->p_minbuffer = min_buffer;


  pthread_t audiothread;
  int pthreaderr = pthread_create(&audiothread, NULL, &liveplaybuffer, (void*) t_args);
        if (pthreaderr != 0)
            printf("\ncan't create thread :[%s]", strerror(pthreaderr));

  // Start recieving all data;
  uint32_t totalbytesrecieved = packet->data_bytes;
  uint32_t prevnum = 0;
  bool recieving = true;

  uint32_t locnum = 0;

  // Quality check variables
  uint32_t last_check_num = 0;
  int last_check_num_bytes_recieved = 0;
  uint32_t highest_sample_number = 0;
  uint32_t totalbytesexpected = 0;

  uint32_t last3dropped[3] = {0};
  uint8_t valuetochangearray = 0;
  bool established_average = false;

  uint32_t progressstep = packet->num_samples / 60;
  uint32_t currentstep = 0;
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
      // | num_samples   | sample_number | data_bytes | compression_level | databytes...
      // | 4 bytes       | 4 bytes       | 2 bytes    | 2 bytes       | 4 bytes per sample ...

      // Check Checksum
      uint16_t checksumstore = packet->checksum;
      packet->checksum = 0;
      if (checksumstore != calc_checksum((uint8_t*)packet, sizeof(struct s_packet) + packet->data_bytes)){
        printf("ERROR CHECKSUM\n");
        printf("Checksum in packet: %u\n", checksumstore);
        printf("Checksum in packet: %u\n", calc_checksum((uint8_t*)packet, sizeof(struct s_packet) + packet->data_bytes));
        exit(-1);
      }

      switch (packet->compression_level){
        case 1:
        case 3:
        case 4: ;
          //uint8_t* p_data = packet->data;
          uint16_t bitboosted;
          for (int i = 0; i < packet->data_bytes; i++){
            memcpy(fullsamplebuffer+(packet->sample_number*SAMPLE_SIZE) + (i*2) + 1, packet->data + i, 1);
          }
          break;
        default:
         memcpy(fullsamplebuffer+(packet->sample_number*SAMPLE_SIZE), packet->data, packet->data_bytes);
         break;
      }

      //printf("data_bytes: %u\n", packet->data_bytes);
      // Get lock
      pthread_mutex_lock(&lock);
      lastsampleset = packet->sample_number;
      pthread_mutex_unlock(&lock);
      // Release lock

    if (COMPRESSION_UPDATING_ENABLED){
      // Quality Check
      if (packet->sample_number > highest_sample_number) highest_sample_number = packet->sample_number;
      totalbytesrecieved += packet->data_bytes;
      if (last_check_num + 32896 < packet->sample_number){
        // Do quality check
        uint32_t expected_bytes_recv = 0;
        switch (packet->compression_level){
          case 0:
            expected_bytes_recv = (highest_sample_number - last_check_num) * 4; // 4 bytes
            break;
          case 1:
            expected_bytes_recv = (highest_sample_number - last_check_num) * 2; // 2 bytes
            break;
          case 2:
            expected_bytes_recv = (highest_sample_number - last_check_num) * 3; // 3 bytes
            break;
          case 3:
            expected_bytes_recv = (((highest_sample_number - last_check_num) * 3) / 2); // 1.5 bytes
            break;
          case 4:
            expected_bytes_recv = (highest_sample_number - last_check_num) / 2; // effectively 0.5 bytes
            break;
        }

        totalbytesexpected += expected_bytes_recv;
        uint32_t actual_bytes_recv = totalbytesrecieved - last_check_num_bytes_recieved;

        if (actual_bytes_recv > expected_bytes_recv) last3dropped[valuetochangearray] = 0;
        else last3dropped[valuetochangearray] = expected_bytes_recv - actual_bytes_recv;

        valuetochangearray++;
        if (valuetochangearray == 3) {
          valuetochangearray = 0;
          if (!established_average){
            established_average = true;
          }
        }
        last_check_num_bytes_recieved = totalbytesrecieved;
        last_check_num = highest_sample_number;
        // Record 3 most recent number of bytes dropped

        // Average these values
        float droppedavg = (float)(last3dropped[0] + last3dropped[1] + last3dropped[2]) / 3.0f;
        // compression_thresholds
        // If average surpasses threasholds change compression
        uint8_t req_compression = calc_compr_level(droppedavg, packet->compression_level);

        // SEND requested compression level IF not already compression compression_level
        if (req_compression != packet->compression_level && established_average){
            printf("Asking compression_level: %u, Avg dropped packets: %.2f\n", req_compression, droppedavg);
            memcpy(sendbuffer, &req_compression, sizeof(uint8_t));

            int compr_senderr = sendto(sockfd, sendbuffer, sizeof(uint8_t),
                MSG_CONFIRM, (const struct sockaddr *) &servaddr,
                    server_addr_len);
        }
      }
    } // IF FALSE
      if (packet->sample_number >= (currentstep * progressstep)){
        print_progress(currentstep, 60);
        currentstep++;
      }
    }

  }while (recieving);
  printf("\nStreaming Finished! The audio will keep playing untill the song is finished\n");

  // Setting lastsampleset to final sample;

  pthread_mutex_lock(&lock);
  if (lastsampleset != number_samples - 1)
    lastsampleset = number_samples - 1;
  pthread_mutex_unlock(&lock);
  if ((int)totalbytesexpected - (int)totalbytesrecieved < 0) totalbytesexpected = totalbytesrecieved;
  //printf("final num: %u\n", locnum);
  printf("Total data bytes expected: %u\n", totalbytesexpected);
  //printf("Total number of packets: %u\n", number_samples*SAMPLE_SIZE / packet->data_bytes);
  printf("Total number of bytes recieved: %u\n", totalbytesrecieved);

  printf("Total number of data bytes dropped: %u\n", totalbytesexpected - totalbytesrecieved);
  float perc_bytes_dropped = (((float)(totalbytesexpected - totalbytesrecieved) / (float)(totalbytesexpected)) * 100.0f);
  printf("Percentage of data bytes dropped: %.2f\n", perc_bytes_dropped);

  printf("Percentage data compressed/dropped to from original: %.2f\n", ((float)totalbytesrecieved / (float)(number_samples*SAMPLE_SIZE)) * 100.0f);
  //
  // printf("Percetage of packets dropped: %0.2f\n", perc_bytes_dropped);

  pthread_join(audiothread, NULL);

  pthread_mutex_destroy(&lock);
  free(packet);
  free(sendbuffer);
  free(t_args);
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
  //printf("Setup message sent.\n");

  if (setuperr <= 0){

    perror("sendto setup");
    //return setuperr;
  }
  return setuperr;
}



uint8_t calc_compr_level(float droppedavg, uint8_t current_compression_level){
    if (current_compression_level == 0 && droppedavg > compression_thresholds[0]) return 1; // I dont like how hardcoded this is
    if (current_compression_level == 1 && droppedavg > compression_thresholds[1]) return 2; // But I cant think if a simple better way
    if (current_compression_level == 2 && droppedavg > compression_thresholds[2]) return 3;
    if (current_compression_level == 3 && droppedavg > compression_thresholds[3]) return 4;

    if (current_compression_level == 4 && droppedavg < compression_thresholds[2]) return 3; // Only allow lower compression if
    if (current_compression_level == 3 && droppedavg < compression_thresholds[1]) return 2; // threshold is lower than currentlevel - 2
    if (current_compression_level == 2 && droppedavg < compression_thresholds[0]) return 1; // This way the compression doesnt bounce back and forth
    if (current_compression_level == 1 && droppedavg < 20.0f) return 0;
    return current_compression_level;
}
