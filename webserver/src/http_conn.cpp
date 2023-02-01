#include "../include/http_conn.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mysql/mysql.h>
#include <string>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

// HTTP response status
const char* ok_200_title = "OK";
const char* ok_200_form = "Success";
const char* ok_201_title = "Created";
const char* ok_201_form = "User created successfully";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this service.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this service.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
// resource folder
// const char* doc_root = "../resource";

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// ############# EPOLL OPERATION #############
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events |= ev | EPOLLIN | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// ############# HTTP_CONN CLASS METHODS IMPLEMENTATION #############

// http_conn class static member initialization
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

void http_conn::close_conn(bool real_close) {
    if (real_close && m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        --m_user_count;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char* root, connection_pool* pool) {
    m_sockfd = sockfd;
    m_address = addr;
    m_pool = pool;
    doc_root = root;
    // debug
    // int reuse = 1;
    // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    ++m_user_count;
    init();
}

void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    // std::cout << "checked idx: " << m_checked_idx << std::endl;
    // std::cout << "read idx: " << m_read_idx << std::endl;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) {
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    // ET: read once
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                        READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        } else if (bytes_read == 0) {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    m_url = strpbrk(text, " \t");
    if (!m_url) {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    
    // request's method
    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
        std::cout << "request method is GET\n";
    } else if (strcasecmp(method, "POST") == 0) {
        m_method = POST;
        std::cout << "request method is POST\n";
    } else {
        return BAD_REQUEST;
    }
    
    // request's version, only support HTTP1.1
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    
    // request's url
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    
    std::cout << "The request url is: " << m_url << std::endl;
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char* text ) {
    // std::cout << "parse header: " << text << std::endl;
    if (text[0] == '\0') {
        // end of headers
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        // std::cout << "Oop! Unknown Header: " << text << std::endl;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if (m_read_idx >= m_checked_idx + m_content_length) {
        text[m_content_length] = '\0';
        std::cout << "request body: " << text << std::endl;
        m_content = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || 
            (line_status = parse_line()) == LINE_OK) {
        text = get_line();
        m_start_line = m_checked_idx;
        std::cout << "got 1 http line: " << text << std::endl;
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::user_register() {
    // extract user's name and password
    char* user = m_content;
    char* password = strpbrk(m_content, "&");
    if (password == NULL) {
        return BAD_REQUEST;
    }
    *password++ = '\0';
    
    user = strpbrk(user, "="); 
    password = strpbrk(password, "=");
    if (user == NULL || password == NULL) {
        return BAD_REQUEST;
    }
    ++user;
    ++password;
    
    // find user in database
    MYSQL* sql = NULL;
    connectionRAII sql_conn = connectionRAII(&sql, m_pool);
    const std::string query = "SELECT * FROM user where username='" 
                                + std::string(user) + "'";
    std::cout << "query: " << query << std::endl;
    if (mysql_query(sql, query.c_str())) {
        std::cout << "error: " << mysql_error(sql) << std::endl;
        return BAD_REQUEST;
    }
    MYSQL_RES *result = mysql_store_result(sql);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row != NULL) {
        return CONFLICT;
    }
    // insert user;
    std::string insert_query = "INSERT INTO user(username, password) VALUES('" 
                                + std::string(user) + "','"
                                + std::string(password) + "')";
    std::cout << "insert query: " << insert_query << std::endl;
    if (mysql_query(sql, insert_query.c_str())) {
        std::cout << "error: " << mysql_error(sql) << std::endl;
        return BAD_REQUEST;
    }
    std::cout << "register ok.\n";
    return CREATED;
}

http_conn::HTTP_CODE http_conn::user_login() {
    // extract user's name and password
    char* user = m_content;
    char* password = strpbrk(m_content, "&");
    if (password == NULL) {
        return BAD_REQUEST;
    }
    *password++ = '\0';
    
    user = strpbrk(user, "="); 
    password = strpbrk(password, "=");
    if (user == NULL || password == NULL) {
        return BAD_REQUEST;
    }
    ++user;
    ++password;
    
    // find user in database
    MYSQL* sql = NULL;
    connectionRAII sql_conn = connectionRAII(&sql, m_pool);
    const std::string query = "SELECT * FROM user where username='" 
                                + std::string(user) + "'";
    std::cout << "query: " << query << std::endl;
    if (mysql_query(sql, query.c_str())) {
        std::cout << "error: " << mysql_error(sql) << std::endl;
        return BAD_REQUEST;
    }
    MYSQL_RES *result = mysql_store_result(sql);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == NULL || strcmp(std::string(row[1]).c_str(), password) != 0) {
        return UNAUTHORISED;
    }
    return OK;
}

http_conn::HTTP_CODE http_conn::api_request() {
    std::cout << "api request.\n";
    if (m_method == POST) {
        if (strcasecmp(m_url, "/register") == 0) {
            return user_register();
        } else if (strcasecmp(m_url, "/login") == 0) {
            return user_login();
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request() {
    if (strchr(m_url, '.') == NULL) {
        return api_request();
    }
    // path of the file
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    std::cout << "file path: " << m_real_file << std::endl;
    
    // validate file path
    if (stat(m_real_file, &m_file_stat) < 0) {
        return NO_REQUEST;
    }
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }
    
    // open file and map to memory
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, 
                        MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, 
                        format, arg_list);
    if (len >= WRITE_BUFFER_SIZE - m_write_idx - 1) {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_content(const char* content) {
    return add_response("%s",content);
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_length, bool api_response) {
    if (api_response) {
        return add_content_type("text/plain")
                && add_content_length(content_length)
                && add_linger()
                && add_blank_line();
    }
    return add_content_length(content_length)
            && add_linger()
            && add_blank_line();
}

bool http_conn::add_content_type(const char* type) {
    return add_response("Content-type:%s\r\n", type);
}

bool http_conn::add_content_length(int content_length) {
    return add_response("Content-Length:%d\r\n", content_length);
}

bool http_conn::add_linger() {
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool http_conn::process_write(HTTP_CODE ret) {
    std::cout << "process write, http_code: " << ret << std::endl;
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            } else {
                const char* ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string)) {
                    return false;
                }
            }
        }
        case CONFLICT: {
            const char* error_user_exists = "User has already exist.\n";
            add_status_line(400,error_400_title);
            add_headers(strlen(error_user_exists), true);
            if (!add_content(error_user_exists)) {
                return false;
            }
            break;
        }
        case CREATED: {
            add_status_line(201, ok_201_title);
            add_headers(strlen(ok_201_form), true);
            if (!add_content(ok_201_form)) {
                return false;
            }
            break;
        }
        case UNAUTHORISED: {
            const char* error_incorrect_user = "Username or password not correct.\n";
            add_status_line(400, error_400_title);
            add_headers(strlen(error_incorrect_user), true);
            if (!add_content(error_incorrect_user)) {
                return false;
            }
            break;
        }
        case OK: {
            add_status_line(200, ok_200_title);
            add_headers(strlen(ok_200_form), true);
            if (!add_content(ok_200_form)) {
                return false;
            }
            break;
        }
        default: {
            return false;
        }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

bool http_conn::write() {
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    // std::cout << "bytes to send: " << bytes_to_send << std::endl;
    
    // if response is empty
    if (bytes_to_send == 0) {
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }
    
    while (1) {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send) {
            unmap();
            if (m_linger) {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            } else {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}