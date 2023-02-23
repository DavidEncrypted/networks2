// Skeleton Computer networks, Leiden University

// Submission by: David Schep s2055961

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>

#include "../communication/asp/asp.h"
#include "../progressbar/progressbar.h"

static int sockfd = -1;

#define SERVERPORT 1234
#define LOCALPORT 1235

#define SERVER_IP "127.0.0.2"
#define CLIENT_IP "127.0.0.1"

#define OPTSTR "f:"

#define SAMPLE_SIZE 4
#define SAMPLES_PER_PACKET 64 // Default: 64 Max: 252  --- Must be multiple of 4 (uint8_t max 255)

#define STARTING_COMPRESSION 0
// worst -- perfect
//   1   --  5
#define CONNECTION_QUALITY 5
#define PACKET_DELAY_LENGTH 200000 // microseconds

const int qualitydroplookup[5] = {30,60,80,90,100};
const int qualitydelaylookup[5] = {400,200,50,10,0}; // i per 100000 sends gets delay

// Only for debug
int numdrops = 0;
int numsends = 0;

int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr);

int sim_sendto(int p_sockfd, struct s_packet *p_packet, int p_bytes_in_packet, const struct sockaddr * p_cliaddr, int p_len);

struct wave_header {
    char riff_id[4];
    uint32_t size;
    char wave_id[4];
    char format_id[4];
    uint32_t format_size;
    uint16_t w_format_tag;
    uint16_t n_channels;
    uint32_t n_samples_per_sec;
    uint32_t n_avg_bytes_per_sec;
    uint16_t n_block_align;
    uint16_t w_bits_per_sample;
};

// wave file handle
struct wave_file {
    struct wave_header *wh;
    int fd;

    void *data;
    uint32_t data_size;

    uint8_t *samples;
    uint32_t payload_size;
};

static struct wave_file wf = {0,};

static int open_wave_file(struct wave_file *wf, const char *filename);
// close the wave file/clean up
static void close_wave_file(struct wave_file *wf);

