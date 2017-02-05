#include "tepollctx.h"

TEpollCtx::TEpollCtx() 
    :m_IOThreadIdx(-1), m_pTimeOutElm(NULL)
{
    m_pEpoller = new TEpoller;
    m_pHandler = new TEpollHandler;
    if(m_pEpoller->Create() != 0)
    {   
        printf("create error!\n");
    }   
}

TEpollCtx::~TEpollCtx() 
{
    if(m_pEpoller)  delete m_pEpoller, m_pEpoller = NULL;
    if(m_pHandler)  delete m_pHandler, m_pHandler = NULL;
}


void TEpollCtx::SetIOThreadIdx(int idx) 
{
    m_IOThreadIdx = idx;
}
int TEpollCtx::GetIOThreadIdx() 
{
    return m_IOThreadIdx;
}

TEpoller * TEpollCtx::GetEpoller() 
{
    return m_pEpoller;
}

TEpollHandler * TEpollCtx::GetHandler() 
{
    return m_pHandler;
}
TElement * TEpollCtx::GetTimeOutElm() 
{
    return m_pTimeOutElm;
}

bool TEpollCtx::HasTimeOutElm() 
{
    return m_pTimeOutElm != NULL;
}

void TEpollCtx::SetTimeOutElm(TElement * pElm) 
{
    m_pTimeOutElm = pElm;
}

void TEpollCtx::DelTimeOutElm() 
{
    m_pTimeOutElm = NULL;
}
