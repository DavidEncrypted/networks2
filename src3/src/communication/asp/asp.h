/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */

/* TODO: Add function declarations for the public functions of your ASP implementation */

struct s_packet {
    uint32_t num_samples;
    uint32_t sample_number;
    uint16_t data_bytes;
    uint8_t compression_level;
    uint8_t samples_in_data;
    uint16_t checksum;
    uint8_t data[];
};

uint16_t calc_checksum(uint8_t* addr, uint32_t  count);
