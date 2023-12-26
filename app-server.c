#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8085
#define BACKLOG 5

volatile sig_atomic_t sighupReceived = 0;

void sigHupHandler(int sigNumber) {
    sighupReceived = 1;
}

int main() {
    int serverFD;
    int incomingSocketFD = 0; 
    struct sockaddr_in socketAddress; 
    int addressLength = sizeof(socketAddress);
    fd_set readfds;
    struct sigaction sa;
    sigset_t blockedMask, origMask;
    char buffer[1024] = { 0 };
    int readBytes;
    int maxSd;
    int signalOrConnectionCount = 0;

    // Socket creating
    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("create error");
        exit(EXIT_FAILURE);
    }

    // Setting socket address parameters
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(PORT);

    // Socket binding to the address
    if (bind(serverFD, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // Started socket listenig
    if (listen(serverFD, BACKLOG) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d \n", PORT);

    // Signal handler registration
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // Setting up signal blocking
    sigemptyset(&blockedMask);
    sigemptyset(&origMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);
   
    while (signalOrConnectionCount < 3) {
        FD_ZERO(&readfds); 
        FD_SET(serverFD, &readfds); 
        
        if (incomingSocketFD > 0) { 
            FD_SET(incomingSocketFD, &readfds); 
        } 
        
        maxSd = (incomingSocketFD > serverFD) ? incomingSocketFD : serverFD; 
 
        if (pselect(maxSd + 1, &readfds, NULL, NULL, NULL, &origMask) < 0 && errno != EINTR) { 
            perror("pselect error"); 
            exit(EXIT_FAILURE); 
        }

        // Receiving SIGHUP signal check
        if (sighupReceived) {
            printf("SIGHUP received.\n");
            sighupReceived = 0;
            signalOrConnectionCount++;
            continue;
        }
    
        // Reading incoming bytes
        if (incomingSocketFD > 0 && FD_ISSET(incomingSocketFD, &readfds)) { 
            readBytes = read(incomingSocketFD, buffer, 1024);

            if (readBytes > 0) { 
                printf("Received data: %d bytes\n", readBytes); 
            } else {
                if (readBytes == 0) {
                    close(incomingSocketFD); 
                    incomingSocketFD = 0; 
                } else { 
                    perror("read error"); 
                } 
                signalOrConnectionCount++;   
            } 
            continue;
        }
        
        // Check of incoming connections
        if (FD_ISSET(serverFD, &readfds)) {
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
