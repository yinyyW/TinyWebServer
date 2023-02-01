#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <errno.h>
#include "sql_connection_pool.h"
#include "log.h"

class http_conn {
    // ############# CONSTANT VARIABLES #############
    public:
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = 2048;
        static const int WRITE_BUFFER_SIZE = 1024;
        enum METHOD { GET = 0, POST, HEAD, PUT, DELETE };
        enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, 
                            CHECK_STATE_CONTENT };
        enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE,
                        FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION,
                        CONFLICT, UNAUTHORISED, CREATED, OK };
        enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

    // ############# FUNCTIONS #############
    public:
        // constructor
        http_conn() {}
        // destructor
        ~http_conn() {}

        // utility functions
        void init(int sockfd, const sockaddr_in& addr, char* doc_root, connection_pool* pool);
        void close_conn(bool real_close = true);
        void process();
        bool read();
        bool write();
        sockaddr_in *get_address() { return &m_address; }

    private:
        // initialize connection
        void init();
        // read request/send response
        HTTP_CODE process_read();
        bool process_write(HTTP_CODE ret);

        // helper functions for reading request
        HTTP_CODE parse_request_line(char* text);
        HTTP_CODE parse_headers(char* text);
        HTTP_CODE parse_content(char* text);
        HTTP_CODE do_request();
        HTTP_CODE api_request();
        HTTP_CODE user_register();
        HTTP_CODE user_login();
        char* get_line() { return m_read_buf + m_start_line; };
        LINE_STATUS parse_line();

        // helper functions for sending response
        void unmap();
        bool add_response(const char* format, ...);
        bool add_content(const char* content);
        bool add_status_line(int status, const char* title);
        bool add_headers(int content_length, bool api_response = false);
        bool add_content_type(const char* type);
        bool add_content_length(int content_length);
        bool add_linger();
        bool add_blank_line();
    
    // ############# VARIABLES DECLARATION #############
    public:
        static int m_epollfd;
        static int m_user_count;
        connection_pool* m_pool;
    
    private:
        // client's socket and address
        int m_sockfd;
        sockaddr_in m_address;
        char* doc_root;

        // member variables for reading request/sending response
        char m_read_buf[READ_BUFFER_SIZE];
        int m_read_idx;
        int m_checked_idx;
        int m_start_line;
        char m_write_buf[WRITE_BUFFER_SIZE];
        int m_write_idx;

        // state machine
        CHECK_STATE m_check_state;
        // request's method
        METHOD m_method;

        // request info
        char m_real_file[FILENAME_LEN];
        char* m_url;
        char* m_version;
        char* m_host;
        int m_content_length;
        char* m_content;
        bool m_linger;

        // response info
        char* m_file_address;
        struct stat m_file_stat;
        struct iovec m_iv[2];
        int m_iv_count;
};

#endif