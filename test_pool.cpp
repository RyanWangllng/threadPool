#include "threadPool.hpp"

void taskFunc(void* arg) {
    int num = *static_cast<int*>(arg);
    std::cout << "thread " << std::to_string(pthread_self()) << " is working, number = " << num << std::endl;
    sleep(1);
}

int main() {
    // 创建线程池
    ThreadPool<int> pool(3, 10);
    for (int i = 0; i < 100; i++) {
        int *num = new int(i + 100);
        pool.addTask(Task<int>(taskFunc, num));
    }

    sleep(20);

    return 0;
}