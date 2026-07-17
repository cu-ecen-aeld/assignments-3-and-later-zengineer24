#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    usleep(thread_func_args->msObtainWait*1000);
    int ret = pthread_mutex_lock(thread_func_args->threadMutex);
    //if fail to acquire mutex set false and return
    if(ret != 0)
    {
            thread_func_args->thread_complete_success = false;
            return thread_param;
    }
    //Otherwise continue, unlock, and return success
    usleep(thread_func_args->msReleaseWait*1000);
    pthread_mutex_unlock(thread_func_args->threadMutex);
    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    //Allocate struct and set values from function arguments
    struct thread_data* threadProperties = (struct thread_data*)malloc(sizeof(struct thread_data));
    threadProperties->msObtainWait = wait_to_obtain_ms;
    threadProperties->msReleaseWait= wait_to_release_ms;
    threadProperties->threadMutex = mutex;

    //Attempt to create thread; report true if succesful, reclaim memory and false if not
     int ret = pthread_create(thread, NULL, threadfunc, (void*)threadProperties);
    
    if(ret == 0)
    {
        return true;
    }
    else
    {
        free(threadProperties);
    }

    return false;
}

