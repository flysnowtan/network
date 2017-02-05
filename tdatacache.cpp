#include "tdatacache.h"

#define T_DATACACHE_BLOCKS_NUM  8
#define T_DATACACHE_BLOCKS_SIZE 65536

//T_DATACACHE_BLOCKS_SIZE * T_DATACACHE_BLOCKS_NUM = 2^19 
TDataCache::TDataCache()
    :m_AllocPos(0), m_CheckPos(0), m_pFreeList(NULL), m_FreeQueue(19)
{
    m_ppBlocks = new TElement*[T_DATACACHE_BLOCKS_NUM];
    for(int i = 0; i < T_DATACACHE_BLOCKS_NUM; i++)
    {   
        m_ppBlocks[i] = NULL;
    }   

    m_ppBlocks[0] = new TElement[T_DATACACHE_BLOCKS_SIZE];
}

TDataCache::~TDataCache()
{
    for(int i = 0; i < T_DATACACHE_BLOCKS_NUM; i++)
    {   
        if(m_ppBlocks[i])
        {   
            delete[] m_ppBlocks[i];
        }   
    }   
    delete[] m_ppBlocks;
}

TElement * TDataCache::Alloc()
{
    TElement * pElm = NULL;
    int iCirTimes = 0;
    while(iCirTimes++ < 10000 && m_FreeQueue.Pop(&pElm, false))
    {   
        pElm->m_pNext = m_pFreeList;
        m_pFreeList = pElm;
    }   

    pElm = NULL;

    
    if(m_pFreeList)
    {   
        pElm = m_pFreeList;
        m_pFreeList = m_pFreeList->m_pNext;
    }   
    else if(m_FreeQueue.Pop(&pElm, false))
    {   
    }   
    else if(m_AllocPos < T_DATACACHE_BLOCKS_NUM * T_DATACACHE_BLOCKS_SIZE)
    {   
        uint32_t iBlock = m_AllocPos / T_DATACACHE_BLOCKS_SIZE;
        uint32_t iPos = m_AllocPos % T_DATACACHE_BLOCKS_SIZE;
        if(m_ppBlocks[iBlock] == NULL)
        {   
            m_ppBlocks[iBlock] = new TElement[T_DATACACHE_BLOCKS_SIZE];
            if(m_ppBlocks[iBlock] == NULL)  return NULL;
        }   

        pElm = &m_ppBlocks[iBlock][iPos];
        m_AllocPos++;
    }   
    else
    {   
        return NULL;
    }   

    pElm->SetNowTime();
    pElm->SetUsed();

    return pElm;
}

bool TDataCache::Free(TElement * pElm, bool bWait)
{
    pElm->Reset();
    while(!m_FreeQueue.Push(pElm))
    {
        if(!bWait)  return false;
        usleep(1000);
    }
    return true;
}

TElement * TDataCache::GetTimeOutElm(uint32_t msec)
{
    uint32_t i = m_CheckPos / T_DATACACHE_BLOCKS_SIZE;
    uint32_t j = m_CheckPos % T_DATACACHE_BLOCKS_SIZE;
    uint32_t iGuard = i;
    uint32_t jGuard = j;

    TElement * pElm = NULL;
    do
    {
        if(++j >= T_DATACACHE_BLOCKS_SIZE)
        {
            j = 0;

            if(++i >= T_DATACACHE_BLOCKS_NUM)
            {
                i = 0;
            }

            if(m_ppBlocks[i] == NULL)
            {
                i = 0;
                j = 0;
            }
        }

        if(m_ppBlocks[i])
        {
            pElm = &m_ppBlocks[i][j];
            if(pElm->IsUsed() && *(pElm->GetListenFd()) > 0)
            {
                struct timeval * pTime = pElm->GetTime();
                if(pTime->tv_sec != 0 || pTime->tv_usec != 0)
                {
                    if(pElm->IsTimeOut(msec))
                    {
                        m_CheckPos = i * T_DATACACHE_BLOCKS_SIZE + j;
                        return pElm;
                    }
                }
            }
        }

        if(i == iGuard && j == jGuard)
        {
            break;
        }

    } while(true);

    return NULL;
}

