#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8085

// Обработчик сигнала
volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) {
    wasSigHup = 1;
}

int main() {
    int serverFD;
    int incomingSocketFD = 0; 
    struct sockaddr_in socketAddress; 
    int addressLength = sizeof(socketAddress);
    char buf[1024] = { 0 };
    int readBytes;
    int count = 0;

    // Создание сокета
    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("create error");
        exit(EXIT_FAILURE);
    }

    // Структура адреса сервера
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(PORT);

    // Привязка сокета к указанному адресу
    if (bind(serverFD, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // Подготовка к прослушиванию соединений
    if (listen(serverFD, 0) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d \n", PORT);

    // Регистрация обработчика сигнала
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // Блокировка сигнала
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigemptyset(&origMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);
   
    fd_set fds;
    int maxFD;
    while (count < 3) {
        FD_ZERO(&fds); 
        FD_SET(serverFD, &fds); 
        
        if (incomingSocketFD > 0) { 
            FD_SET(incomingSocketFD, &fds); 
        } 
        
        maxFD = (incomingSocketFD > serverFD) ? incomingSocketFD : serverFD; 
 
        if (pselect(maxFD + 1, &fds, NULL, NULL, NULL, &origMask) == -1) { 
            if (errno == EINTR) {
                if (wasSigHup) {
                    printf("Received SIGHUP.\n");
                    wasSigHup = 0;
                    count++;
                    continue;
                }
            } else {
                perror("pselect error\n");
                exit(EXIT_FAILURE);
              }
        }
    
        // Чтение данных входящего соединения
        if (incomingSocketFD > 0 && FD_ISSET(incomingSocketFD, &fds)) { 
            readBytes = read(incomingSocketFD, buf, 1024);

            if (readBytes > 0) { 
                printf("Received data: %d bytes\n", readBytes); 
            } else {
                if (readBytes == 0) {
                    close(incomingSocketFD); 
                    incomingSocketFD = 0; 
                } else { 
                    perror("read error"); 
                } 
                count++;   
            } 
            continue;
        }
        
        // Проверка наличия входящих соединений
        if (FD_ISSET(serverFD, &fds)) {
            if ((incomingSocketFD = accept(serverFD, (struct sockaddr*)&socketAddress, (socklen_t*)&addressLength)) < 0) {
                perror("accept error");
                exit(EXIT_FAILURE);
            }

            printf("New connection.\n");
        }
    }

    close(serverFD);

    return 0;
}
