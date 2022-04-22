#include "threadpool.h"

#include <chrono>

int main() {

    ThreadPool pool;
    pool.start();
    
    // 
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}