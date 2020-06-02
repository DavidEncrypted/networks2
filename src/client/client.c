/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */


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

#include "../communication/asp/asp.h"

#define BIND_PORT 1235



#define PORT 5001
#define MAXLINE 2048

/* prototypes */
void die(const char *);
void pdie(const char *);


#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024
// 1 Frame = Stereo 16 bit = 32 bit = 4kbit
#define FRAME_SIZE 4


int main(int argc, char **argv) {

    unsigned blocksize = 0;





    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Setup Tubthumping";
    struct sockaddr_in     servaddr;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));
    int n;
    socklen_t len;


    char serv_sin_str[124];
    // now get it back and print it
    inet_ntop(AF_INET, &(servaddr.sin_addr), serv_sin_str, INET_ADDRSTRLEN);

    printf(" Clientaddr: %s\n", serv_sin_str); // prints "192.0.2.33"


    sendto(sockfd, (const char *)hello, strlen(hello),
        MSG_CONFIRM, (const struct sockaddr *) &servaddr,
            sizeof(servaddr));
    printf("Setup message sent.\n");

    char setserv_sin_str[124];
    // now get it back and print it
    inet_ntop(AF_INET, &(servaddr.sin_addr), setserv_sin_str, INET_ADDRSTRLEN);

    printf(" Clientaddr: %s\n", setserv_sin_str); // prints "192.0.2.33"

    n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                MSG_WAITALL, (struct sockaddr *) &servaddr,
                &len);
    buffer[n] = '\0';
    printf("Server : %s\n", buffer);

















    // Open audio device
    snd_pcm_t *snd_handle;

    int err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    if (err < 0) {
        fprintf(stderr, "couldnt open audio device: %s\n", snd_strerror(err));
        return -1;
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
        return -1;
    }

    uint8_t* packetbuffer = malloc(MAXLINE);
    uint32_t frame_n;
    uint32_t prev_frame_n;
    // set up buffers/queues
    uint8_t* recvbuffer = malloc(BLOCK_SIZE);
    uint8_t* playbuffer = malloc(BLOCK_SIZE);
    uint8_t* recv_ptr = recvbuffer;

    int skippedframes = 0;

    for (int k = 0; k < 4000; k++){
        do {
            //printf("wait receive\n");
            n = recvfrom(sockfd, packetbuffer, 68,
                        MSG_WAITALL, (struct sockaddr *) &servaddr,
                        &len);
            //printf("frame size: %i\n", n);

            char recvserv_sin_str[124];
            // now get it back and print it
            inet_ntop(AF_INET, &(servaddr.sin_addr), recvserv_sin_str, INET_ADDRSTRLEN);

            printf("recv Clientaddr: %s\n", recvserv_sin_str); // prints "192.0.2.33"

            if (n < 0) {
                printf("FUCKING FAILED, n= %i\n", n);
                continue;
                //exit(EXIT_FAILURE);
            }
            //if (n != 8) {printf("EXCUSEME"); exit(EXIT_FAILURE);}
            //uint32_t frame_n = *(packetbuffer);

            frame_n = packetbuffer[0] | (packetbuffer[1] << 8) | (packetbuffer[2] << 16) | (packetbuffer[3] << 24);
            if (frame_n != prev_frame_n + 16) {
                skippedframes += frame_n - (prev_frame_n);
                //printf("Frame skipped/out of order: prev: %u, current: %u\n", prev_frame_n, frame_n);
            }
            //printf("Framenum: %u\n", frame_n);
            memcpy(recv_ptr, packetbuffer + 4, n - 4);
            //blocksize += n;
            recv_ptr += n - 4;

            prev_frame_n = frame_n;

            //printf("recv_ptr - recvbuffer : %li\n", recv_ptr - recvbuffer);
        } while (recv_ptr - recvbuffer < BLOCK_SIZE); // 256 samples   44100 / s
    printf(" frames skipped per 256 samples: %i\n", skippedframes);
    skippedframes = 0;
    snd_pcm_sframes_t preframes = snd_pcm_writei(snd_handle, recvbuffer, (recv_ptr - recvbuffer) / FRAME_SIZE);
    recv_ptr = recvbuffer;
    printf("PREFILLED FRAMES: %i\n", preframes);
    if (preframes != 256) {printf("OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOoY\n");
        exit(1);

        }
    }
    //TODO: fill the buffer

    // Play
    printf("playing...\n");

    int i = 0;

    uint8_t* play_ptr;
    while (false) {
        if (i <= 0) {
            // TODO: get sample
            blocksize = recv_ptr - recvbuffer;
            memcpy(playbuffer, recvbuffer, blocksize);
            recv_ptr = recvbuffer;

            play_ptr = playbuffer;
            i = blocksize;
        }
        printf("start write to ALSA");
        // write frames to ALSA

        snd_pcm_sframes_t frames;
        if (true){
            frames = snd_pcm_writei(snd_handle, play_ptr, (blocksize - (*play_ptr - *playbuffer)) / FRAME_SIZE);
        }
        else {
            frames = blocksize / FRAME_SIZE;
            printf("fake frames: %i\n", frames);
            for (int x = 0; x < (blocksize - (*play_ptr - *playbuffer)); x += 4){
                uint32_t datanum = play_ptr[x] | (play_ptr[x+1] << 8) | (play_ptr[x+2] << 16) | (play_ptr[x+3] << 24);
                printf("datanum: %u\n", datanum);
            }
        }

        printf("Write expected %i : %li\n", (blocksize - (*play_ptr - *playbuffer)) / FRAME_SIZE, frames);

        printf("snd_pcm_avail %d \n", snd_pcm_avail(snd_handle));
        printf("snd_pcm_status_get_avail %d \n", snd_pcm_status_get_avail(snd_handle));
        // Check for errors
        int ret = 0;
        if (frames < 0)
            ret = snd_pcm_recover(snd_handle, frames, 0);
        if (ret < 0) {
            fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %i\n", ret);
            exit(EXIT_FAILURE);
        }
        if (frames > 0 && frames < (blocksize - (*play_ptr - *playbuffer)) / FRAME_SIZE)
            printf("Short write (expected %i, wrote %li)\n", (blocksize - (*play_ptr - *playbuffer)) / FRAME_SIZE, frames);

        // advance pointers accordingly
        if (frames > 0) {
            play_ptr += frames * FRAME_SIZE;
            i -= frames * FRAME_SIZE;
        }

        if ((unsigned) (*play_ptr - *playbuffer) == blocksize)
            i = 0;


        // TODO: try to receive a block from the server?

        do {
            n = recvfrom(sockfd, packetbuffer, MAXLINE,
                        MSG_WAITALL, (struct sockaddr *) &servaddr,
                        &len);
            //printf("frame size: %i\n", n);
            if (n < 0) {
                printf("FUCKING FAILED, n= %i", n);
                exit(EXIT_FAILURE);
            }
            //uint32_t frame_n = *(packetbuffer);







            frame_n = packetbuffer[0] | (packetbuffer[1] << 8) | (packetbuffer[2] << 16) | (packetbuffer[3] << 24);
            //printf("Framenum: %u\n", frame_n);
            memcpy(recv_ptr, packetbuffer+4, n - 4);
            //blocksize += n;
            recv_ptr += n - 4;
            //printf("recv_ptr - recvbuffer : %li\n", recv_ptr - recvbuffer);
        } while (recv_ptr - recvbuffer < BLOCK_SIZE);

    }
    //err = wait_for_poll(snd_handle, ufds, count);
    printf("start 5 sec sleep");
    sleep(5);

    // clean up
    free(recvbuffer);
    free(playbuffer);

    snd_pcm_drain(snd_handle);
    snd_pcm_hw_free(snd_handle);
    snd_pcm_close(snd_handle);

    close(sockfd);

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
