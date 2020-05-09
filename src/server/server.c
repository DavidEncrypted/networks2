/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "../communication/asp/asp.h"

#define BUFLEN 512
#define NPACK 10
#define PORT 9930

static int asp_socket_fd = -1;

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

// open the wave file
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

void diep(char *s)
{
  perror(s);
  exit(1);
}

int main(int argc, char **argv) {

    // TODO: Parse command-line options
    char *filename = "tubthumping.wav";

    // Open the WAVE file
    if (open_wave_file(&wf, filename) < 0)
        return -1;

    struct sockaddr_in si_me, si_other;
    int s, i, slen=sizeof(si_other);
    char buf[BUFLEN];

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      diep("socket");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, &si_me, sizeof(si_me))==-1)
      diep("bind");

    for (i=0; i<NPACK; i++) {
      if (recvfrom(s, buf, BUFLEN, 0, &si_other, &slen)==-1)
        diep("recvfrom()");
      printf("Received packet from %s:%d\nData: %s\n\n",
      inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
    }

    close(s);


    // TODO: Read and send audio data

    // Clean up
    close_wave_file(&wf);

    return 0;
}
