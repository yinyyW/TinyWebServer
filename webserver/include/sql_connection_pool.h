#include <list>
#include <string>
#include <iostream>
#include <mysql/mysql.h>
#include "../include/locker.h"

class connection_pool {
    public:
        static connection_pool* getInstance();
        void init(std::string url, std::string user, std::string passWord, 
                std::string databaseName, int port, int maxConn, int close_log);
        MYSQL* getConnection();
    	bool releaseConnection(MYSQL *conn);
    	int getFreeConn();
    	void destroyPool();
        
    private:
        connection_pool();
        ~connection_pool();
    
    private:
        int m_maxConn;
        int m_currConn;
        int m_freeConn;
        locker lock;
        std::list<MYSQL*> connList;
        sem reserve;
        
    public:
        std::string m_url;
	    std::string m_port;
	    std::string m_user;
	    std::string m_passWord;
	    std::string m_databaseName;
        int m_close_log;
};

class connectionRAII {
    public:
        connectionRAII(MYSQL** mysql, connection_pool* pool);
        ~connectionRAII();
        
    private:
        MYSQL* connRAII;
        connection_pool* poolRAII;
        
};