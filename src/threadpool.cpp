#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;

////////////* 线程池方法实现 *////////////

// 初始化线程池
ThreadPool::ThreadPool()
    : initThreadNums_(0)
    , taskNums_(0)
    , taskNumsMaxThreshhold_(TASK_MAX_THRESHHOLD)
    , poolMode_(PoolMode::MODE_FIXED) {}

// 销毁线程池
ThreadPool::~ThreadPool() {}

// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
    poolMode_ = mode;
}

// 设置任务队列数量上限
void ThreadPool::setTaskQueMaxThreshHold(int threshHold) {
    taskNumsMaxThreshhold_ = threshHold;
}

// 给线程池提交任务，生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sPtr) {

}

// 启动线程池
void ThreadPool::start(int initThreadNums) {
    // 记录线程池初始线程个数
    initThreadNums_ = initThreadNums;

    // 创建线程对象
    for (int i = 0; i < initThreadNums_; ++i) {
        // 创建线程对象的时候，需要把线程函数给到线程对象
        threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
    }

    // 启动所有线程
    for (int i = 0; i < initThreadNums_; ++i) {
        threads_[i]->start();
    }
}

// 定义线程函数，消费任务
void ThreadPool::threadFunc() {
    std::cout << "begin threadFunc tid: " << std::this_thread::get_id() 
        << std::endl;
    std::cout << "end threadFunc tid: " << std::this_thread::get_id() 
        << std::endl;
}

////////////* 线程方法实现 *////////////

// 线程构造函数
Thread::Thread(ThreadFunc func) 
    : func_(func) {}

// 线程析构函数
Thread::~Thread() {}

// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_); // 出了作用域，线程对象t被销毁，线程函数func_一直在，所以要detach
    t.detach();
}