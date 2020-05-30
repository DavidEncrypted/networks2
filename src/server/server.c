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


#define DATA "Danger Will Roger . . ."
#define TRUE 1
#define SERVER_PORT 5001
#define BUFFER_SIZE 1024


/* prototypes */
void die(const char *);
void pdie(const char *);


int main(int argc, char **argv) {

  int sock;   /* fd for main socket */
  int msgsock;   /* fd from accept return */
  struct sockaddr_in server;   /* socket struct for server connection */
  struct sockaddr_in client;   /* socket struct for client connection */
  int clientLen;   /* returned length of client from accept() */
  int rval;   /* return value from read() */
  char buf[BUFFER_SIZE];   /* receive buffer */

  /* Open a socket, not bound yet.  Type is Internet TCP. */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  pdie("Opening stream socket");

  /*
  Prepare to bind.  Permit Internet connections from any client
  to our SERVER_PORT.
  */
  bzero((char *) &server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(SERVER_PORT);
  if (bind(sock, (struct sockaddr *) &server, sizeof(server)))
  pdie("Binding stream socket");

  printf("Socket has port %hu\n", ntohs(server.sin_port));

  listen(s, 5);


  /* Loop, waiting for client connections. */
  /* This is an interactive server. */
  while (TRUE) {

    clientLen = sizeof(client);
    if ((msgsock = accept(sock, (struct sockaddr *) &client,
                          &clientLen)) == -1)
       pdie("Accept");
    else {
       /* Print information about the client. */
       if (clientLen != sizeof(client))
          pdie("Accept overwrote sockaddr structure.");

       printf("Client IP: %s\n", inet_ntoa(client.sin_addr));
       printf("Client Port: %hu\n", ntohs(client.sin_port));

       do {   /* Read from client until it's closed the connection. */
          /* Prepare read buffer and read. */
          bzero(buf, sizeof(buf));
          if ((rval = read(msgsock, buf, BUFFER_SIZE)) < 0)
             pdie("Reading stream message");

          if (rval == 0)   /* Client has closed the connection */
             fprintf(stderr, "Ending connection\n");
          else
             printf("S: %s\n", buf);

          /* Write back to client. */
          if (write(msgsock, DATA, sizeof(DATA)) < 0)
             pdie("Writing on stream socket");

       } while (rval != 0);
    }   /* else */

    close(msgsock);
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


  close(s);


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
