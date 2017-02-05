#include "tbuffer.h"
#include <assert.h>

char * TBufferPool::Alloc(const uint32_t iSize, uint32_t * pRealSize)
{
    uint32_t iRealSize = iSize ? iSize : 1;
    //不是2的幂次
    if((iSize-1) & iSize)
    {
        int iBits = 1;
        while(iRealSize >> 1)
        {
            iBits++;
            iRealSize = iRealSize >> 1;
        }
        iRealSize = 1 << iBits;
        if(iRealSize < iSize)
        {
            printf("alloc err, realsize %u needsize %u\n", iRealSize, iSize);
            return NULL;
        }
    }

    char * pMem = new char[iRealSize];
    if(pRealSize) *pRealSize = iRealSize;
    return pMem;
}

void TBufferPool::Dealloc(char * pMem)
{
    if(pMem)
    {
        delete[] pMem;
    }
}

TBuffer::TBuffer()
    : m_pMem(NULL), m_iReadPos(0), m_iWritePos(0), m_iCapacity(0)
{
    m_pPool = new TBufferPool;
    m_bDefaultPool = true;
}


TBuffer::TBuffer(TBufferPool * pPool)
    :m_pPool(pPool), m_pMem(NULL), m_iReadPos(0), m_iWritePos(0), m_iCapacity(0)
{
    m_bDefaultPool = false;
}

TBuffer::~TBuffer()
{
    if(m_pPool)
    {
        m_pPool->Dealloc(m_pMem);
        m_pMem = NULL;
        if(m_bDefaultPool)
        {
            delete m_pPool;
            m_pPool = NULL;
        }
    }
}

void TBuffer::ExtendSize(uint32_t iSize)
{
    if(m_iCapacity < iSize)
    {
        assert(m_pPool != NULL);
        char *pTmp = m_pPool->Alloc(iSize, &m_iCapacity);
        assert(pTmp != NULL);

        if(m_pMem != NULL)
        {
            memcpy(pTmp, m_pMem, m_iWritePos);
            m_pPool->Dealloc(m_pMem);
        }
        m_pMem = pTmp;
    }
}

uint32_t TBuffer::Write(const char * pBuffer, uint32_t iSize)
{
    ExtendSize(iSize + m_iWritePos);
    memcpy(m_pMem + m_iWritePos, pBuffer, iSize);
    m_iWritePos += iSize;
    return iSize;
}

uint32_t TBuffer::Write(const std::string & str)
{
    ExtendSize(str.size() + m_iWritePos);
    memcpy(m_pMem + m_iWritePos, str.c_str(), str.size());
    m_iWritePos += str.size();
    return str.size();
}


char * TBuffer::GetWritePos(uint32_t iSize)
{
    ExtendSize(iSize);
    return m_pMem + m_iWritePos;
}

char * TBuffer::GetWritePos()
{
    return m_pMem + m_iWritePos;
}

void TBuffer::AddWritePos(uint32_t iSize)
{
    m_iWritePos += iSize;
}

void TBuffer::AddReadPos(uint32_t iSize)
{
    m_iReadPos += iSize;
}

void TBuffer::SetReadPos(uint32_t iSize)
{
    m_iReadPos = iSize;
}

void TBuffer::SetWritePos(uint32_t iSize)
{
    m_iWritePos = iSize;
}

uint32_t TBuffer::GetUnReadSize()
{
    return m_iWritePos - m_iReadPos;
}

char * TBuffer::GetReadPos()
{
    return m_pMem + m_iReadPos;
}

char * TBuffer::GetBuffer()
{
    return m_pMem;
}

uint32_t TBuffer::GetCapacity()
{
    return m_iCapacity;
}

uint32_t TBuffer::GetLen()
{
    return m_iWritePos;
}

void TBuffer::Reset()
{
    if(m_pPool)
    {
        m_pPool->Dealloc(m_pMem);
        m_pMem = NULL;
    }

    m_iReadPos = 0;
    m_iWritePos = 0;
    m_iCapacity = 0;
}
                
                
