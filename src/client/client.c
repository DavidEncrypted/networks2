/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <alsa/asoundlib.h>

#include "../communication/asp/asp.h"

#define BIND_PORT 1235


#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024
// 1 Frame = Stereo 16 bit = 32 bit = 4kbit
#define FRAME_SIZE 4

int main(int argc, char **argv) {
    //int buffer_size = 1024;
    int bind_port = BIND_PORT;
    
    unsigned blocksize = 0;


    // TODO: Parse command-line options

    // TODO: Set up network connection

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

    // set up buffers/queues
    uint8_t* recvbuffer = malloc(BLOCK_SIZE);
    uint8_t* playbuffer = malloc(BLOCK_SIZE);

    //TODO: fill the buffer

    // Play
    printf("playing...\n");

    int i = 0;
    uint8_t* recv_ptr = recvbuffer;
    uint8_t* play_ptr;
    while (true) {
        if (i <= 0) {
            // TODO: get sample

            play_ptr = playbuffer;
            i = blocksize;
        }

        // write frames to ALSA
        snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, play_ptr, (blocksize - (*play_ptr - *playbuffer)) / FRAME_SIZE);

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

    }

    // clean up
    free(recvbuffer);
    free(playbuffer);

    snd_pcm_drain(snd_handle);
    snd_pcm_hw_free(snd_handle);
    snd_pcm_close(snd_handle);

    return 0;
}
