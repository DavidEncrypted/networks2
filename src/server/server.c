/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */
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

#include "../communication/asp/asp.h"



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


#define PORT 1235
#define MAXLINE 1024
#define BUFFER_SIZE 1024


/* prototypes */
void die(const char *);
void pdie(const char *);


int main(int argc, char **argv) {

    // TODO: Parse command-line options
    char *filename = "tubthumping.wav";

    // Open the WAVE file
    if (open_wave_file(&wf, filename) < 0)
        return -1;


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
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    //cliaddr.sin_family = AF_INET; // IPv4
    //cliaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "192.168.178.163", &(cliaddr.sin_addr));

    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int n;
    socklen_t len = sizeof(cliaddr);  //len is value/resuslt

    if (connect (sockfd, (const struct sockaddr *)&cliaddr,
            sizeof(cliaddr))){
              perror("connect");
              close(sockfd);
              exit(1);
            }

    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    struct timeval tv;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    // int preretval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
    // if (preretval < 0){
    //     printf("preretval < 0");
    //     return 0;
    // }
    // if (FD_ISSET(sockfd, &rfds)) {
    //     printf("SOCKET READABLE\n");
    // }

    n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                &len);
    buffer[n] = '\0';
    printf("Client : %s\n", buffer);

    if (connect (sockfd, (const struct sockaddr *)&cliaddr,
            sizeof(cliaddr))){
              perror("connect");
              close(sockfd);
              exit(1);
            }

    char sin_str[124];
    // now get it back and print it
    inet_ntop(AF_INET, &(cliaddr.sin_addr), sin_str, INET_ADDRSTRLEN);

    printf(" Clientaddr: %s\n", sin_str); // prints "192.0.2.33"

    // sendto(sockfd, (const char *)hello, strlen(hello),
    //     MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
    //         len);
    // printf("ACK message sent.\n");
    // char postsin_str[124];
    // //fcntl(sockfd, F_SETFL, O_NONBLOCK);
    // inet_ntop(AF_INET, &(cliaddr.sin_addr), postsin_str, INET_ADDRSTRLEN);
    //
    // printf("post hello Clientaddr: %s\n", postsin_str); // prints "192.0.2.33"


    // Sending data

    fprintf(stdout, "sample with bitsize %d\n",
                    wf.wh->w_bits_per_sample);
    fprintf(stdout, "samples per sec %d\n",
                    wf.wh->n_samples_per_sec);

    fprintf(stdout, "n channels %d\n",
                    wf.wh->n_channels);

    uint32_t numbersamples = (int) wf.payload_size / (wf.wh->n_channels * wf.wh->w_bits_per_sample / 8);

    fprintf(stdout, "n samples %i\n",
                    numbersamples);

    uint32_t samplesize = (wf.wh->w_bits_per_sample / 8) * wf.wh->n_channels;
    fprintf(stdout, "samplesize: %i\n",
                    samplesize);
    uint32_t currentsample = 0;
    uint8_t* currentp = wf.samples;

    const int samplesperpacket = 16;

    uint8_t* packetbuffer = malloc((samplesperpacket * samplesize) + sizeof(currentsample));

    // uint32_t datamax = 100000;
    // uint32_t currentdata = 0;
    //while (currentdata < datamax){






// ------------------------------------------------------------------------------------

// #define NUM_CHANNELS 2
// #define SAMPLE_RATE 44100
// #define BLOCK_SIZE 1024
// // 1 Frame = Stereo 16 bit = 32 bit = 4kbit
// #define FRAME_SIZE 4
//
//
//
// snd_pcm_t *snd_handle;
//
// int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
//
// if (err < 0) {
//     fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
//     return -1;
// }
//
// // Configure parameters of PCM output
// err = snd_pcm_set_params(snd_handle,
//                          SND_PCM_FORMAT_S16_LE,
//                          SND_PCM_ACCESS_RW_INTERLEAVED,
//                          NUM_CHANNELS,
//                          SAMPLE_RATE,
//                          0,              // Allow software resampling
//                          500000);        // 0.5 seconds latency
// if (err < 0) {
//     printf("couldnt configure audio device: %s\n", snd_strerror(err));
//     return -1;
// }
//
//
//
//



// ------------------------------------------------------------------------------------











    while (currentsample < numbersamples){

        memcpy(packetbuffer, &currentsample, sizeof(currentsample));
        //uint32_t i32 = packetbuffer[0] | (packetbuffer[1] << 8) | (packetbuffer[2] << 16) | (packetbuffer[3] << 24);
        //printf("packetbuf: %u, %u\n", packetbuffer[0], packetbuffer[1]);
        //printf("packeti32: %u\n", i32);

        //memcpy(packetbuffer + 4, &currentdata, sizeof(currentdata));

        //printf("sending currentsample: %u, currentdata, %u\n", currentsample, currentdata);
        //currentdata++;


        memcpy(packetbuffer + sizeof(currentsample), currentp, (samplesperpacket * samplesize));


        //snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle, packetbuffer + sizeof(currentsample), (samplesperpacket * samplesize) / FRAME_SIZE);

        int err;
        // FD_ZERO(&rfds);
        // FD_SET(sockfd, &rfds);
        // printf("select\n");
        // int retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        // printf("retval: %i", retval);
        // if (retval < 0){
        //     printf("retval < 0");
        //     return 0;
        // }
        // if (FD_ISSET(sockfd, &rfds)) {
        //     printf("SOCKET READABLE\n");
        //



        err = sendto(sockfd, packetbuffer, (samplesperpacket * samplesize) + sizeof(currentsample),
            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                len);
        char sendsin_str[124];
        // now get it back and print it
        inet_ntop(AF_INET, &(cliaddr.sin_addr), sendsin_str, INET_ADDRSTRLEN);

        //printf("sendloop Clientaddr: %s\n", sendsin_str); // prints "192.0.2.33"

        //printf("sendloop Clientaddr: %s", inet_ntoa(cliaddr));
        if (err < 0){
            if (err == EWOULDBLOCK){
                printf("WOULD BLOCK ERROR");
            }
            else {
                printf("SENDTO ERROR : %i", err);
            }
        }
        // }
        // sendto(sockfd, tempdatabuffer, 8,
        //     MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
        //         len);

        currentp += (samplesperpacket * samplesize);
        currentsample += samplesperpacket;




        //currentdata++;
        //printf("currentp: %u\n", currentp - wf.samples);

        if (currentsample % 10000 == 0){
            printf("Sample: %i / %i\n", currentsample, numbersamples);
        }
    }



  // for (j=0; j<NPACK; j++) {
  //   printf("Sending packet %d\n", j);
  //   sprintf(buf, "This is packet %d\n", j);
  //   if (sendto(s, buf, BUFLEN, 0, &si_other, slen)==-1)
  //   diep("sendto()");
  // }

  // for (i=0; i<NPACK; i++) {
  //   if (recvfrom(s, buf, BUFLEN, 0, &si_other, &slen)==-1)
  //     diep("recvfrom()");
  //   printf("Received packet from %s:%d\nData: %s\n\n",
  //   inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
  // }


  // TODO: Read and send audio data

  // Clean up
  close_wave_file(&wf);

  return 0;
}




/**********************************************************************
 * pdie --- Call perror() to figure out what's going on and die.
 **********************************************************************/

void pdie(const char *mesg) {

   perror(mesg);
   exit(1);
}


/**********************************************************************
 * die --- Print a message and die.
 **********************************************************************/

void die(const char *mesg) {

   fputs(mesg, stderr);
   fputc('\n', stderr);
   exit(1);
}
