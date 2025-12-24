#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include "../include/task_channel.h"
#include "test_task.h"

using TaskChannel = spiritsaway::concurrency::task_channels<TestTask>;
using TaskPtr = std::shared_ptr<TestTask>;

// 测试基本功能
void test_basic_functionality()
{
	std::cout << "=== 测试基本功能 ===\n";

	TaskChannel channel;

	// 添加任务到不同channel
	channel.add_task(std::make_shared<TestTask>(1, 1, "Task 1"));
	channel.add_task(std::make_shared<TestTask>(2, 2, "Task 2"));
	channel.add_task(std::make_shared<TestTask>(0, 3, "Default channel task"));

	// 获取并完成任务
	TaskPtr task1 = channel.poll_one_task(0, 1);
	assert(task1 != nullptr);
	std::cout << "get_task: Channel=" << task1->channel_id()
			  << ", TaskID=" << task1->task_id()
			  << ", Data=" << task1->data() << std::endl;
	channel.finish_task(task1);

	TaskPtr task2 = channel.poll_one_task(1, 1);
	assert(task2 != nullptr);
	std::cout << "get_task: Channel=" << task2->channel_id()
			  << ", TaskID=" << task2->task_id()
			  << ", Data=" << task2->data() << std::endl;
	channel.finish_task(task2);

	TaskPtr task3 = channel.poll_one_task(2, 1);
	assert(task3 != nullptr);
	std::cout << "get_task: Channel=" << task3->channel_id()
			  << ", TaskID=" << task3->task_id()
			  << ", Data=" << task3->data() << std::endl;
	channel.finish_task(task3);

	// 验证所有任务完成
	assert(channel.tasks_all_finished() == true);
	assert(channel.poll_one_task(0, 1) == nullptr);
	std::cout << "finish all tasks!\n\n";
}


void test_channel_switch(int pref_channel)
{
	std::cout << "=== 测试基本功能 ===\n";

	TaskChannel channel;

	// 添加任务到不同channel
	channel.add_task(std::make_shared<TestTask>(1, 1, "Task 1"));
	channel.add_task(std::make_shared<TestTask>(2, 2, "Task 2"));
	channel.add_task(std::make_shared<TestTask>(0, 3, "Default channel task"));

	// 获取并完成任务
	TaskPtr task1 = channel.poll_one_task(pref_channel, 1);
	assert(task1 != nullptr);
	std::cout << "get_task: Channel=" << task1->channel_id()
			  << ", TaskID=" << task1->task_id()
			  << ", Data=" << task1->data() << std::endl;
	channel.finish_task(task1);

	TaskPtr task2 = channel.poll_one_task(pref_channel, 1);
	assert(task2 != nullptr);
	std::cout << "get_task: Channel=" << task2->channel_id()
			  << ", TaskID=" << task2->task_id()
			  << ", Data=" << task2->data() << std::endl;
	channel.finish_task(task2);

	TaskPtr task3 = channel.poll_one_task(pref_channel, 1);
	assert(task3 != nullptr);
	std::cout << "get_task: Channel=" << task3->channel_id()
			  << ", TaskID=" << task3->task_id()
			  << ", Data=" << task3->data() << std::endl;
	channel.finish_task(task3);

	// 验证所有任务完成
	assert(channel.tasks_all_finished() == true);
	assert(channel.poll_one_task(0, 1) == nullptr);
	std::cout << "finish all tasks!\n\n";
}

// test_multi_threading
void test_multithreading()
{
	std::cout << "=== test_multi_threading ===\n";

	TaskChannel channel;
	const int TASK_COUNT = 100;
	const int WORKER_COUNT = 5;

	// 添加大量任务到不同channel
	for (int i = 0; i < TASK_COUNT; ++i)
	{
		int channel_id = i % 10; // 使用10个不同的channel
		channel.add_task(std::make_shared<TestTask>(channel_id, i, "Task " + std::to_string(i)));
	}

	std::vector<std::thread> workers;
	std::atomic<int> processed_count = 0;

	// 创建多个worker
	for (int worker_id = 0; worker_id < WORKER_COUNT; ++worker_id)
	{
		workers.emplace_back([&channel, worker_id, &processed_count, TASK_COUNT]()
							 {
			// 每个worker优先处理一个channel
			int prefer_channel = worker_id % 10;
			while (true) {

				TaskPtr task = channel.poll_one_task(prefer_channel, worker_id + 1);
				if (task == nullptr) {
					// 所有任务处理完成
					break;
				}
				std::cout << "Worker " << worker_id << " poll_one_task: Channel=" 
						  << task->channel_id() << ", TaskID=" << task->task_id() << std::endl;
				prefer_channel = task->channel_id();
				// 模拟任务处理
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				
				channel.finish_task(task);
				processed_count++;
				
				// 每处理10个任务输出一次
				if (processed_count % 10 == 0) {
					std::cout << "Worker " << worker_id << " finish_task: Channel=" 
							  << task->channel_id() << ", TaskID=" << task->task_id() 
							  << ", process_count: " << processed_count << "/" << TASK_COUNT << std::endl;
				}
			} });
	}

	// 等待所有worker完成
	for (auto &worker : workers)
	{
		worker.join();
	}

	// 验证所有任务完成
	assert(channel.tasks_all_finished() == true);
	assert(processed_count == TASK_COUNT);
	std::cout << "finish multi_threading! processed_count: " << processed_count << "/" << TASK_COUNT << std::endl;
}

