This repo shows the code for an assignment for the course Networks at the university of Leiden's Bachelor of Computer Science.
The goal was to first design a streaming protocol. This protocol is documented in `protocol.txt`. Then the protocol was to be implemented using C.
The repo contains a client and server implementation. Further explaination of the code and functionality can be viewed in the pdf file `networks2.pdf`.



# Usage

Compile using `make`

Clean binary files using `make clean`

This code was writen on linux and compiled using gcc

Client Arguments:
```
./client -b [Size of buffer]

-b [Size of buffer] // This is the size of the buffer to fill before we can start playing the audio
                    // Not required has a default of 65536
```
Server Arguments:
```
./server -f [Filename]

-f [Filename] // This is filename of the .wav audiofile to play
              // Required
```
The repo contains tubthumping.wav. This can be used to test the server: `-f tubthumping.wav`

# Info

Data is sent using this packet layout:
```

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
 ```
```
Number of total samples: The number of total samples in the entire audio file
Current sample number: The sample number of the first sample of the data bytes
Number of data bytes: Number of bytes in the data bytes field
Compr. level: The current compression level
Samples in data: How many samples are in the data bytes field
Checksum: The Internet checksum of the entire packet with checksum=0
Padding: Necessary to pad to 32 bytes increments
Data bytes: The actual data bytes
```

```
The struct to store this packet layout in:
struct s_packet {
    uint32_t num_samples;         // Number of total samples
    uint32_t sample_number;       // Current sample number
    uint16_t data_bytes;          // Number of data bytes
    uint8_t compression_level;    // Compr. level
    uint8_t samples_in_data;      // Samples in data
    uint16_t checksum;            // Checksum
    uint8_t data[];               // Data bytes
};
```

```
Example of how the client en server connect and start streaming

Client            Server
  |                 |
Start           Start waiting
trying to       for start command
send start          |
command             |
  |  ----Start-->   |
  |                 R
  |             Start command recieved
  |                 |
  |              Start sending data packets
  |   <--Data----   |
  R                 |
First data          |
packet recieved     |
Start recieve loop  |
  |                 |
  |                 |
  |   <--Data----   |
  R                 |
          ...
  |                 |
  |   <--Data----   |
  R                 |
  |                 |
Check connection    |
quality             |
  |                 |
Send compression_level
request             |
  |                 |
  |--Compr_level--> |
  |                 R
  |               update compression level
  |                continue sending data packets
          ...

Client connection steps:
1. Start sending start to server
  Repeat until recieve
2. Start main loop
    3. Recieve packet
    4. Handle packet
      5. if only 1 byte recieved == 1, sending finished, exit loop
    6, If i % ERROR_DETECTION_WAIT_BLOCK do error-detection and compression level update


Server connection steps
1. Wait for client start packet
2. Start main send loop
    3. Check if can recieve
        4. Recieve compression level update -> update compression level
    4. Send data packets
    5. If last data packet sent, exit loop
6. Send 1 byte which equals 1 to tell the client it is done sending
```

DEFINE DIRECTIVES
```
--------------------------------------------------------------------------------
dataclient.c:

#define OPTSTR "b:"
Don't change - it tells the argument parser to parse -b

#define SAMPLE_SIZE 4
Don't change - the number of bytes in a single non-compressed sample

#define LOCALPORT 1235
#define SERVERPORT 1234
The local/client port and the server port
Can be changed, but need to be the same on the server side

#define SERVER_IP "127.0.0.2"
#define CLIENT_IP "127.0.0.1"
The client and server ip, can be changed if the programs are not on the same linux system

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
Don't change - The number of channels and the sample rate of the WAV file

#define MAX_PACKET_SIZE 16 + (SAMPLE_SIZE * 256) // Hardcoded max packet size
This is the maximum size a packet can get - 16 bytes for the header + 256 samples * number of bytes in 1 sample

#define COMPRESSION_UPDATING_ENABLED 1
If this is 1 the client will send compression level requests to the server according to the avg packet droprate.

#define ERROR_DETECTION_WAIT_BLOCK 32896
How many samples should the error detection algorithm wait between checks

#define ROLLING_AVERAGE_LENGTH 4
Don't change - Hardcoded rolling average length


--------------------------------------------------------------------------------
slowserver.c
#define SERVERPORT 1234
#define LOCALPORT 1235
The local/client port and the server port
Can be changed, but need to be the same on the client side

#define SERVER_IP "127.0.0.2"
#define CLIENT_IP "127.0.0.1"
The client and server ip, can be changed if the programs are not on the same linux system

#define OPTSTR "f:"
Don't change - it tells the argument parser to parse -f

#define SAMPLE_SIZE 4
Don't change - the number of bytes in a single non-compressed sample

#define SAMPLES_PER_PACKET 64
The number of samples per packet - Default: 64 Max: 252  --- Must be multiple of 4 (uint8_t max 255)

#define STARTING_COMPRESSION 0
The compression level the server will start at when sending data

// worst -- perfect
//   1   --  5
#define CONNECTION_QUALITY 5
The connection quality, worst means more delay and drops. Perfect means no delay or drops

#define PACKET_DELAY_LENGTH 200000 // microseconds
The delay in microsecond for the server to wait when delaying a packet
--------------------------------------------------------------------------------
```

Both main functions are very long, this is because many variables are created and necessary in many parts of main
To now put parts of main into a function would give the function way to many parameters and would make main even less readable
Sadly this is a design mistake at the start and I should have taken more time to design a system that does not rely on this many variables

I would have liked to have the audio player show a progressbar. But this progressbar would have interfered with the streaming progressbar.
And would have created a lot of difficulty with the multithreading so I decided it was beter to not implement this.
Maybe it would have been possible using ncurses. But that is out of the scope of this project


--------------------------------------------------------------------------------
The code inside progressbar/progressbar.c is not mine but created by github user amullins83
https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499

The internet checksum implementation is taken from https://www.csee.usf.edu/~kchriste/tools/checksum.c
