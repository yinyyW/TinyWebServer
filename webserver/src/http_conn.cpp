#include "../include/http_conn.h"

// HTTP response status
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this service.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this service.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFD, new_option);
    return old_option;
}

void add_fd(int epollfd, int fd, bool one_shot) {
    
}

// http_conn class static member initialization
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;