// test channel allocation
void test_channel_allocation()
{
	std::cout << "=== test channel allocation ===\n";

	TaskChannel channel;

	// 添加多个相同channel的任务
	const int SAME_CHANNEL_TASKS = 5;
	const int CHANNEL_ID = 100;

	for (int i = 0; i < SAME_CHANNEL_TASKS; ++i)
	{
		channel.add_task(std::make_shared<TestTask>(CHANNEL_ID, i, "Channel " + std::to_string(CHANNEL_ID) + " Task " + std::to_string(i)));
	}

	// 获取该channel的所有任务
	int retrieved_count = 0;
	while (true)
	{
		TaskPtr task = channel.poll_one_task(CHANNEL_ID, 1);
		if (task == nullptr)
		{
			break;
		}

		std::cout << "get_task: Channel=" << task->channel_id()
				  << ", TaskID=" << task->task_id()
				  << ", Data=" << task->data() << std::endl;

		channel.finish_task(task);
		retrieved_count++;
	}

	assert(retrieved_count == SAME_CHANNEL_TASKS);
	std::cout << "finish test_channel_allocation " << retrieved_count << " tasks from channel " << CHANNEL_ID << "\n\n";
}

// test default channel
void test_default_channel()
{
	std::cout << "=== test default channel ===\n";

	TaskChannel channel;

	// 添加默认channel的任务
	const int DEFAULT_CHANNEL_TASKS = 3;

	for (int i = 0; i < DEFAULT_CHANNEL_TASKS; ++i)
	{
		channel.add_task(std::make_shared<TestTask>(0, i, "Default channel task " + std::to_string(i)));
	}

	// 获取默认channel的任务
	int retrieved_count = 0;
	while (true)
	{
		TaskPtr task = channel.poll_one_task(0, 1);
		if (task == nullptr)
		{
			break;
		}

		std::cout << "get task from default channel: TaskID=" << task->task_id()
				  << ", Data=" << task->data() << std::endl;

		channel.finish_task(task);
		retrieved_count++;
	}

	assert(retrieved_count == DEFAULT_CHANNEL_TASKS);
	std::cout << "finish default channel " << retrieved_count << " tasks\n\n";
}

// test channel status
void test_task_status()
{
	std::cout << "=== test channel status ===\n";

	TaskChannel channel;

	// 添加任务
	channel.add_task(std::make_shared<TestTask>(1, 1, "Task 1"));
	channel.add_task(std::make_shared<TestTask>(2, 2, "Task 2"));

	// 验证任务未完成
	assert(channel.tasks_all_finished() == false);
	std::cout << "check tasks_all_finished() == false\n";

	// 获取并完成第一个任务
	TaskPtr task1 = channel.poll_one_task(1, 1);
	assert(task1 != nullptr);
	channel.finish_task(task1);

	// 验证任务仍未全部完成
	assert(channel.tasks_all_finished() == false);
	std::cout << "check tasks_all_finished() == false\n";

	// 获取并完成第二个任务
	TaskPtr task2 = channel.poll_one_task(2, 1);
	assert(task2 != nullptr);
	channel.finish_task(task2);

	// 验证所有任务完成
	assert(channel.tasks_all_finished() == true);
	std::cout << "check tasks_all_finished() == true\n\n";
}

int main()
{
	try
	{
		for (int i = 0; i < 20; i++)
		{
			test_basic_functionality();
			test_channel_switch(i);
			test_multithreading();
			test_channel_allocation();
			test_default_channel();
			test_task_status();
		}
		

		std::cout << "all test cases pass!\n";
		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << "fail test case: " << e.what() << std::endl;
		return 1;
	}
}