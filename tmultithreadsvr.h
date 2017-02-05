#pragma once

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
