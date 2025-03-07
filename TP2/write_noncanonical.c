// Write to serial port in non-canonical mode
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
#include <signal.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define MAX_RETRANSMISSIONS 3
#define TIMEOUT 3

// Frame and protocol constants
#define FLAG 0x7E
#define ADRESS_SENDER 0x03
#define ADRESS_RECEIVER 0x01
#define SET 0x03
#define UA 0x07

volatile int STOP = FALSE;
volatile int alarmEnabled = FALSE;
volatile int alarmCount = 0;
volatile int retransmissions = 0;

//Imprime os bytes do buffer no formato hexadecimal
void memdump(void *addr, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i)
        printf("%02x ", *((char*) addr + i));
}

// Alarm handler function to trigger retransmissions
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);

    // Se o número de retransmissões não ultrapassou MAX_RETRANSMISSIONS(3), retransmite o SET frame
    if (retransmissions < MAX_RETRANSMISSIONS) {
        printf("Retransmitting SET frame...\n");
        alarm(TIMEOUT);  // Restart the alarm
        alarmEnabled = TRUE;
        retransmissions++;
    }
}

int main(int argc, char *argv[]) {
    const char *serialPortName = argv[1];

    if (argc < 2) {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n", argv[0], argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio, newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 5;  // read() só retorna após receber 5 bytes

    // Flush any unwanted data
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Set alarm function handler
    signal(SIGALRM, alarmHandler);

    // Create SET frame to send
    unsigned char buf[BUF_SIZE] = {0};
    buf[0] = FLAG;
    buf[1] = ADRESS_SENDER;
    buf[2] = SET;
    buf[3] = ADRESS_SENDER ^ SET;
    buf[4] = FLAG;

    unsigned char expected[5] = {FLAG, ADRESS_RECEIVER, UA, ADRESS_RECEIVER ^ UA, FLAG};

    int bytes;

    // Send SET frame
    bytes = write(fd, buf, 5);
    printf("%d bytes written (SET frame): ", bytes);
    memdump(buf, 5);
    printf("\n");

    // Enable the alarm and retransmission logic
    alarm(TIMEOUT);
    alarmEnabled = TRUE;

    while (retransmissions < MAX_RETRANSMISSIONS) {
        printf("Waiting for UA frame...\n");
        bytes = read(fd, buf, 5); // Attempt to read the UA frame

        // Check if we received exactly 5 bytes (UA frame size)
        if (bytes == 5) {
            if (memcmp(buf, expected, 5) == 0) {
                printf("UA frame received: ");
                memdump(buf, 5);
                printf("\n");
                alarm(0); // Disable the alarm
                break; // Exit loop upon successful reception of UA
            } else {
                printf("Received frame, but it's not UA. Ignoring...\n");
            }
        }

        // Check if the alarm has sounded and we need to retransmit
        if (!alarmEnabled) {
            // Retransmit the SET frame
            printf("Retransmitting SET frame...\n");
            bytes = write(fd, buf, 5);
            printf("%d bytes retransmitted (SET frame): ", bytes);
            memdump(buf, 5);
            printf("\n");

            // Reset the alarm
            alarm(TIMEOUT);
            alarmEnabled = TRUE;
        }

        // Stop retransmissions if we've hit the limit
        if (alarmCount >= MAX_RETRANSMISSIONS) {
            printf("Maximum retransmissions reached, ending...\n");
            break;
        }
    }

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
