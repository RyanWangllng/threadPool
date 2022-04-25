//
//  threadpool.cpp
//  threadPool
//
//  Created by Ryan Wang.
//

#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME  = 60; // 秒

////////////* 线程池方法实现 *////////////

// 初始化线程池
ThreadPool::ThreadPool()
    : initThreadNums_(0)
    , threadNumsMaxThreshold_(THREAD_MAX_THRESHHOLD)
    , idleThreadNums_(0)
    , threadNums_(0)
    , taskNums_(0)
    , taskNumsMaxThreshhold_(TASK_MAX_THRESHHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    , isRuning_(false) {}

// 销毁线程池
ThreadPool::~ThreadPool() {
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
void ThreadPool::setMode(PoolMode mode) {
    if (checkRuningState()) return;
    poolMode_ = mode;
}

// 设置线程池cached模式下线程数量上限
void ThreadPool::setThreadNumMaxThreshHold(int threshHold) {
    if (checkRuningState()) return;
    if (poolMode_ == PoolMode::MODE_CACHED) {
        threadNumsMaxThreshold_ = threshHold;
    }
}

// 设置任务队列数量上限
void ThreadPool::setTaskQueMaxThreshHold(int threshHold) {
    if (checkRuningState()) return;
    taskNumsMaxThreshhold_ = threshHold;
}

// 给线程池提交任务，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sPtr) {
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
        return Result(sPtr, false);
    }

    // 将任务添加进任务队列中
    taskQue_.emplace(sPtr);
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
    return Result(sPtr);
}

// 启动线程池
void ThreadPool::start(int initThreadNums) {
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

// 定义线程函数，消费任务
void ThreadPool::threadFunc(uint threadid) {
//    std::cout << "begin threadFunc tid: " << std::this_thread::get_id()
//        << std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();
    
    // 所有任务必须执行完成，才能回收线程资源
    for (;;) {
        std::shared_ptr<Task> task;
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
                    // 等待notEmpty_条件
//                    auto pred = [&]() -> bool {
//                        return taskNums_ > 0;
//                    }; // 这个条件交给外面的while循环
                    notEmpty_.wait(ulock);
                }
                
//                // 线程池要结束，回收线程资源
//                if (!isRuning_) {
//                    threads_.erase(threadid);
//                    std::cout << "threadid: " << std::this_thread::get_id()
//                        << " exit!" << std::endl;
//                    exitCond_.notify_all(); // 通知主线程
//                    return;
//                }
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
//            std::cout << "tid: " << std::this_thread::get_id() << " run!"
//                << std::endl;
//            task->run(); // 执行任务；将任务的返回值通过setVal给到Result
            task->exec();
        }
        
        ++idleThreadNums_;
        // 更新线程执行完任务的时间
        lastTime = std::chrono::high_resolution_clock().now();
    }
    
//    std::cout << "end threadFunc tid: " << std::this_thread::get_id()
//        << std::endl;
}

// 检查线程池的运行状态
bool ThreadPool::checkRuningState() const {
    return isRuning_;
}

////////////* Task方法实现 *////////////
Task::Task()
    : result_(nullptr) {}

void Task::exec() {
    if (result_ != nullptr) {
        result_->setVal(run()); // 这里发生多态调用
    }
}

void Task::setResult(Result *res) {
    result_ = res;
}

////////////* 线程方法实现 *////////////

uint Thread::generateId_ = 0;

// 线程构造函数
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++) {}

// 线程析构函数
Thread::~Thread() {}

// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_, threadId_); // 出了作用域，线程对象t被销毁，线程函数func_一直在，所以要detach
    t.detach();
//    t.join();
}

// 获取线程id
uint Thread::getId() const {
    return threadId_;
}

////////////* Result方法实现 *////////////

// 构造函数
Result::Result(std::shared_ptr<Task> task, bool isValid)
    : isValid_(isValid)
    , task_(task) {
        task_->setResult(this);
    }


// 用户获取task的返回值
Any Result::get() {
    if (!isValid_) {
        return "";
    }
    sem_.wait(); // task任务如果没有执行完，就会阻塞在这里
    return std::move(any_);
}

// 获取任务执行完的返回值，存入Any类型
void Result::setVal(Any any) {
    // 存储task的返回值
    this->any_ = std::move(any);
    
    // 已经获取任务返回值，增加信号量资源
    sem_.post();
}
