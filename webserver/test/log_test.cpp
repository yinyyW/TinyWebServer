#include "../include/log.h"
#include <thread>
#include <iostream>
#include <unistd.h>

void log_info() {
    LOG_INFO("%s\n", "log info test");
}

void log_debug() {
    LOG_DEBUG("%s\n", "log debug test");
}

void log_warn() {
    LOG_WARN("%s\n", "log warn test");
}

void log_error() {
    LOG_ERROR("%s\n", "log error test");
}

int main(int argc, char* argv[]) {
    Log::get_instance()->init("./ServerLog", 0, 2000, 
                            800000, 800);
    std::thread t1 = std::thread(log_info);
    std::thread t2 = std::thread(log_debug);
    std::thread t3 = std::thread(log_warn);
    std::thread t4 = std::thread(log_error);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    sleep(5);
    return 0;
}