#include "tepollutils.h"
#include "tmultithreadsvr.h"
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <memory>
#include "tmultithreadsvr.h"
#include "tepollutils.h"
#include "tepoller.h"
#include "tqueue.h"
#include "tepollhandler.h"
#include "tepollsvrenum.h"

#include <stdio.h>
#include <vector>
#include <errno.h>
#include <syslog.h>



    
int main(void)
{
    int iListenFd = TEpollUtils::Listen(NULL, 9900, 1024);
    if(iListenFd > 0)
    {   
        struct sockaddr_in sockin;
        socklen_t socklen = sizeof(struct sockaddr_in);
        while(true)
        {   
            int iFd = accept(iListenFd, (struct sockaddr*)&sockin, &socklen);
            if(iFd == -1) 
            {   
                //进程创建文件数超过限制后，监听队列里面的新连接不会被取出，所以会一直accept err,不用打log
                if(errno != EMFILE)
                {   
                    perror("accept error!");
                }   
                continue;
            }   

            char buffer[1024] = {0};
            read(iFd, buffer, sizeof(buffer));
            sleep(4);
            printf("write size %d\n", write(iFd, buffer, sizeof(buffer)));
            close(iFd);
            printf("write size %d\n", write(iFd, buffer, sizeof(buffer)));

        }   

    }   
    return 0;
}
