// write_noncanonical.c
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
#define C_NS0 0x00  // Control for NS0
#define C_NS1 0x40  // Control for NS1

volatile int STOP = FALSE;
volatile int alarmEnabled = FALSE;
volatile int alarmCount = 0;
volatile int retransmissions = 0;

// Imprime os bytes do buffer no formato hexadecimal
void memdump(void *addr, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i)
        printf("%02x ", *((char*) addr + i));
}

// Alarm handler function to trigger retransmissions
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);

    // Se o número de retransmissões não ultrapassou MAX_RETRANSMISSIONS(3), retransmite a Trama I
    if (retransmissions < MAX_RETRANSMISSIONS) {
        printf("Retransmitting Trama I...\n");
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

    // Criar a Trama I para enviar
    unsigned char buf[BUF_SIZE] = {0};
    buf[0] = FLAG;
    buf[1] = ADRESS_SENDER;
    buf[2] = C_NS0;  // Pode ser C_NS0 ou C_NS1 dependendo do número de sequência
    buf[3] = buf[1] ^ buf[2];  // BCC1 (XOR de ADDRESS e CONTROL)
    buf[4] = 'H';  // Exemplo de dado (dados podem ser ajustados conforme necessário)
    buf[5] = 'e';
    buf[6] = 'l';
    buf[7] = 'l';
    buf[8] = 'o';  // Dado de exemplo, pode ser o que você precisar
    buf[9] = FLAG;

    unsigned char expected[5] = {FLAG, ADRESS_RECEIVER, C_NS1, ADRESS_RECEIVER ^ C_NS1, FLAG}; // UA frame

    int bytes;

    // Enviar Trama I
    bytes = write(fd, buf, 10);
    printf("%d bytes written (Trama I): ", bytes);
    memdump(buf, 10);
    printf("\n");

    // Ativar alarme e lógica de retransmissão
    alarm(TIMEOUT);
    alarmEnabled = TRUE;

    while (retransmissions < MAX_RETRANSMISSIONS) {
        printf("Waiting for UA frame...\n");
        bytes = read(fd, buf, 5); // Tentar ler o frame UA

        // Verificar se recebemos exatamente 5 bytes (tamanho do frame UA)
        if (bytes == 5) {
            if (memcmp(buf, expected, 5) == 0) {
                printf("UA frame received: ");
                memdump(buf, 5);
                printf("\n");
                alarm(0); // Desativar o alarme
                break; // Sair do loop ao receber UA com sucesso
            } else {
                printf("Received frame, but it's not UA. Ignoring...\n");
            }
        }

        // Verificar se o alarme soou e precisamos retransmitir
        if (!alarmEnabled) {
            // Retransmitir a Trama I
            printf("Retransmitting Trama I...\n");
            bytes = write(fd, buf, 10);
            printf("%d bytes retransmitted (Trama I): ", bytes);
            memdump(buf, 10);
            printf("\n");

            // Resetar o alarme
            alarm(TIMEOUT);
            alarmEnabled = TRUE;
        }

        // Parar as retransmissões se atingirmos o limite
        if (alarmCount >= MAX_RETRANSMISSIONS) {
            printf("Maximum retransmissions reached, ending...\n");
            break;
        }
    }

    // Esperar até que todos os bytes tenham sido escritos na porta serial
    sleep(1);

    // Restaurar as configurações anteriores da porta
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
