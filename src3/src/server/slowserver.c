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

#define SAMPLE_SIZE 4

int setup_socket(struct sockaddr_in *p_servaddr, struct sockaddr_in *p_cliaddr){
  int sockfd;
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
  p_servaddr->sin_addr.s_addr = inet_addr("127.0.0.2");

  p_cliaddr->sin_family = AF_INET; // IPv4
  p_cliaddr->sin_port = htons(LOCALPORT);
  p_cliaddr->sin_addr.s_addr = inet_addr("127.0.0.1");

  // Bind the socket with the server address
  if ( bind(sockfd, (const struct sockaddr *)p_servaddr,
          sizeof(*(p_servaddr))) < 0 )
  {
      perror("bind failed");
      exit(EXIT_FAILURE);
  }
  return sockfd;
}


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

int main(){

  // TODO: Parse command-line options
  char *filename = "tubthumping.wav";

  // Open the WAVE file
  if (open_wave_file(&wf, filename) < 0)
      return -1;

  uint32_t num_samples = (uint32_t) wf.payload_size / (wf.wh->n_channels * wf.wh->w_bits_per_sample / 8);
  printf("n samples %u\n", num_samples);


  struct sockaddr_in servaddr, cliaddr;

  int sockfd = setup_socket(&servaddr, &cliaddr);

  int n;
  socklen_t len = sizeof(cliaddr);  //len is value/resuslt
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

  uint8_t* currentp = wf.samples;

  int samples_per_packet = 4;
  uint16_t data_bytes = samples_per_packet * SAMPLE_SIZE;
  int bytes_in_packet = 4 + 4 + 2 + data_bytes; // 4 + 4 + 2 + 16 = 26
  uint8_t* packetbuffer = malloc(bytes_in_packet);
  memset(packetbuffer, 0, bytes_in_packet);


  // | num_samples   | sample_number | data_bytes | databytes...
  // | 4 bytes       | 4 bytes       | 2 bytes    | 4 bytes per sample
  //uint32_t num_samples = 1000064;
  uint32_t i = 0;
  //uint8_t k = 69;
  while (i < num_samples){
    // copy header into packet
    memcpy(packetbuffer, &num_samples, sizeof(num_samples));
    memcpy(packetbuffer + 4, &i, sizeof(i));
    memcpy(packetbuffer + 8, &data_bytes, sizeof(data_bytes));

    //copy data into packet
    memcpy(packetbuffer + 10, currentp, data_bytes);
    currentp += data_bytes;

    i += samples_per_packet;

    int err = sendto(sockfd, packetbuffer, bytes_in_packet,
        MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
            len);
    if (err <= 0){
      printf("ERROR: %i\n", err);
    }
    if (i % 10000 == 0){
      usleep(100);
      printf("i: %u\n", i);
    }
  }

  // DONE -----------------------

  uint8_t finished = 1;
  memcpy(packetbuffer, &finished, 1);
  int finerr = sendto(sockfd, packetbuffer, 1,
      MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
          len);
  if (finerr <= 0){
    printf("ERROR: %i\n", finerr);
  }
  printf("finished final send succesfully\n");




}
