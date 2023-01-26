#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include "locker.h"
#include <cstdlib>
#include <queue>

template<typename T>
class block_queue {
    public:
        block_queue<T>(int max_size) : m_max_size(max_size), m_size(0),
                                        m_queue(std::queue<T>()) {}
                                    
                                    
        ~block_queue<T>() {}
        
        void clear() {
            m_mutex.lock();
            m_size = 0;
            m_mutex.unlock();
        }
        
        bool full() {
            m_mutex.lock();
            if (m_size >= m_max_size) {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        
        bool empty() {
            m_mutex.lock();
            if (m_size == 0) {
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        
        bool front(T &value) {
            m_mutex.lock();
            if (m_size == 0) {
                m_mutex.unlock();
                return false;
            }
            value = m_queue.front();
            m_mutex.unlock();
            return true;
        }
        
        bool back(T &value) {
            m_mutex.lock();
            if (m_size == 0) {
                m_mutex.unlock();
                return false;
            }
            value = m_queue.back();
            m_mutex.unlock();
            return true;
        }
        
        int size() {
            int tmp = 0;
    
            m_mutex.lock();
            tmp = m_size;
    
            m_mutex.unlock();
            return tmp;
        }
        
        int max_size() {
            int tmp = 0;
    
            m_mutex.lock();
            tmp = m_max_size;
    
            m_mutex.unlock();
            return tmp;
        }
        
        bool push(const T& item) {
            m_mutex.lock();
            if (m_size >= m_max_size) {
                m_cond.broadcast();
                m_mutex.unlock();
                return false;
            }
            m_queue.emplace(item);
            ++m_size;
            m_cond.broadcast();
            m_mutex.unlock();
            return true;
        }
        
        bool pop(T& item) {
            m_mutex.lock();
            while (m_size <= 0) {
                if (!m_cond.wait(m_mutex.get())) {
                    m_mutex.unlock();
                    return false;
                }
            }
            item = m_queue.front();
            m_queue.pop();
            --m_size;
            m_mutex.unlock();
            return true;
        }
        
    private:
        std::queue<T> m_queue;
        int m_size;
        int m_max_size;
        
        locker m_mutex;
        cond m_cond;
};

#endif