/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include <math.h>

#include "asp.h"

/* An asp socket descriptor for information about the sockets current state */
struct asp_socket_info {
    int sockfd;

    struct sockaddr_in local_addr;
    socklen_t local_addrlen;

    struct sockaddr_in remote_addr;
    socklen_t remote_addrlen;

    struct asp_socket_info *next;

    int current_quality_level;
    int sequence_count;

    int packets_received;
    int packets_missing;

    struct timeval last_packet_received;
    struct timeval last_problem;

    unsigned int is_connected : 1;
    unsigned int has_accepted : 1;
};
