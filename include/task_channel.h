#pragma once
#include <unordered_map>
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <cstdint>
#include <atomic>
namespace spiritsaway::concurrency
{
	// T for task type
	// channel_type T::channel_id()
	const std::uint32_t queue_size = 16;
	template <typename T>
	class simple_queue
	{
	protected:
		std::deque<task_ptr> _queue;
		std::mutex _mutex;
	public:
		simple_queue(const simple_queue& other) = delete;
		simple_queue& operator=(const simple_queue& other) = delete;
		void push_fromt(T&& data)
		{
			std::lock_guard<std::mutex> cur_lock;
			_queue.push_front(std::forward<T>(data));
			return;
		}
		void push_back(T&& data)
		{
			std::lock_guard<std::mutex> cur_lock;
			_queue.push_back(std::forward<T>(data));
		}
		bool pop_front(T& dest)
		{
			std::lock_guard<std::mutex> cur_lock;
			if(_queue.empty())
			{
				return false;
			}
			else
			{
				dest = _queue.front();
				_queue.pop_front();
				return true;
			}
		}
		bool pop_back(T& dest)
		{
			std::lock_guard<std::mutex> cur_lock;
			if(_queue.empty())
			{
				return false;
			}
			else
			{
				dest = _queue.back();
				_queue.pop_back();
				return true;
			}
		}

	};
	template <typename T,  template<typename T2> typename con_queue>
	class task_channels
	{
	public:
		using channel_type = typename T::channel_type;
		using task_ptr = std::shared_ptr<T>;
		struct task_queue
		{
			con_queue<task_ptr> _queue;
			std::atomic<std::uint32_t> executor_id = 0;
			bool claim(std::uint32_t new_exec_id)
			{
				if(new_exec_id == 0)
				{
					return false;
				}
				while(executor_id == 0)
				{
					if(executor_id.compare_exchange_strong(0, new_exec_id))
					{
						return true;
					}
				}
				return false;
			}
			bool renounce(std::uint32_t pre_exec_id)
			{
				while(executor_id == pre_exec_id)
				{
					if(executor_id.compare_exchange_strong(pre_exec_id, 0))
					{
						return true;
					}
				}
				return false;
			}
		};
	protected:
		std::array<task_queue, queue_size> _tasks_by_channel;
		std::mutex _task_mutex;
		std::atomic<std::uint32_t> add_task_count;
		std::atomic<std::uint32_t> run_task_count;
		std::atomic<std::uint32_t> finish_task_count;
	protected:

		task_queue& queue_for_channel(const channel_type& cur_channel)
		{
			return _tasks_by_channel[std::hash<channel_type>()(cur_channel) % _tasks_by_channel.size()];
		}

	public:
		void add_task(task_ptr task, bool front = false)
		{
			auto cur_channel_id = task->channel_id();
			auto& cur_queue = queue_for_channel(cur_channel_id);
			if(front)
			{
				cur_queue._queue.push_front(task);
			}
			else
			{
				cur_queue._queue.push_back(task);
			}
			
		}
		task_channels()
		{

		}
		task_channels(const task_channels& other) = delete;
		task_channels& operator=(const task_channels& other) = delete;

		task_ptr poll_one_task(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			auto& result_queue = select_task_queue(prefer_channel);
			if(result_queue.executor_id == cur_executor_id)
			{
				task_ptr result;
				if(result_queue._queue.pop_front(result))
				{
					run_task_count++;
					return result;
				}
				else
				{
					result_queue.renounce(cur_executor_id);
				}
			}

			for(auto& one_queue: _tasks_by_channel)
			{
				if(one_queue.claim(cur_executor_id))
				{
					task_ptr result;
					if(one_queue._queue.pop_front(result))
					{
						run_task_count++;
						return result;
					}
					else
					{
						one_queue.renouce(cur_executor_id);
					}
				}
			}
			return {};
		}
	};
}