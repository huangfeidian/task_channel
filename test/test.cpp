#include "task_channel"
using namespace spiritsaway::concurrency;
using test_task = std::function<int(int)>;
int fib(int a)
{
    if(a == 0 || a== 1)
    {
        return 1;
    }
    else
    {
        return fib(a-1) * fib(a - 2);
    }
}
void task_worker(task_channels<test_task, simple_queue>& task_source, std::uint32_t index)
{
    
}
int main()
{

}