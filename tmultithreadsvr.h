#ifndef __T__MULTITHREADSVR__H__
#define __T__MULTITHREADSVR__H__

#include "telement.h"

class TEpollSvr
{
public:
    TEpollSvr();
    ~TEpollSvr();

    int Init();
    int Run();
    int Stop();

private:
};

#endif
