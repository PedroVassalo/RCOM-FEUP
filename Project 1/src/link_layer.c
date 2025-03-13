// Link layer protocol implementation

#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <signal.h>

// Definições das tramas
#define FLAG 0x7E
#define A 0x03  // Endereço
#define C_SET 0x03
#define C_UA  0x07
#define BCC_SET (A ^ C_SET)
#define BCC_UA  (A ^ C_UA)

#define TIMEOUT 3
#define MAX_RETRIES 3

int fd; // File descriptor da porta série

// Função que será chamada quando o alarme disparar (timeout)
void alarmHandler(int sig) {
    // Lidar com o timeout se necessário
}

// A função llopen é responsável por abrir a porta e estabelecer a comunicação
int llopen(LinkLayer connectionParameters) {
    struct termios oldtio, newtio;

    // 1️⃣ Abrir a porta série
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Erro ao abrir a porta série");
        return -1;
    }

    // 2️⃣ Guardar configuração atual e configurar nova
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("Erro ao obter atributos da porta");
        return -1;
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 1;

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("Erro ao configurar atributos da porta");
        return -1;
    }

    // 3️⃣ Enviar trama SET
    unsigned char set_frame[] = {FLAG, A, C_SET, BCC_SET, FLAG};
    int retries = 0;
    unsigned char response[5];

    // Configurar alarme para o timeout
    signal(SIGALRM, alarmHandler);
    
    while (retries < MAX_RETRIES) {
        write(fd, set_frame, 5);
        printf("SET enviado. Tentativa %d\n", retries + 1);

        // 4️⃣ Esperar resposta UA
        alarm(TIMEOUT);
        int res = read(fd, response, 5);
        alarm(0);

        if (res == 5 && response[0] == FLAG && response[1] == A && response[2] == C_UA && response[3] == BCC_UA && response[4] == FLAG) {
            printf("Recebido UA! Conexão estabelecida.\n");
            return fd; // Sucesso
        }

        retries++;
    }

    printf("Falha ao estabelecer ligação.\n");
    return -1;
}
////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
