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

#define BUF_SIZE 5

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_
} State;

State state = START;
unsigned char byte;

void memdump(void *addr, size_t bytes) {
    for (size_t i = 0; i < bytes; i++) {
        printf("%02x ", *((char*) addr + i));
    }
}

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
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

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    unsigned char buf[BUF_SIZE + 1] = {0};
    while (state != STOP) {
        int res = read(fd, &byte, 1);  

        if (res < 0) {
            perror("Error reading byte");
            break;
        }

        switch (state) {
            case START:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                if (byte == ADRESS_SENDER) {
                    state = A_RCV;
                } else if (byte != FLAG) {
                    state = START;  
                }
                break;

            case A_RCV:
                if (byte == SET) {
                    state = C_RCV;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;

            case C_RCV:
                if (byte == (SET ^ ADRESS_SENDER)) {
                    state = BCC_OK;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG) {
                    state = STOP_; 
                } else {
                    state = START;
                }
                break;

            default:
                state = START;
                break;
        }
    }

    sleep(1);

    unsigned char buf_answer[BUF_SIZE] = {0};

    buf_answer[0] = FLAG;
    buf_answer[1] = ADRESS_RECEIVER;
    buf_answer[2] = UA;
    buf_answer[3] = ADRESS_RECEIVER ^ UA;
    buf_answer[4] = FLAG;

    int bytes = write(fd, buf_answer, BUF_SIZE);
    printf("%d bytes written to answer\n", bytes);

    memdump(buf_answer, 5);
    printf("\n");

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
