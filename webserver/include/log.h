#ifndef LOG_H
#define LOG_H
#include "block_queue.h"
#include "locker.h"
#include <cstdio>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include <string>

class Log {
    public:
        static Log* get_instance() {
            static Log instance;
            return &instance;
        }
        bool init(const char *file_name, int close_log,
                    int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
        static void* flush_log_thread(void* args) {
            return Log::get_instance()->async_write_log();
        }
        void write_log(int level, const char *format, ...);
        void flush(void);
        int get_close_log() { return m_close_log; }
        void set_close_log(int close_log) { m_close_log = close_log; }
        
    private:
        Log();
        virtual ~Log();
        void* async_write_log() {
            std::string single_log;
            while (m_log_queue->pop(single_log)) {
                m_mutex.lock();
                fputs(single_log.c_str(), m_fp);
                m_mutex.unlock();
            }
            return nullptr;
        }
    
    private:
        char dir_name[128];
        char log_name[128];
        FILE* m_fp;
        
        locker m_mutex;
        block_queue<std::string>* m_log_queue;
        int m_max_lines;
        long long m_count_line;
        int m_today;
        char *m_buf;
        int m_log_buf_size;
        bool m_is_async;
        int m_close_log;
        
};

#define LOG_DEBUG(format, ...) if(Log::get_instance()->get_close_log() == 0) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(Log::get_instance()->get_close_log() == 0) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(Log::get_instance()->get_close_log() == 0) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(Log::get_instance()->get_close_log() == 0) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif