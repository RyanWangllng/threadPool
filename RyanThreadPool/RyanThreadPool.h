//
//  RyanThreadPool.h
//  RyanThreadPool
//
//  Created by Ryan Wang.
//

#ifndef ryanthreadpool_h
#define ryanthreadpool_h

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <unordered_map>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME  = 60; // 秒

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
    Thread(ThreadFunc func)
        : func_(func)
        , threadId_(generateId_++) {}

    // 线程析构函数
    ~Thread() = default;

    // 启动线程
    void start() {
        // 创建一个线程来执行一个线程函数
        std::thread t(func_, threadId_); // 出了作用域，线程对象t被销毁，线程函数func_一直在，所以要detach
        t.detach();
    }
    
    // 获取线程id
    uint getId() const {
        return threadId_;
    }
private:
    ThreadFunc func_;
    
    static uint generateId_;
    uint threadId_; //  线程id
};

uint Thread::generateId_ = 0;

// 线程池类型
class ThreadPool
{
public:
    // 初始化线程池
    ThreadPool()
        : initThreadNums_(0)
        , threadNumsMaxThreshold_(THREAD_MAX_THRESHHOLD)
        , idleThreadNums_(0)
        , threadNums_(0)
        , taskNums_(0)
        , taskNumsMaxThreshhold_(TASK_MAX_THRESHHOLD)
        , poolMode_(PoolMode::MODE_FIXED)
        , isRuning_(false) {}
    
    // 销毁线程池
    ~ThreadPool() {
        isRuning_ = false;
    //    notEmpty_.notify_all();
        
        // 等待线程池中所有线程返回（阻塞 and 运行）
        std::unique_lock<std::mutex> ulock(taskQueMtx_);
        notEmpty_.notify_all(); // 防止死锁
        exitCond_.wait(ulock, [&]() -> bool {
            return threads_.size() == 0;
        });
    }

    // 设置线程池工作模式
    void setMode(PoolMode mode) {
        if (checkRuningState()) return;
        poolMode_ = mode;
    }

    // 设置线程池cached模式下线程数量上限
    void setThreadNumMaxThreshHold(int threshHold) {
        if (checkRuningState()) return;
        if (poolMode_ == PoolMode::MODE_CACHED) {
            threadNumsMaxThreshold_ = threshHold;
        }
    }
    
    // 设置任务队列数量上限
    void setTaskQueMaxThreshHold(int threshHold) {
        if (checkRuningState()) return;
        taskNumsMaxThreshhold_ = threshHold;
    }

    // 给线程池提交任务
    // 使用可变参模板编程，让其可以接受任意任务函数和任意数量的参数
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        // 打包任务，放入任务队列
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();
                              
        // 获取锁
        std::unique_lock<std::mutex> ulock(taskQueMtx_);

