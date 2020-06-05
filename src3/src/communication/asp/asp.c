/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include <math.h>

#include "asp.h"



uint16_t calc_checksum(uint8_t* addr, uint32_t  count){ // https://www.csee.usf.edu/~kchriste/tools/checksum.c
  register uint32_t sum = 0;

  // Main summing loop
  while(count > 1)
  {
    sum = sum + *((uint16_t *) addr);
    addr += 2;
    count = count - 2;
  }

  // Add left-over byte, if any
  if (count > 0)
    sum = sum + *((uint8_t *) addr);

  // Fold 32-bit sum to 16 bits
  while (sum>>16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return(~sum);
}
