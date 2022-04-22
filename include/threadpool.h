#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

// 任务抽象基类
class Task {
public:
    // 用户可以自定义任务类型，继承该基类，重写run方法，实现自定义任务处理
    virtual void run() = 0;
};

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED,  // 固定模式
    MODE_CACHED, // 动态增长模式
};

// 线程类型
class Thread {
public:
    // 线程函数对象类型
    using ThreadFunc = std::function<void()>;
    
    // 线程构造函数
    Thread(ThreadFunc func);

    // 线程析构函数
    ~Thread();

    // 启动线程
    void start();
private:
    ThreadFunc func_;
};

// 线程池类型
class ThreadPool
{
public:
    // 初始化线程池
    ThreadPool();
    
    // 销毁线程池
    ~ThreadPool();

    // 设置线程池工作模式
    void setMode(PoolMode mode);

    // 设置任务队列数量上限
    void setTaskQueMaxThreshHold(int threshHold);

    // 给线程池提交任务
    void submitTask(std::shared_ptr<Task> sPtr);

    // 启动线程池
    void start(int initThreadNums = 2);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void threadFunc();

private:
    PoolMode poolMode_; // 当前线程池的工作模式

    std::vector<Thread*> threads_; // 线程列表
    size_t initThreadNums_; // 初始的线程数量

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskNums_; // 任务数量
    size_t taskNumsMaxThreshhold_; // 任务队列中任务数量的上限

    std::mutex taskQueMtx_; // 保证任务队列线程安全的互斥锁
    std::condition_variable notFull_; // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
};

#endif