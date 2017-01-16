#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "tepollutils.h"

int TEpollUtils::Listen(const char *ip, const uint16_t port, const int backlog)
{
    int iListenFd = socket(AF_INET, SOCK_STREAM, 0); 
    if(iListenFd < 0)
    {   
        return -1; 
    }   

    struct sockaddr_in sockin;
    bzero(&sockin, sizeof(struct sockaddr_in));
    sockin.sin_family = AF_INET;
    if(ip == NULL || *ip == '\0')
    {   
        sockin.sin_addr.s_addr = htonl(INADDR_ANY);
    }   
    else
    {   
        sockin.sin_addr.s_addr = inet_addr(ip);
    }   
    sockin.sin_port = htons(port);
    
    int ret = 0;
    int optval = 1;
    ret = setsockopt(iListenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(int));
    if(ret == -1) 
    {   
        return -1; 
    }   

    ret = bind(iListenFd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in));
    if(ret == -1) 
    {   
        return -1; 
    }   

    ret = listen(iListenFd, backlog);
    if(ret == -1) 
    {   
        return -1; 
    }   

    return iListenFd;
}


void TEpollUtils::InitSignalHandler()
{
    struct sigaction sa; 
    bzero(&sa, sizeof(struct sigaction));
    sa.sa_handler = SigHandler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, 0); 
    sigaction(SIGABRT, &sa, 0); 
    sigaction(SIGILL, &sa, 0); 
    sigaction(SIGFPE, &sa, 0); 
    sigaction(SIGSYS, &sa, 0); 
    sigaction(SIGBUS, &sa, 0); 
}
void TEpollUtils::SigHandler(int sig)
{
    //print core log
}

int TEpollUtils::ForkAsDaemon()
{
    int ret = fork();
    if(ret == 0)
    {
        setsid();
    }
    else if(ret > 0)
    {
        exit(0);
    }
    else
    {
        ret = -1;
    }
    return ret;
}
int TEpollUtils::SetNonBlock(int iFd)
{
    int flag = fcntl(iFd, F_GETFL);
    if(flag < 0)    return -1;

    flag |= O_NONBLOCK;

    if(fcntl(iFd, F_SETFL, flag) < 0)   return -1;

    return 0;
}
