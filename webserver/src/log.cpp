#include "../include/log.h"
#include <bits/types/struct_timeval.h>
#include <bits/types/time_t.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/select.h>

Log::Log() {
    m_close_log = 0;
    m_is_async = false;
}

Log::~Log() {
    if (m_fp != NULL) {
        fclose(m_fp);
    }
}

bool Log::init(const char *file_name, int close_log, int log_buf_size,
                int split_lines, int max_queue_size) {
    if (max_queue_size >= 1) {
        // async write
        m_is_async = true;
        m_log_queue = new block_queue<std::string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[log_buf_size];
    memset(m_buf, '\0', log_buf_size);
    m_max_lines = split_lines;
    
    // log time
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    
    // directory name and file name
    char log_full_name[256] = {0};
    const char *p = strrchr(file_name, '/');
    if (p == NULL) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s",
                    sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday, file_name);
    } else {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s",
                    dir_name, sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday, log_name);
    }
    
    m_today = sys_tm->tm_mday;
    m_fp = fopen(log_full_name, "a");
    
    if (m_fp == NULL) {
        return false;
    }
    return true;
}

void Log::write_log(int level, const char *format, ...) {
    // level
    char s[16] = {0};
    switch (level) {
        case 0: {
            strcpy(s, "[debug]");
            break;
        }
        case 1: {
            strcpy(s, "[info]");
            break;
        }
        case 2: {
            strcpy(s, "[warn]");
            break;
        }
        case 3: {
            strcpy(s, "[error]");
            break;
        }
        default: {
            strcpy(s, "[info]");
            break;
        }
    }
    
    // write to file
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);
    
    m_mutex.lock();
    ++m_count_line;
    
    // new file to write
    if (m_today != sys_tm->tm_mday || m_count_line % m_max_lines == 0) {
        fflush(m_fp);
        fclose(m_fp);
        
        char new_log[256] = {0};
        char tail[16] = {0};
       
        snprintf(tail, 16, "%d_%02d_%02d_", sys_tm->tm_year + 1900,
                sys_tm->tm_mon + 1, sys_tm->tm_mday);
        if (m_today != sys_tm->tm_mday) {
            snprintf(new_log, 256, "%s%s%s", dir_name, tail, log_name);
            m_today = sys_tm->tm_mday;
            m_count_line = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name,
                        tail, log_name, m_count_line / m_max_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();
    
    // write to file
    va_list valst;
    va_start(valst, format);
    std::string log_str;
    m_mutex.lock();
    
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday,
                     sys_tm->tm_hour, sys_tm->tm_min, sys_tm->tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[m + n] = '\n';
    m_buf[m + n + 1] = '\0';
    log_str = m_buf;
    m_mutex.unlock();
    
    if (m_is_async && !m_log_queue->full()) {
        m_log_queue->push(log_str);
    } else {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}

void Log::flush(void) {
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}