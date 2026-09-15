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
#include "queue.h"
#include <pthread.h>
#include <time.h>

int keepRunning = 1;

//args passed to threads
typedef struct params {
    pthread_mutex_t* dataMutex;
    int acceptFd;
    int dataFd;
    char* clientOctet;
    int octetSize;
    int* keepRunning; 
}threadParams;

//linked list queue structure
typedef struct slist_data_s {
    pthread_t thread;
    SLIST_ENTRY(slist_data_s) entries;
}slist_data_t;

//forward declaration of threads for use in main
void* receiverThread(void* arg);
void timerThread(union sigval sv);

//helper function for timer
static inline void timespec_add( struct timespec *result, const struct timespec *ts_1, const struct timespec *ts_2)
{
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if( result->tv_nsec > 1000000000L ) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}

void terminateHandler(int signum)
{
    keepRunning = 0;
}

int main(int argc, char const *argv[])
{
    //variable declaration
    struct sigaction terminateAction;
    pid_t daemonPid;
    slist_data_t* connQueue = NULL;
    struct sockaddr_in addrStruct;
    int acceptFd;
    pthread_mutex_t dataMutex;
    int optVal = 1;
    //threadParams td;
    struct sigevent sev;
    timer_t timerid;
    struct itimerspec itimerspec;
    struct timespec start_time;
    threadParams* td = malloc(sizeof(threadParams));

    if (pthread_mutex_init(&dataMutex, NULL) != 0) 
    {
        return -1;
    }
    
    //setup thread linked list
    SLIST_HEAD(slisthead, slist_data_s) head;
    SLIST_INIT(&head);

    //setup signals and make sure register is successful
    memset(&terminateAction, 0, sizeof(terminateAction));
    terminateAction.sa_handler = terminateHandler;

    if(sigaction(SIGTERM, &terminateAction, NULL) != 0)
    {
        return -1;
    }
    if(sigaction(SIGINT, &terminateAction, NULL) != 0)
    {
        return -1;
    }

    //create and open socket to bind to port 9000
    int socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if(socketDesc == -1)
    {
        return -1;
    }

    addrStruct.sin_family = AF_INET;
    addrStruct.sin_addr.s_addr = INADDR_ANY;
    addrStruct.sin_port = htons(9000);

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
    //open file to write data
    int dataFd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, 0644);
    if(dataFd == -1)
    {
        syslog(LOG_ERR, "Unable to open file /var/tmp/aesdsocketdata for writing");
        return -1;
    }
    
    //Configure parameters for timer
    td->dataMutex = &dataMutex;
    td->dataFd = dataFd;

    int clock_id = CLOCK_MONOTONIC;
    memset(&sev,0,sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = td;
    sev.sigev_notify_function = timerThread;
    if ( timer_create(clock_id,&sev,&timerid) != 0 ) 
    {
            return -1;
    } 
     if ( clock_gettime(clock_id,&start_time) != 0 ) 
     {
        return -1;
    } 
    else 
    {
            memset(&itimerspec, 0, sizeof(struct itimerspec));
            itimerspec.it_interval.tv_sec = 10;
            itimerspec.it_interval.tv_nsec = 0 * 1000000;
            itimerspec.it_value.tv_sec = 2;
            itimerspec.it_value.tv_nsec = 0;
            timespec_add(&itimerspec.it_value,&start_time,&itimerspec.it_interval);
    }

    bool first = true;
    //Keep looping until signal is sent
    while(keepRunning == 1)
    {
        struct sockaddr clientAddr;
        socklen_t sockLen = sizeof(struct sockaddr);
        
        //Listen
        int listenResult = listen(socketDesc, 30);
        if(listenResult == -1)
        {
            syslog(LOG_ERR, "Failed to listen on socket!");
        }

        //Accept
        acceptFd = accept(socketDesc, &clientAddr, &sockLen);
        if(acceptFd == -1)
        {
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        char clientOctet[INET_ADDRSTRLEN];
        struct sockaddr_in* clientIp= (struct sockaddr_in*)&clientAddr;

        inet_ntop(AF_INET, &(clientIp->sin_addr), clientOctet, INET_ADDRSTRLEN);

        //setup args for receiver thread
        //start thread and add entry to queue
        threadParams* generalParams = malloc(sizeof(threadParams));
        generalParams->acceptFd = acceptFd;
        generalParams->clientOctet = clientOctet;
        generalParams->octetSize = INET_ADDRSTRLEN;
        generalParams->dataFd = dataFd;
        generalParams->dataMutex = &dataMutex;
        
        connQueue = malloc(sizeof(slist_data_t));
        pthread_create(&connQueue->thread, NULL, receiverThread, generalParams);
        SLIST_INSERT_HEAD(&head, connQueue, entries);

        //Start timer
        if(first)
        {
            if( timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL ) != 0 ) 
            {
                syslog(LOG_ERR, "Failed to set timer.");
            }
            first = false;
        }

        //Allocate entry for use with slist safe call
        slist_data_t* tempVar;

        //Iterate through all queue entries and check if threads can be joined.
        //Free malloced memory if joined
        SLIST_FOREACH_SAFE(connQueue, &head, entries, tempVar)
        {
            int retvalue = pthread_tryjoin_np(connQueue->thread, NULL);
            if(retvalue == 0)
            {
                SLIST_REMOVE(&head, connQueue, slist_data_s, entries);
                free(connQueue);
            }
        }
    }   

    //Allocate entry for use with slist safe call
    while(!SLIST_EMPTY(&head))
    {
        connQueue = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        int retvalue = pthread_join(connQueue->thread, NULL);
        free(connQueue);
    }
    
    syslog(LOG_DEBUG, "Caught signal, exiting");
    close(socketDesc);
    close(dataFd);
    remove("/var/tmp/aesdsocketdata");
    closelog();
    timer_delete(timerid);
    free(td);
    return 0;
}

void* receiverThread(void* arg) 
{
    syslog(LOG_DEBUG, "Starting receiver thread.");
    threadParams* params = ((threadParams*)arg);
    //declare data buffer
    char dataArray[32768];
    int readReturn;
    uint32_t dataSize = (sizeof(dataArray)/sizeof(dataArray[0]));
    memset(dataArray, '0', dataSize);
    int recvReturn = recv(params->acceptFd, dataArray, dataSize, 0);

    if((recvReturn == -1))
    {
        syslog(LOG_ERR, "Error receiving message from client!");
    }
    else
    {
        dataArray[recvReturn] = '\0';
    }

    //add mutex
    int mutexResult = pthread_mutex_lock(params->dataMutex);
    if ( mutexResult != 0 ) 
    {
        syslog(LOG_ERR, "pthread_mutex_lock failed\n");
    } else 
    {
        lseek(params->dataFd, 0, SEEK_END);
        int writeReturn = write(params->dataFd, dataArray, recvReturn);
        if(writeReturn == -1)
        {
            syslog(LOG_ERR, "Failed to write data to file!");
        }

        lseek(params->dataFd, 0, SEEK_SET);
        memset(dataArray, '0', sizeof(dataArray));
        readReturn = read(params->dataFd, dataArray, sizeof(dataArray));
        if(readReturn == -1)
        {
            syslog(LOG_ERR, "Failed to read file!");
        }
    }
        //release mutex
    mutexResult = pthread_mutex_unlock(params->dataMutex);
    if(mutexResult != 0)
    {
        syslog(LOG_ERR, "pthread_mutex_unlock failed\n");
    }

    int sendReturn = send(params->acceptFd, dataArray, readReturn, 0);
    if(sendReturn == -1)
    {
        syslog(LOG_ERR, "Failed to send data back to client!");
    }

    //TODO: Close connection etc.
    syslog(LOG_DEBUG, "Closed connection from %s", (const char*)params->clientOctet);
    close(params->acceptFd);
    free(params);
    return 0;
}

void timerThread(union sigval sv)
{
    threadParams *params=(threadParams*)sv.sival_ptr;
    time_t timeNs = 0;

    char timeString[64] = "timestamp:"; 
    struct tm timeData;
    if(localtime_r(&timeNs, &timeData) != NULL)
    {
        int stringSize = strftime(timeString + 10, sizeof(timeString) - 10, "%a %b %d %H:%M:%S %Y\n\n", &timeData);
        int mutexResult = pthread_mutex_lock(params->dataMutex);
        if ( mutexResult != 0 ) 
        {
            syslog(LOG_ERR, "pthread_mutex_lock failed with %d\n", mutexResult);
        } else 
        {
            lseek(params->dataFd, 0, SEEK_END);

            //+10 is for timestamp: text
            int writeReturn = write(params->dataFd, timeString, stringSize+10);
            if(writeReturn == -1)
            {
                syslog(LOG_ERR, "Failed to write data to file!");
            }
        }
        mutexResult = pthread_mutex_unlock(params->dataMutex);
        if(mutexResult != 0)
        {
            syslog(LOG_ERR, "pthread_mutex_unlock failed with %d\n", mutexResult);
        }
    }
}