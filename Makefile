objects = tqueue.o tmultithreadsvr.o telement.o tmutexlock.o tepollutils.o tepoller.o 

CC = g++ 

all: epollsvr

epollsvr: tepollsvr.cpp tepollutils.cpp tepoller.cpp  tmultithreadsvr.cpp tepollhandler.cpp
    $(CC) -o epollsvr tepollsvr.cpp  tepollutils.cpp tepoller.cpp tmultithreadsvr.cpp tepollhandler.cpp -pthread -g -O0

queue: queue.cpp tqueue.h tmutexlock.h
    $(CC)  queue.cpp -lpthread -o queue

tmultithreadsvr.o : tepoller.h telement.h tepollutils.h

tepoller.o : tepoller.h
    $(CC) -c tepoller.h -o tepoller.o

tqueue.o : tqueue.h tmutexlock.h
    $(CC) -c tqueue.h -o tqueue.o

telement.o : telement.h
    $(CC) -c telement.h -o telement.o 

tmutexlock.o : tmutexlock.h
    $(CC) -c tmutexlock.h -o tmutexlock.o

tepollutils.o : tepollutils.h


clean:  
    rm -f $(objects)
