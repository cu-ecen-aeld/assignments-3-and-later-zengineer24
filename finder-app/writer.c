#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char const *argv[])
{

    //open user log to use with syslog for error reporting
    openlog(NULL, 0, LOG_USER);

    //check argument count
    //Program name is considered first argument so we are actually expecting 3.
    if(argc != 3) 
    {
        syslog(LOG_ERR, "%d arguments provided when 2 were expected.", argc);
        return 1;
    }

    //save arguments for future use
    const char* filesdir = argv[1];
    int writeLen = strlen(argv[2]);
    //char writeStr[writeLen];
    const char* writestr = argv[2];

    //open file, report and exit if failed
    int fd = open((const char*)filesdir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd == -1)
    {
        syslog(LOG_ERR, "Unable to open file %s for writing", filesdir);
        return 1;
    }

    //write file, report and exit if failed
    syslog(LOG_DEBUG, "Writing %s to %s", filesdir, writestr);
    int writeSize = write(fd, (const void*)writestr, writeLen);
    if(writeSize == -1)
    {
        syslog(LOG_ERR, "Unable to write string '%s' to file", writestr);
        return 1;
    }

    //close file, report and exit if failed
    if(close(fd) == -1)
    {
        syslog(LOG_ERR, "Unable to close file %s", filesdir);
        return 1;
    }
    return 0;
}