int main(int argc, char *argv[]){
  // Parse arguments
  int opt;
  opterr = 0;
  char* filename = NULL;
  while ((opt = getopt(argc, argv, OPTSTR)) != EOF)
     switch(opt) {
         case 'f':
            filename = optarg;
            break;
     }
  if (filename == NULL){
    printf(" Usage: ./server -f [filename] \n");
    exit(0);
  }

  srand (time(NULL)); // Seed randomizer
  // Open the WAVE file
  if (open_wave_file(&wf, filename) < 0)
      return -1;

  uint32_t num_samples = (uint32_t) wf.payload_size / (wf.wh->n_channels * wf.wh->w_bits_per_sample / 8);

  struct sockaddr_in servaddr, cliaddr;

  int sockfd = setup_socket(&servaddr, &cliaddr);

  int n;
  socklen_t len = sizeof(cliaddr);  //len is value/result
  size_t recievelen;

  printf("Start recieve wait\n");
  // Recieve client setup
  char* buffer = malloc(sizeof(char) * 128);
  n = recvfrom(sockfd, buffer, sizeof(char) * 128,
              MSG_WAITALL, ( struct sockaddr *) &cliaddr,
              &len);
  buffer[n] = '\0';

  printf("Client : %s\n", buffer);
  // Start sending
  printf("Start send recieved\n");

  // Allocating recieve and send buffers
  uint8_t* currentp = wf.samples;
  uint16_t max_data_bytes = SAMPLES_PER_PACKET * SAMPLE_SIZE;
  uint32_t max_bytes_in_packet = sizeof(struct s_packet) + max_data_bytes; // 16 + ...

  uint8_t* packetbuffer = calloc(sizeof(uint8_t), max_bytes_in_packet);

  struct s_packet *packet;
  packet = calloc(sizeof(struct s_packet), max_bytes_in_packet);

  uint8_t* com_buffer = calloc(sizeof(uint8_t), 1);
  uint8_t current_compression = STARTING_COMPRESSION;

  uint32_t i = 0;
  uint32_t progressstep = num_samples / 60;
  uint32_t currentstep = 0;
  fd_set readfds, writefds, masterfds;
  struct timeval timeout;
  while (i < num_samples){
    // Do compression and data copy into packet
    switch (current_compression){
      case 0:
        packet->data_bytes = max_data_bytes;
        memcpy(packet->data, currentp, packet->data_bytes);
        break;
      case 1:
        // Reduce bitrate from 16 bit to 8 bit --- 4 bytes per sample to 2
        packet->data_bytes = SAMPLES_PER_PACKET * (SAMPLE_SIZE / 2);
        for (int i = 0; i < SAMPLES_PER_PACKET * (SAMPLE_SIZE / 2); i++)
          memcpy(packet->data + i, currentp + (i*2) + 1, 1);
        break;
      case 2:
        // drops every 4th samples this reduces the data transfered by 1/4th
        packet->data_bytes = ((SAMPLES_PER_PACKET / 4) * 3) * SAMPLE_SIZE;
        for (int l = 0; l < SAMPLES_PER_PACKET / 4; l++){
          memcpy(packet->data + (l*(SAMPLE_SIZE*3)), currentp + (l*(SAMPLE_SIZE*4)), SAMPLE_SIZE*3); // Remove 1 sample from the data
        }
        break;
      case 3:
        // Drop 4th sample and bitreduce to 8 bits
        packet->data_bytes = ((SAMPLES_PER_PACKET / 4) * 3) * 2;
        for (int l = 0; l < SAMPLES_PER_PACKET / 4; l++){
          for (int k = 0; k < 3; k++){
            memcpy(packet->data + (l*3) + k, currentp + (l*(SAMPLE_SIZE*4)) + (k*2) + 1, 1); // Remove 1 sample from the data
          }
        }
        break;
      case 4:
        // Drop 2,3,4th sample and bitreduce to 8 bits
        packet->data_bytes = ((SAMPLES_PER_PACKET / 4)) * 2;
        for (int l = 0; l < SAMPLES_PER_PACKET / 4; l++){
          memcpy(packet->data + (l), currentp + (l*(SAMPLE_SIZE*4)) + 1, 1); // Remove 3 samples from the data
        }
        break;
    }
    // copy header into packet
    packet->num_samples = num_samples;
    packet->sample_number = i;
    packet->samples_in_data = SAMPLES_PER_PACKET;
    packet->compression_level = current_compression;
    packet->checksum = 0;
    packet->checksum = calc_checksum((uint8_t *)packet, sizeof(struct s_packet) + packet->data_bytes);

    currentp += max_data_bytes;

    // Select --------------------------------------
    timeout.tv_sec = 0;                    /*set the timeout to 10 seconds*/
    timeout.tv_usec = 10000;

    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);

    memcpy(&readfds, &masterfds, sizeof(fd_set));
    memcpy(&writefds, &masterfds, sizeof(fd_set));

    if (select(sockfd+1, &readfds, &writefds, NULL, &timeout) < 0)
    {
      perror("on select");
      exit(1);
    }
    if (FD_ISSET(sockfd, &readfds))
    {
      int fn = recvfrom(sockfd, com_buffer, sizeof(uint8_t),
             MSG_WAITALL, (struct sockaddr *) &servaddr,
             &len);
      if (fn <= 0){
        perror("recv");
        exit(-1);
      }
      if (fn == sizeof(uint8_t)){
          printf("\nCompression request: %u\n", *(com_buffer));
          current_compression = *(com_buffer);
      }
      else {
          perror("compr return");
          exit(-1);
      }
    }
    else
    {
        int err = sim_sendto(sockfd, packet, sizeof(struct s_packet) + packet->data_bytes, (const struct sockaddr *) &cliaddr, len);
        if (err < 0){
          printf("ERROR: %i\n", err);
        }
    }
    // Select end ----------------------
    i += SAMPLES_PER_PACKET;
    if (i >= (currentstep * progressstep)){
      print_progress(currentstep, 60);
      currentstep++;
    }
  }
  printf("\n");
  // DONE Main Loop -----------------------

  uint8_t finished = 1;
  memcpy(com_buffer, &finished, 1);
  int finerr = sendto(sockfd, com_buffer, 1,
      MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
          len);
  if (finerr <= 0){
    printf("ERROR: %i\n", finerr);
  }
  printf("finished final send succesfully\n");

  close_wave_file(&wf);

  free(buffer);
  free(packetbuffer);
  free(packet);
  free(com_buffer);

  close(sockfd);
  sockfd = -1;

  printf("Total num bytes: %u\n", num_samples * SAMPLE_SIZE);
  printf("Total num bytes sent: %i\n", numsends * max_data_bytes);
  printf("Total num sends: %i\n", numsends);
  printf("Total num drops: %i\n", numdrops);
  printf("Total num bytes dropped: %i\n", numdrops * max_data_bytes);
  printf("Perc sends dropped: %.2f\n", (float)numdrops / (float)numsends * 100.0f);
}

