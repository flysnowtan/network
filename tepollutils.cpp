#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>
#include <string>

#include "tepollutils.h"

const char * GetSelfName( char * buf, int count)
{
    int rslt = readlink("/proc/self/exe", buf, count - 1);
    if (rslt < 0 || (rslt >= count - 1))
    {
        return NULL;
    }
    buf[rslt] = '\0';
    return buf;
}

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

    sigaction(SIGPIPE, &sa, 0);
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
    printf("sig num %d\n", sig);
    if(sig != SIGPIPE)
    {
        std::string info;
        DumpStack(info);

        printf("core.log", "%s", info.c_str());

        signal(sig, SIG_DFL);
        exit(-1);
    }
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

void TEpollUtils::DumpStack(std::string &info)
{
    info.clear();

    void *bufs[100];
    int n = backtrace(bufs, 100);
    char **infos = backtrace_symbols(bufs, n);

    if (!infos) exit(1);

    fprintf(stderr, "==================\n");
    fprintf(stderr, "Frame info:\n");

    char name[1024];
    char cmd[1024];
    int len = snprintf(cmd, sizeof(cmd),
            "addr2line -ifC -e %s", GetSelfName(name, sizeof(name)));
    char *p = cmd + len;
    size_t s = sizeof(cmd) - len;
    for(int i = 0; i < n; ++i) {
        fprintf(stderr, "%s\n", infos[i]);
        if(s > 0) {
            len = snprintf(p, s, " %p", bufs[i]);
            p += len;
            s -= len;
        }
    }
    fprintf(stderr, "src info:\n");

    FILE *fp;
    char buf[128];
    if((fp = popen(cmd, "r"))) {
        while(fgets(buf, sizeof(buf), fp))
        {
            fprintf(stderr, "%s", buf);
            info += std::string(buf);
        }
        pclose(fp);
    }
    fprintf(stderr, "==================\n");
    free(infos);
}


