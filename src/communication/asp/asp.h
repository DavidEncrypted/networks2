/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 *
 * Submission by: David Schep s2055961
 */

 // 0       7 8     15 16    23 24     31
 // +--------+--------+--------+--------+
 // |             Number of             |
 // |           total samples           |
 // +--------+--------+--------+--------+
 // |          Current sample           |
 // |              number               |
 // +--------+--------+--------+--------+
 // |   Number of     | Compr. | Samples|
 // |  data bytes     | level  | in data|
 // +--------+--------+--------+--------+
 // |    Checksum     |     Padding     |
 // |                 |                 |
 // +--------+--------+--------+--------+
 // |
 // |            Data bytes ...
 // +--------+-------- ...


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
