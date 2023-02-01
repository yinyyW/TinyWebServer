#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <cstddef>
#include <exception>
#include <pthread.h>
#include "./locker.h"
#include <list>

template<typename T>
class threadpool {
    public:
        threadpool(int thread_number = 8, int max_request = 10000);
        ~threadpool();
        // add request to queue
        bool append(T* request);
    
    // HELPER FUNCTIONS
    private:
        static void* worker(void* arg);
        void run();
    
    // class members
    private:
        int m_thread_number;
        int m_max_request;
        pthread_t* m_threads;
        std::list<T*> m_workqueue;
        locker m_queuelocker;
        sem m_queuestat;
        bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_request) :
                m_thread_number(thread_number), m_max_request(max_request),
                m_stop(false), m_threads(NULL){
    printf("thread_num: %d, max_request: %d.\n", thread_number, max_request);
    if (thread_number <= 0 || max_request <= 0) {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) {
        throw std::exception();
    }
    
    for (int i = 0; i < m_thread_number; ++i) {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T *request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}

#endif