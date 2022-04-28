## ThreadPool
### 涉及技术：
- C++、线程间通信、可变参数模板、lambda表达式
- g++编译动态库，GDB多线程调试
### 特性：
- 支持任意数量参数的函数传递
- 哈希表和队列管理线程对象和任务
- 支持线程池双模式切换
### TreadPool
> 此目录下编译好的动态库。 需要用户继承任务基类，重写run方法。
#### 编译
```bash
git clone git@github.com:RyanWangllng/threadPool.git

cd threadPool/ThreadPool

g++ -fPIC -shared threadpool.cpp -o libryanpool.so -std=c++17
```
#### 使用方法
```bash
mv libryanpool.so /usr/local/lib/

mv threadpool.h /usr/local/include/

cd /etc/ld.so.conf.d/

vim ryanlib.conf # 在其中加入刚才动态库的路径

ldconfig # 将动态库刷新到 /etc/ld.so.cahce中

# 编译时连接动态库
g++ test.cpp -std=c++17 -lryanpool -lpthread
```
#### 使用示例
```cpp
class Mytask : public Task {
public:
    Mytask(int para1, int para2)
        : para1_(para1)
        , para2_(para2) {}

    Any run() {
        return para1_ + para2_;
    }
private:
    int para1_;
    int para2_;
};

int main() {
    ThreadPool pool;
    pool.start(4);
        
    Result res1 = pool.submitTask(std::make_shared<Mytask>(1, 2));
    Result res2 = pool.submitTask(std::make_shared<Mytask>(2, 3));
    Result res3 = pool.submitTask(std::make_shared<Mytask>(3, 4));
    int sum1 = res1.get().cast_<int>();
    int sum2 = res2.get().cast_<int>();
    int sum3 = res3.get().cast_<int>();
    return 0;
}
```
### RyanThreadPool
> 使用可变参数模板，支持任意数量参数的任务函数加入线程池任务队列
#### 使用方法
> Header only. 直接包含头文件即可
#### 使用示例
```cpp
ThreadPool pool;
pool.start(2);
auto f = [](int a, int b) -> int {
    int sum = 0;
    for (int i = a; i <= b; ++i) {
        sum += i;
    }
    return sum;
};
std::future<int> r2 = pool.submitTask(f, 1, 100);

std::cout << r1.get() << std::endl;
```