//
//  threadpool.h
//  threadPool
//
//  Created by Ryan Wang.
//

#ifndef threadpool_h
#define threadpool_h

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <chrono>
#include <unordered_map>

// Any类型，可以接收任意的数据类型
class Any {
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;
    
    // 此构造函数让Any类型接收任意其它的数据
    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data)) {}
    
    // 此方法可以将Any对象里面存储的data数据提取出来
    template<typename T>
    T cast_() {
        // 从base_里面找到它所指向的Derive对象，从它里面取出data成员变量
        Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
        if (pd == nullptr) {
            throw "type is not match!";
        }
        return pd->data_;
    }
private:
    // 基类类型
    class Base {
    public:
        virtual ~Base() = default;
    };
    
    // 派生类类型
    template<typename T>
    class Derive : public Base {
    public:
        Derive(T data) :data_(data) {}

        T data_; // 保存任意的其他类型
    };
private:
    std::unique_ptr<Base> base_;
};

// 信号量类型实现
class Semaphore {
public:
    Semaphore(int count = 0)
        : resCount_(count) {}
    ~Semaphore() = default;
    
    // 获取一个信号量资源
    void wait() {
//        std::cout << "thread: " << std::this_thread::get_id()
//            << " sem_.wait begin..." << std::endl;
        std::unique_lock<std::mutex> ulock(mtx_);
        
        // 等待信号量资源，若信号量为0，就阻塞当前线程
        auto pred = [&]() -> bool {
            return resCount_ > 0;
        };
        condv_.wait(ulock, pred);
        --resCount_;
//        std::cout << "thread: " << std::this_thread::get_id()
//            << " sem_.wait get..." << std::endl;
    }
    
    // 增加一个信号量资源
    void post() {
//        std::cout << "thread: " << std::this_thread::get_id()
//            << " sem_.post" << std::endl;
        std::unique_lock<std::mutex> ulock(mtx_);
        ++resCount_;
        condv_.notify_all();
    }
private:
    int resCount_;
    std::mutex mtx_;
    std::condition_variable condv_;
};

class Task;

// 线程池Task执行完的返回值类型Result实现
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;
    
    // 获取任务执行完的返回值，存入Any类型
    void setVal(Any any);
    
    // 用户获取task的返回值
    Any get();
private:
    Any any_; // 存储任务的返回值
    Semaphore sem_; // 线程通信信号量
    std::shared_ptr<Task> task_; // 指向对应获取返回值的任务对象
    std::atomic_bool isValid_; // 返回值是否有效
};

// 任务抽象基类
class Task {
public:
    Task();
    ~Task() = default;
    
    void exec();
    void setResult(Result* res);
    
    // 用户可以自定义任务类型，继承该基类，重写run方法，实现自定义任务处理
    virtual Any run() = 0;
private:
    // Result的生命周期肯定大于Task
    Result* result_; // 不能用强智能指针，会循环引用
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
    using ThreadFunc = std::function<void(uint)>;
    
    // 线程构造函数
    Thread(ThreadFunc func);

    // 线程析构函数
    ~Thread();

    // 启动线程
    void start();
    
    // 获取线程id
    uint getId() const;
private:
    ThreadFunc func_;
    
    static uint generateId_;
    uint threadId_; //  线程id
};

/*
example:
 TreadPool pool;
 pool.start(4);
 
 class MyTask : public Task {
 public:
    void run() { ... } // 重写基类run方法
 };
 
 pool.submitTask(std::make_shared<MyTask>());
*/

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

    // 设置线程池cached模式下线程数量上限
    void setThreadNumMaxThreshHold(int threshHold);
    
    // 设置任务队列数量上限
    void setTaskQueMaxThreshHold(int threshHold);

    // 给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sPtr);

    // 启动线程池
    void start(int initThreadNums = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void threadFunc(uint threadid);
    
    // 检查线程池的运行状态
    bool checkRuningState() const;
private:
    PoolMode poolMode_; // 当前线程池的工作模式
    std::atomic_bool isRuning_; // 判断线程池的运行状态

//    std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<uint, std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadNums_; // 初始的线程数量
    std::atomic_uint threadNums_;  // 线程池中线程总数量
    size_t threadNumsMaxThreshold_; // 线程数量的上限
    std::atomic_uint idleThreadNums_; // 空闲线程的数量

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskNums_; // 任务数量
    size_t taskNumsMaxThreshhold_; // 任务队列中任务数量的上限

    std::mutex taskQueMtx_; // 保证任务队列线程安全的互斥锁
    std::condition_variable notFull_; // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等待线程资源全部回收
};

#endif /* threadpool_h */
