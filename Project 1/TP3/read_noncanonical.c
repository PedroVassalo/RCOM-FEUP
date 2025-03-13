// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1

#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define ADRESS_SENDER 0x03
#define ADRESS_RECEIVER 0x01
#define SET 0x03
#define UA 0x07
#define I_FRAME 0x08  // Control byte for I-frame

#define BUF_SIZE 256

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    DATA_RCV,
    BCC_OK,
    STOP_
} State;

State state = START;
unsigned char byte;
unsigned char frame[BUF_SIZE];  // Buffer to store received frame
int frame_index = 0;  // Current index in the frame buffer

// Function to compute the BCC (XOR checksum)
unsigned char calculate_bcc(unsigned char *data, int length) {
    unsigned char bcc = 0;
    for (int i = 0; i < length; i++) {
        bcc ^= data[i];
    }
    return bcc;
}

void memdump(void *addr, size_t bytes) {
    for (size_t i = 0; i < bytes; i++) {
        printf("%02x ", *((char*) addr + i));
    }
}

volatile int STOP = FALSE;

int main(int argc, char *argv[]) {
    const char *serialPortName = argv[1];

    if (argc < 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n", argv[0], argv[0]);
        exit(1);
    }

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    // Receive frame processing loop
    while (state != STOP) {
        int res = read(fd, &byte, 1);  // Read byte from serial port

        if (res < 0) {
            perror("Error reading byte");
            break;
        }

        // Process received byte based on state machine
        switch (state) {
            case START:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                    frame_index = 0;  // Start a new frame
                    frame[frame_index++] = byte;  // Store the flag
                }
                break;

            case FLAG_RCV:
                if (byte == ADRESS_SENDER) {
                    state = A_RCV;
                    frame[frame_index++] = byte;  // Store address byte
                } else if (byte != FLAG) {
                    state = START;  // Not the expected start byte, restart
                } else {
                    frame[frame_index++] = byte;  // Store the flag byte again
                }
                break;

            case A_RCV:
                frame[frame_index++] = byte;  // Store address byte
                if (byte == I_FRAME) {  // Check for I-frame
                    state = C_RCV;  // Expected control byte for I-frame
                } else {
                    state = START;
                }
                break;

            case C_RCV:
                frame[frame_index++] = byte;  // Store control byte
                state = DATA_RCV;  // Move to data receiving
                break;

            case DATA_RCV:
                frame[frame_index++] = byte;  // Store data byte
                if (frame_index >= BUF_SIZE - 1) {
                    state = BCC_OK;  // If we have received enough data, move to BCC check
                }
                break;

            case BCC_OK:
                unsigned char bcc = calculate_bcc(frame, frame_index - 1);  // Calculate BCC excluding the flag bytes
                if (bcc == byte) {
                    printf("BCC OK, frame received successfully: ");
                    memdump(frame, frame_index);
                    printf("\n");
                    state = STOP_;
                } else {
                    printf("BCC error\n");
                    state = START;  // BCC mismatch, restart frame
                }
                break;

            default:
                state = START;  // Reset state if something goes wrong
                break;
        }
    }

    // Send UA frame (Unnumbered Acknowledgment)
    unsigned char buf_answer[5] = {0};
    buf_answer[0] = FLAG;
    buf_answer[1] = ADRESS_RECEIVER;
    buf_answer[2] = UA;
    buf_answer[3] = ADRESS_RECEIVER ^ UA;
    buf_answer[4] = FLAG;

    int bytes = write(fd, buf_answer, 5);
    printf("%d bytes written to answer\n", bytes);
    memdump(buf_answer, 5);
    printf("\n");

    // Restore old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