int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr){

  // Creating socket file descriptor
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
  }

  memset(p_servaddr, 0, sizeof(*(p_servaddr)));
  memset(p_cliaddr, 0, sizeof(*(p_cliaddr)));

  // Filling server information
  p_servaddr->sin_family = AF_INET; // IPv4
  //servaddr.sin_addr.s_addr = INADDR_ANY;
  p_servaddr->sin_port = htons(SERVERPORT);
  p_servaddr->sin_addr.s_addr = inet_addr(SERVER_IP);

  p_cliaddr->sin_family = AF_INET; // IPv4
  p_cliaddr->sin_port = htons(LOCALPORT);
  p_cliaddr->sin_addr.s_addr = inet_addr(CLIENT_IP);

  // Bind the socket with the server address
  if ( bind(sockfd, (const struct sockaddr *)p_servaddr,
          sizeof(*(p_servaddr))) < 0 )
  {
      perror("bind failed");
      exit(EXIT_FAILURE);
  }
  return sockfd;
}

int sim_sendto(int p_sockfd, struct s_packet *p_packet, int p_bytes_in_packet, const struct sockaddr * p_cliaddr, int p_len){

    int randomdelay = rand() % 1000000; // 0 - 99
    // Check if we delay
    if (randomdelay < qualitydelaylookup[CONNECTION_QUALITY-1]){
        usleep(PACKET_DELAY_LENGTH);
    }

    int randomnum = rand() % 100; // 0 - 99
    // Send
    int err = 0;
    if (randomnum < qualitydroplookup[CONNECTION_QUALITY-1]){
        err = sendto(p_sockfd, p_packet, p_bytes_in_packet,
            MSG_CONFIRM, p_cliaddr,
                p_len);
    }
    else numdrops++;
    numsends++;
    return err;
}

static int open_wave_file(struct wave_file *wf, const char *filename) {
    // Open the file for read only access
    wf->fd = open(filename, O_RDONLY);
    if (wf->fd < 0) {
        fprintf(stderr, "couldn't open %s\n", filename);
        return -1;
    }

    struct stat statbuf;
    // Get the size of the file
    if (fstat(wf->fd, &statbuf) < 0)
        return -1;

    wf->data_size = statbuf.st_size; // Total size of the file

    // Map the file into memory
    wf->data = mmap(0x0, wf->data_size, PROT_READ, MAP_SHARED, wf->fd, 0);
    if (wf->data == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
        return -1;
    }

    wf->wh = wf->data;

    // Check whether the file is a wave file
    if (strncmp(wf->wh->riff_id, "RIFF", 4)
            || strncmp(wf->wh->wave_id, "WAVE", 4)
            || strncmp(wf->wh->format_id, "fmt", 3)) {
        fprintf(stderr, "%s is not a valid wave file\n", filename);
        return -1;
    }

    // Skip to actual data fragment
    uint8_t* p = (uint8_t*) wf->data + wf->wh->format_size + 16 + 4;
    uint32_t* size = (uint32_t*) (p + 4);
    do {
        if (strncmp((char*) p, "data", 4))
            break;

        wf->samples = p;
        wf->payload_size = *size;
        p += 8 + *size;
    } while (strncmp((char*) p, "data", 4) && (uint32_t) (((uint8_t*) p) - (uint8_t*) wf->data) < statbuf.st_size);

    if (wf->wh->w_bits_per_sample != 16) {
        fprintf(stderr, "can't play sample with bitsize %d\n",
                        wf->wh->w_bits_per_sample);
        return -1;
    }

    float playlength = (float) *size / (wf->wh->n_channels * wf->wh->n_samples_per_sec * wf->wh->w_bits_per_sample / 8);

    printf("file %s, mode %s, samplerate %u, time %.1f sec\n", filename, wf->wh->n_channels == 2 ? "Stereo" : "Mono", wf->wh->n_samples_per_sec, playlength);
    return 0;
}

// close the wave file/clean up
static void close_wave_file(struct wave_file *wf) {
    munmap(wf->data, wf->data_size);
    close(wf->fd);
}
