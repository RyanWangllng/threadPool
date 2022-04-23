//
//  test.cpp
//  threadPool
//
//  Created by Ryan Wang.
//

#include "threadpool.h"

#include <chrono>

class Mytask : public Task {
public:
    Mytask(int begin, int end)
        : begin_(begin)
        , end_(end) {}
    
    Any run() {
        std::cout << "tid: " << std::this_thread::get_id() << " begin!"
            << std::endl;
        
//        std::this_thread::sleep_for(std::chrono::seconds(5));
        int sum = 0;
        for (int i = begin_; i < end_; ++i) {
            sum += i;
        }
        
        std::cout << "tid: " << std::this_thread::get_id() << " end!"
            << std::endl;
        
        return sum;
    }
private:
    int begin_;
    int end_;
};

int main() {

    ThreadPool pool;
    pool.start(4);
    
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    
    
    getchar();
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