        auto pred = [&]() -> bool {
            return taskQue_.size() < taskNumsMaxThreshhold_;
        };
        if (!notFull_.wait_for(ulock, std::chrono::seconds(1), pred)) {
            // 表示等待1s后，条件依然不满足
            std::cerr << "task queue is full, submit task fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>([]() -> RType {
                return RType();
            });
            (*task)();
            return task->get_future();
        }
                              
        // 将任务添加进任务队列中
        taskQue_.emplace([task]() { (*task)(); });
        ++taskNums_;
                              
        // 任务队列中新增了任务，任务队列肯定不空，notEmpty_上通知消费
        notEmpty_.notify_all();
                                  
        // cached模式，场景为使用小而快的任务；根据任务数量和空闲线程的数量，判断是否需要创建爱你新的线程出来
        if (poolMode_ == PoolMode::MODE_CACHED
            && taskNums_ > idleThreadNums_
            && threadNums_ < threadNumsMaxThreshold_) {
                            
            std::cout << " ===>>> creeate new thread <<<=== " << std::endl;
                                    
            // 创建新线程
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            uint threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr)); // unique_ptr只能右值拷贝
            threads_[threadId]->start();
            //        threads_.emplace_back(std::move(ptr));
            ++threadNums_;
            ++idleThreadNums_;
        }
                            
        // 返回任务的Result对象
        //    return task->getResult(); // 线程执行完task，task对象就被析构了，依赖于task对象的Result对象也没了，这种方式不行。
        return result;
                              
    }

    // 启动线程池
    void start(int initThreadNums) {
        // 设置线程池运行状态
        isRuning_ = true;
        
        // 记录线程池初始线程个数
        initThreadNums_ = initThreadNums;
        threadNums_ = initThreadNums;

        // 创建线程对象
        for (int i = 0; i < initThreadNums_; ++i) {
            // 创建线程对象的时候，需要把线程函数给到线程对象
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            uint threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
    //        threads_.emplace_back(std::move(ptr)); // unique_ptr只能右值拷贝
        }

        // 启动所有线程
        for (int i = 0; i < initThreadNums_; ++i) {
            threads_[i]->start(); // 需要执行一个线程函数
            ++idleThreadNums_; // 每启动一个线程，空闲线程数量就加一
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void threadFunc(uint threadid) {
    //    std::cout << "begin threadFunc tid: " << std::this_thread::get_id()
    //        << std::endl;
        auto lastTime = std::chrono::high_resolution_clock().now();
        
        // 所有任务必须执行完成，才能回收线程资源
        for (;;) {
            Task task;
            {
                // 先获取锁
                std::unique_lock<std::mutex> ulock(taskQueMtx_);
                std::cout << "tid: " << std::this_thread::get_id() << " get task from queue!"
                    << std::endl;
                
                // cached模式下，创建出来的线程若空闲时间超过60s（当前时间 - 上一次线程执行的时间），应该将其回收
                // 超过initThreadNums_数量的线程要回收
                // 双重判断 + 锁：预防死锁
                while (taskQue_.size() == 0) { // 没有任务才看看要不要回收线程
                    // 线程池要结束，回收线程资源
                    if (!isRuning_) {
                        threads_.erase(threadid);
                        std::cout << "threadid: " << std::this_thread::get_id()
                            << " exit!" << std::endl;
                        exitCond_.notify_all(); // 通知主线程
                        return; // 线程函数结束，线程结束
                    }
                    
                    if (poolMode_ == PoolMode::MODE_CACHED) {
                        // 条件变量超时返回，每秒中返回一次
                        if (std::cv_status::timeout
                            == notEmpty_.wait_for(ulock, std::chrono::seconds(1))) {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (dur.count() >= THREAD_MAX_IDLE_TIME
                                && threadNums_ > initThreadNums_) {
                                // 回收当前线程
                                // 更改线程数量相关值
                                // 线程列表中移除线程对象，通过threadid找到线程对象然后再移除
                                threads_.erase(threadid);
                                --threadNums_;
                                --idleThreadNums_;
                                
                                std::cout << "threadid: " << std::this_thread::get_id()
                                    << " exit!" << std::endl;
                                return;
                            }
                        }
                    } else {
                        notEmpty_.wait(ulock);
                    }
                }
                
                --idleThreadNums_;
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
                task(); // 执行function<void()>;
            }
            
            ++idleThreadNums_;
            // 更新线程执行完任务的时间
            lastTime = std::chrono::high_resolution_clock().now();
        }
        
    //    std::cout << "end threadFunc tid: " << std::this_thread::get_id()
    //        << std::endl;
    }
    
    // 检查线程池的运行状态
    bool checkRuningState() const {
        return isRuning_;
    }
private:
    PoolMode poolMode_; // 当前线程池的工作模式
    std::atomic_bool isRuning_; // 判断线程池的运行状态

    std::unordered_map<uint, std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadNums_; // 初始的线程数量
    std::atomic_uint threadNums_;  // 线程池中线程总数量
    size_t threadNumsMaxThreshold_; // 线程数量的上限
    std::atomic_uint idleThreadNums_; // 空闲线程的数量

    using Task = std::function<void()>;
    std::queue<Task> taskQue_; // 任务队列
    std::atomic_uint taskNums_; // 任务数量
    size_t taskNumsMaxThreshhold_; // 任务队列中任务数量的上限

    std::mutex taskQueMtx_; // 保证任务队列线程安全的互斥锁
    std::condition_variable notFull_; // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等待线程资源全部回收
};


#endif /* ryanthreadpool_h */

