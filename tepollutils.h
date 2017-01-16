#ifndef __T__EPOLLUTILS__H__
#define __T__EPOLLUTILS__H__

#include <stdint.h>

class TEpollUtils
{
public:
    static int Listen(const char *ip, const uint16_t port, const int backlog);
    static void InitSignalHandler();
    static void SigHandler(int sig);
    static int ForkAsDaemon();
    static int SetNonBlock(int iFd);
};

#endif
      
