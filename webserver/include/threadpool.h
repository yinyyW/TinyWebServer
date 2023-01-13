#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

template<typename T>
class threadpool {
    public:
        threadpool(int thread_number = 8, int max_request = 10000);
        ~threadpool();
        // add request to queue
        bool append(T* request);
    private:
        static void* worker(void* arg);
        void run();
    private:
        int m_thread_number;
        int m_max_request;
        pthread_t* m_threads;
};

#endif