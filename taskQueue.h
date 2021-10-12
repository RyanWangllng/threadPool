#ifndef _TASKQUEUE_H
#define _TASKQUEUE_H

#include <queue>
#include <pthread.h>

using callback = void (*) (void* arg);

// 任务结构体
template <typename T>
class Task {
    public:
        // 函数指针
        Task() {
            function = nullptr;
            arg = nullptr;
        }
        Task(callback f, void* arg) {
            this->arg = static_cast<T*>(arg);
            function = f;
        }
        callback function;
        T* arg;
};

template <typename T>
class TaskQueue{
    public:
        TaskQueue();
        ~TaskQueue();

        // 添加任务
        void addTask(Task<T>& task);
        void addTask(callback f, void* arg);
        // 取一个任务
        Task<T> getTask();
        // 获取当前任务的个数
        inline size_t taskNum() {
            return m_queue.size();
        }
    private:
        std::queue<Task<T>> m_queue;   // 任务队列
        pthread_mutex_t m_mutex;    // 互斥锁
};

template <typename T>
TaskQueue<T>::TaskQueue() {
    pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue() {
    pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T>& task) {
    pthread_mutex_lock(&m_mutex);
    m_queue.push(task);
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(callback func, void* arg) {
    pthread_mutex_lock(&m_mutex);
    Task<T> task;
    task.function = func;
    task.arg = arg;
    m_queue.push(task);
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
Task<T> TaskQueue<T>::getTask() {
    Task<T> t;
    pthread_mutex_lock(&m_mutex);
    if (!m_queue.empty()) {
        t = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return t;
}

#endif