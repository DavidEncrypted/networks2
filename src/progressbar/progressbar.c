// ALL CREDIT GOES TO GITHUB USER: amullins83
// https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include "progressbar.h"

void
print_progress(size_t count, size_t max)
{
	const char prefix[] = "Stream progress: [";
	const char suffix[] = "]";
	const size_t prefix_length = sizeof(prefix) - 1;
	const size_t suffix_length = sizeof(suffix) - 1;
	char *buffer = calloc(max + prefix_length + suffix_length + 1, 1); // +1 for \0
	size_t i = 0;

	strcpy(buffer, prefix);
	for (; i < max; ++i)
	{
		buffer[prefix_length + i] = i < count ? '#' : ' ';
	}

	strcpy(&buffer[prefix_length + i], suffix);
	printf("\b%c[2K\r%s", 27, buffer);
	fflush(stdout);
	free(buffer);
}
