//
//  threadpool.cpp
//  threadPool
//
//  Created by Ryan Wang.
//

#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 2;

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
    // 获取锁
    std::unique_lock<std::mutex> ulock(taskQueMtx_);

    // 线程通信，等待任务队列有空余位置
    // 最长等待时间不能超过1s，否则就判定提交任务失败，返回
    /*while (taskQue_.size() == taskNumsMaxThreshhold_) {
        notFull_.wait(ulock); // 等待
    }*/
    auto pred = [&]() -> bool {
        return taskQue_.size() < taskNumsMaxThreshhold_;
    };
    if (!notFull_.wait_for(ulock, std::chrono::seconds(1), pred)) {
        // 表示等待1s后，条件依然不满足
        std::cerr << "task queue is full, submit task fail." << std::endl;
        return;
    }

    // 将任务添加进任务队列中
    taskQue_.emplace(sPtr);
    ++taskNums_;

    // 任务队列中新增了任务，任务队列肯定不空，notEmpty_上通知消费
    notEmpty_.notify_all();
}

// 启动线程池
void ThreadPool::start(int initThreadNums) {
    // 记录线程池初始线程个数
    initThreadNums_ = initThreadNums;

    // 创建线程对象
    for (int i = 0; i < initThreadNums_; ++i) {
        // 创建线程对象的时候，需要把线程函数给到线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr)); // unique_ptr只能右值拷贝
    }

    // 启动所有线程
    for (int i = 0; i < initThreadNums_; ++i) {
        threads_[i]->start();
    }
}

// 定义线程函数，消费任务
void ThreadPool::threadFunc() {
//    std::cout << "begin threadFunc tid: " << std::this_thread::get_id()
//        << std::endl;

    for (;;) {
        std::shared_ptr<Task> task;

        {
            // 先获取锁
            std::unique_lock<std::mutex> ulock(taskQueMtx_);
            std::cout << "tid: " << std::this_thread::get_id() << " get task from queue!"
                << std::endl;
            
            // 等待notEmpty_条件
            auto pred = [&]() -> bool {
                return taskNums_ > 0;
            };
            notEmpty_.wait(ulock, pred);
            std::cout << "tid: " << std::this_thread::get_id() << " get task success!"
                << std::endl;

            // 取出一个任务
            task = taskQue_.front();
            taskQue_.pop();
            --taskNums_;

            // 队列中不止一个任务，通知其他线程获取任务
            if (taskNums_ > 0) {
                notEmpty_.notify_all();
            }

            // 取出任务，队列不满了，notFull_上通知生产
            notFull_.notify_all();
        } // 锁释放，其他线程可以获取锁操作任务队列

        // 当前线程执行该任务
        if (task != nullptr) {
//            std::cout << "tid: " << std::this_thread::get_id() << " run!"
//                << std::endl;
            task->run();
        }
    }

//    std::cout << "end threadFunc tid: " << std::this_thread::get_id()
//        << std::endl;
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
//    t.join();
}
