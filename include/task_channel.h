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
	template <typename T>
	class task_channels
	{
	public:
		using channel_type = typename T::channel_type;
		using task_ptr = std::shared_ptr<T>;
		struct task_queue
		{
			std::uint32_t executor_id = 0;
			std::deque<task_ptr> _queue;
		};
	protected:
		std::unordered_map<channel_type, std::shared_ptr<task_queue>> _tasks_by_channel;
		std::shared_ptr<task_queue> _tasks_without_channel;
		std::mutex _task_mutex;
		const channel_type default_channel_id;
		std::atomic<std::uint32_t> add_task_count;
		std::atomic<std::uint32_t> run_task_count;
		std::atomic<std::uint32_t> finish_task_count;
	protected:

		
		bool is_default_channel(const channel_type& channel_id) const
		{
			return channel_id == default_channel_id;
		}
	public:
		void add_task(task_ptr task, bool front = false)
		{
			auto cur_channel_id = task->channel_id();
			std::lock_guard<std::mutex> task_lock(_task_mutex);

			if(!is_default_channel(cur_channel_id))
			{
				auto cur_channel_iter = _tasks_by_channel.find(cur_channel_id);
				if(cur_channel_iter == _tasks_by_channel.end())
				{

					auto cur_task_channel = std::make_shared<task_queue>();
					cur_task_channel->_queue.push_back(task);
				}
				else
				{
					if(front)
					{
						cur_channel_iter->second->_queue.push_front(task);
					}
					else
					{
						cur_channel_iter->second->_queue.push_back(task);
					}
					
				}
			}
			else
			{
				_tasks_without_channel->_queue.push_back(task);
			}
			add_task_count++;
			if(add_task_count % 2000 == 0)
			{
				remove_empty_channels();
			}
			
		}
		task_channels()
		: _tasks_without_channel(std::make_shared<task_queue>())
		, default_channel_id()
		{

		}
		task_channels(const task_channels& other) = delete;
		task_channels& operator=(const task_channels& other) = delete;

		task_ptr poll_one_task(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			std::lock_guard<std::mutex> task_lock(_task_mutex);
			auto result_queue = select_task_queue(prefer_channel);
			if(!result_queue)
			{
				return {};
			}
			result_queue->executor_id = cur_executor_id;
			auto cur_task = result_queue->_queue.front();
			result_queue->_queue.pop_front();
			run_task_count++;
			return cur_task;
			
		}
		std::shared_ptr<task_queue> select_task_queue(channel_type prefer_channel)
		{
			std::shared_ptr<task_queue> result_queue;
			if(!is_default_channel(prefer_channel))
			{
				auto cur_channel_iter = _tasks_by_channel.find(prefer_channel);
				if(cur_channel_iter != _tasks_by_channel.end())
				{
					auto& cur_queue = cur_channel_iter->second;
					if(!(cur_queue->_queue.empty()))
					{
						return cur_queue;
					}
					else
					{
						cur_queue->executor_id = 0;
					}
					
				}
			}

			if(!_tasks_without_channel->_queue.empty())
			{
				return _tasks_without_channel;
			}
			for(auto& [k, v] : _tasks_by_channel)
			{
				if(v->executor_id || v->_queue.empty())
				{
					continue;
				}
				return v;
			}
			return {};
		}
		void remove_empty_channels()
		{
			auto iter_begin = _tasks_by_channel.begin();
			while(iter_begin != _tasks_by_channel.end())
			{
				auto cur_iter = iter_begin;
				iter_begin++;
				if(cur_iter->second->_queue.empty())
				{
					_tasks_by_channel.erase(cur_iter);
				}
			}
		}
		virtual void finish_task(task_ptr cur_task)
		{
			finish_task_count++;
		}

	};
}