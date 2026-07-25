#define _GNU_SOURCE 1 // FIRST LINE OF FILE
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

static bool keepRunning = true;

void terminateHandler(int signum)
{
    keepRunning = false;
}

int main(int argc, char const *argv[])
{
    pid_t daemonPid;
    //setup signals and make sure register is successful
    struct sigaction terminateAction;
    bool success = true;
    memset(&terminateAction, 0, sizeof(terminateAction));
    terminateAction.sa_handler = terminateHandler;

    if(sigaction(SIGTERM, &terminateAction, NULL) != 0)
    {
        success = false;
        return success;
    }
    if(sigaction(SIGINT, &terminateAction, NULL) != 0)
    {
        success = false;
        return success;
    }

    //create and open socket to bind to port 9000
    int socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if(socketDesc == -1)
    {
        return -1;
    }

    struct sockaddr_in addrStruct;
    addrStruct.sin_family = AF_INET;
    addrStruct.sin_addr.s_addr = INADDR_ANY;
    addrStruct.sin_port = htons(9000);

    int optVal = 1;
    setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));
    int bindResult = bind(socketDesc, (struct sockaddr*)(&addrStruct), sizeof(addrStruct));
    if(bindResult == -1)
    {
        return -1;
    }

    //if -d flag asserted, start daemon
    if((argc == 2) && (strcmp(argv[1], "-d") == 0))
    {
        daemonPid = fork();
        if(daemonPid == -1)
        {
            return -1;
        } 
        else if(daemonPid != 0)
        {
            exit(EXIT_SUCCESS);
        }

        if(setsid() == -1)
        {
            return -1;
        }

        if(chdir("/") == -1)
        {
            return -1;
        }

        //open dev null to get file descriptor and copy std i/o to /dev/null. Using dup2 to do this explicitly since I don't want to close all open fds.
        int nullFd = open("/dev/null", O_RDWR);
        dup2(0, nullFd);
        dup2(1, nullFd);
        dup2(2, nullFd);
    }

    //open user log to use with syslog for error reporting
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_DEBUG, "Keep running");
    //open file to write data
    int dataFd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, 0644);
    if(dataFd == -1)
    {
        syslog(LOG_ERR, "Unable to open file /var/tmp/aesdsocketdata for writing");
        return -1;
    }

    //declare data buffer
    char dataArray[32768];
    int acceptFd;

    //Keep looping until signal is sent
    while(keepRunning)
    {

        struct sockaddr clientAddr;
        socklen_t sockLen = sizeof(struct sockaddr);
        //Listen and accept
        int listenResult = listen(socketDesc, 0);
        if(listenResult == -1)
        {
            syslog(LOG_ERR, "Failed to listen on socket!");
        }

        acceptFd = accept(socketDesc, &clientAddr, &sockLen);
        if(acceptFd == -1)
        {
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        struct sockaddr_in* clientIp= (struct sockaddr_in*)&clientAddr;
        char clientOctet[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &(clientIp->sin_addr), clientOctet, INET_ADDRSTRLEN);

        syslog(LOG_DEBUG, "Accepted connection from %s", (const char*)clientOctet);

        uint32_t dataSize = (sizeof(dataArray)/sizeof(dataArray[0]));
        memset(dataArray, '0', dataSize);
        int recvReturn = recv(acceptFd, dataArray, dataSize, 0);

        if((recvReturn == -1))
        {
            syslog(LOG_ERR, "Error receiving message from client!");
        }
        else
        {
            dataArray[recvReturn] = '\0';
        }

        lseek(dataFd, 0, SEEK_END);
        int writeReturn = write(dataFd, dataArray, recvReturn);
        if(writeReturn == -1)
        {
            syslog(LOG_ERR, "Failed to write data to file!");
        }

        lseek(dataFd, 0, SEEK_SET);
        memset(dataArray, '0', sizeof(dataArray));
        int readReturn = read(dataFd, dataArray, sizeof(dataArray));
        if(readReturn == -1)
        {
            syslog(LOG_ERR, "Failed to read file!");
        }

        int sendReturn = send(acceptFd, dataArray, readReturn, 0);
        if(sendReturn == -1)
        {
            syslog(LOG_ERR, "Failed to send data back to client!");
        }
        syslog(LOG_DEBUG, "Closed connection from %s", (const char*)clientOctet);
    
        //TODO: Close connection etc.
        close(acceptFd);
    }
    
    //cleanup on program termination
    syslog(LOG_DEBUG, "Caught signal, exiting");
    close(socketDesc);
    close(dataFd);
    remove("/var/tmp/aesdsocketdata");
    closelog();

    return 0;
}
