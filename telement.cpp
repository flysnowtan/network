#include "telement.h"

TElement::TElement()
:m_ListenFd(-1), m_Flag(0), m_pNext(NULL), m_IOThreadIdx(-1)
{
    m_InQueueTime.tv_sec = m_InQueueTime.tv_usec = 0;
    m_Time.tv_sec = m_Time.tv_usec = 0;
}

TElement::TElement(TBufferPool * pPool)
:m_ListenFd(-1), m_Flag(0), m_ReqPkg(pPool), m_RspPkg(pPool), m_pNext(NULL), m_IOThreadIdx(-1)
{
    m_InQueueTime.tv_sec = m_InQueueTime.tv_usec = 0;
    m_Time.tv_sec = m_Time.tv_usec = 0;
}

TElement::~TElement()
{
}

int *TElement::GetListenFd()
{
    return &m_ListenFd;
}

TBuffer* TElement::GetReqPkg()
{
    return &m_ReqPkg;
}

TBuffer* TElement::GetRspPkg()
{
    return &m_RspPkg;
}

void TElement::SetProcessing()
{
    m_Flag |= IS_PROCESSING;
}

bool TElement::IsProcessing()
{
    return m_Flag & IS_PROCESSING;
}

void TElement::UnSetProcessing()
{
    m_Flag &= (~IS_PROCESSING);
}

bool TElement::IsUsed()
{
    return m_Flag & IS_USED;
}

void TElement::SetUsed()
{
    m_Flag |= IS_USED;
}

struct timeval * TElement::GetInQueueTime()
{
    return &m_InQueueTime;
}

struct timeval * TElement::GetTime()
{
    return &m_Time;
}

bool TElement::IsInQueueTimeOut()
{
    if(INQUEUE_TIME_OUT != 0)
    {
        return IsTimeOut(INQUEUE_TIME_OUT * 1000);
    }
    return false;
}

bool TElement::IsReadTimeOut()
{
    if(READ_TIME_OUT != 0)
    {
        return IsTimeOut(READ_TIME_OUT * 1000);
    }
    return false;
}

bool TElement::IsWriteTimeOut()
{
    if(WRITE_TIME_OUT != 0 )
    {
        return IsTimeOut(WRITE_TIME_OUT * 1000);
    }
    return false;
}

bool TElement::IsTimeOut(uint32_t msec)
{
    if(m_Time.tv_sec != 0)
    {
        struct timeval end;
        gettimeofday(&end, NULL);
        uint32_t iDurTime = (end.tv_sec - m_Time.tv_sec) * 1000 + (end.tv_usec - m_Time.tv_usec) / 1000;
        //超时时间不应该达到这么大，如果达到这么大，说明出现m_Time为0了，
        //这是由于线程不安全引起的，
        if(iDurTime > msec  && iDurTime < 1455033600) //2016.2.10:00:00
        {
            printf("sec %d time %d\n", end.tv_sec - m_Time.tv_sec, m_Time.tv_sec);
            return true;
        }
    }

    return false;
}

void TElement::SetInQueueTime()
{
    gettimeofday(&m_InQueueTime, NULL);
}

void TElement::SetNowTime()
{
    gettimeofday(&m_Time, NULL);
}

void TElement::Clear()
{
    m_ReqPkg.Reset();
    m_RspPkg.Reset();
}

void TElement::ClearFlag()
{
    m_Flag = 0;
}

void TElement::ClearReqPkg()
{
    if(m_ReqPkg.GetCapacity() > MAX_PKG_BUFFER_LEN)
    {
        m_ReqPkg.Reset();
    }
    else
    {
        m_ReqPkg.SetReadPos(0);
        m_ReqPkg.SetWritePos(0);
    }
}

void TElement::ClearRspPkg()
{
    if(m_RspPkg.GetCapacity() > MAX_PKG_BUFFER_LEN)
    {
        m_RspPkg.Reset();
    }
    else
    {
        m_RspPkg.SetReadPos(0);
        m_RspPkg.SetWritePos(0);
    }
}

void TElement::Reset()
{
    m_InQueueTime.tv_sec = m_InQueueTime.tv_usec = 0;
    m_Time.tv_sec = m_Time.tv_usec = 0;
    m_Flag = 0;
    m_ListenFd = -1;
    m_IOThreadIdx = -1;
    ClearReqPkg();
    ClearRspPkg();
}

void TElement::ClearTime()
{
    m_Time.tv_sec = m_Time.tv_usec = 0;
}


void TElement::SetIOThreadIdx(int idx)
{
    m_IOThreadIdx = idx;
}

int TElement::GetIOThreadIdx()
{
    return m_IOThreadIdx;
}
