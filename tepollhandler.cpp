#include "tepollhandler.h"
#include "tepollsvrenum.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>


static int outputnum = 0;
int TEpollHandler::HandleRecvData(TElement *pElm, bool bDoWork)
{
    setbuf(stdout, NULL);
    int iLen = 0;
    TBuffer *pReqPkg = pElm->GetReqPkg();

    int iRet = 0;
    int iFd = *(pElm->GetListenFd());
    int iLoop = 1;
    while(true)
    {   
        pReqPkg->ExtendSize((iLoop++) * RECV_BUFFER_LEN);
        iLen = read(iFd, pReqPkg->GetWritePos(), RECV_BUFFER_LEN);
        if(iLen < 0)
        {   
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {   
                break;
            }   
            else if(errno == EINTR)
            {   
                printf("intr occur! continue\n");
                continue;
            }   

            printf("read err, errno %d, %s\n", errno, strerror(errno));
            return -1; 
        }   
        else if(iLen > 0)
        {   
            pReqPkg->AddWritePos(iLen);
        }   
        else
        {   
            //closed
//          printf("client closed\n");
            return -1002;
        }   
    }   

    if(pElm->IsReadTimeOut())
    {   
        printf("read timeout! %s->%d", __func__, __LINE__);
        return -1; 
    }   

    iRet = IsCompletedPkg(pElm);
    if(iRet == 0)
    {   
        pElm->SetNowTime();
        if(bDoWork)
        {   
            DoWork(pElm);
        }   
    }   

    return iRet;
}

int TEpollHandler::HandleSendData(TElement *pElm)
{
    setbuf(stdout, NULL);
    int iWrite = 0;
    int iFd = *(pElm->GetListenFd());
    TBuffer * pRspPkg = pElm->GetRspPkg();

    if(pRspPkg->GetUnReadSize() == 0)
    {
        printf("no data to send\n");
        return 0;
    }

    while(true)
    {
        iWrite = write(iFd, pRspPkg->GetReadPos(), pRspPkg->GetUnReadSize());
        if(iWrite < 0)
        {
            //need deal with EPIPE
            if(errno == EINTR)
            {
                continue;
            }
            else if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("would block\n");
                return 1;
            }
            else
            {
                printf("send err, errno %d", errno);
                return -1;
            }
        }
        else
        {
            pRspPkg->AddReadPos(iWrite);
        }

        if(pRspPkg->GetUnReadSize() == 0)   break;
    }

    if(pElm->IsWriteTimeOut())
    {
        printf("write timeout! %s->%d", __func__, __LINE__);
        return -1;
    }

    //debug info
    {
//      printf("len %d\n", pRspPkg->GetLen());
    }

    return 0;
}

int TEpollHandler::DoWork(TElement * pElm)
{
    TBuffer *pRspPkg = pElm->GetRspPkg();
    TBuffer *pReqPkg = pElm->GetReqPkg();
    pRspPkg->Write(pReqPkg->GetBuffer(), pReqPkg->GetLen());
    return 0;
}

int TEpollHandler::IsCompletedPkg(TElement * pElm)
{
    return 0;
}

