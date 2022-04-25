//
//  main.cpp
//  threadPool
//
//  Created by Ryan Wang.
//

#include "threadpool.h"

using ull = unsigned long long;

class Mytask : public Task {
public:
    Mytask(int begin, int end)
        : begin_(begin)
        , end_(end) {}
    
    Any run() { // 在线程池分配的线程中去执行
        std::cout << "tid: " << std::this_thread::get_id() << " begin!"
            << begin_ << " -> " << end_ << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ull sum = 0;
        for (int i = begin_; i <= end_; ++i) {
            sum += i;
        }
        
        std::cout << "tid: " << std::this_thread::get_id() << " end!"
            << begin_ << " -> " << end_ << std::endl;
        
        return sum;
    }
private:
    int begin_;
    int end_;
};

int main() {

    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    
    Result res1 = pool.submitTask(std::make_shared<Mytask>(1, 100000000));
    Result res2 = pool.submitTask(std::make_shared<Mytask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));
    Result res4 = pool.submitTask(std::make_shared<Mytask>(300000001, 400000000));
    
    Result res5 = pool.submitTask(std::make_shared<Mytask>(400000001, 500000000));
    Result res6 = pool.submitTask(std::make_shared<Mytask>(500000001, 600000000));
    
    ull sum1 = res1.get().cast_<ull>();
    ull sum2 = res2.get().cast_<ull>();
    ull sum3 = res3.get().cast_<ull>();
    ull sum4 = res4.get().cast_<ull>();
    ull sum5 = res5.get().cast_<ull>();
    ull sum6 = res6.get().cast_<ull>();
    
    ull sum = sum1 + sum2 + sum3 + sum4 + sum5 + sum6;
    std::cout << "sum = " << sum << std::endl;
    

    /*ull sum_true = 0;
    for (int i = 1; i <= 300000000; ++i) {
        sum_true += i;
    }
    std::cout << "sum_true = " << sum_true << std::endl;*/
    
    getchar();
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
