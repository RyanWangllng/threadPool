#include "threadPool.hpp"

void taskFunc(void* arg1, void* arg2) {
    int num1 = *static_cast<int*>(arg1);
    int num2 = *static_cast<int*>(arg2);
    std::cout << "thread " << std::to_string(pthread_self()) << " is working, number: " << num1 << " and " << num2 << std::endl;
    sleep(1);
}

int main() {
    // 创建线程池
    ThreadPool<int> pool(3, 10);
    for (int i = 0; i < 100; i++) {
        int *num1 = new int(i + 100);
        int *num2 = new int(i + 1000);
        pool.addTask(Task<int>(taskFunc, num1, num2));
    }

    sleep(20);

    return 0;
}