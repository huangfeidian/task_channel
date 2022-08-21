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
	template <typename T, bool threading = false>
	class task_channels
	{
	public:
		using channel_type = typename T::channel_type;
		using task_ptr = std::shared_ptr<T>;
		struct task_queue
		{
			std::uint32_t executor_id = 0;
			std::deque<task_ptr> queue;
		};
	protected:
		std::unordered_map<channel_type, std::shared_ptr<task_queue>> m_tasks_by_channel;
		std::shared_ptr<task_queue> m_tasks_without_channel;
		std::mutex m_task_mutex;
		const channel_type m_default_channel_id;
		std::atomic<std::size_t> m_add_task_count = 0;
		std::atomic<std::size_t> m_run_task_count = 0;
		std::atomic<std::size_t> m_finish_task_count = 0;
	protected:

		
		bool is_default_channel(const channel_type& channel_id) const
		{
			return channel_id == m_default_channel_id;
		}
		void add_task_impl(task_ptr task, bool front)
		{
			auto cur_channel_id = task->channel_id();

			if (!is_default_channel(cur_channel_id))
			{

				auto cur_channel_iter = m_tasks_by_channel.find(cur_channel_id);
				if (cur_channel_iter == m_tasks_by_channel.end())
				{

					auto cur_task_channel = std::make_shared<task_queue>();
					cur_task_channel->queue.push_back(task);
				}
				else
				{
					if (front)
					{
						cur_channel_iter->second->queue.push_front(task);
					}
					else
					{
						cur_channel_iter->second->queue.push_back(task);
					}

				}
			}
			else
			{
				m_tasks_without_channel->queue.push_back(task);
			}
			m_add_task_count++;

			if (m_add_task_count % 2000 == 0)
			{
				remove_empty_channels();
			}
		}
		
		task_ptr poll_one_task_impl(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			auto result_queue = select_task_queue(prefer_channel);
			if (!result_queue)
			{
				return {};
			}
			result_queue->executor_id = cur_executor_id;
			auto cur_task = result_queue->queue.front();
			result_queue->queue.pop_front();
			m_run_task_count++;
			return cur_task;
		}
		std::shared_ptr<task_queue> select_task_queue(channel_type prefer_channel)
		{
			std::shared_ptr<task_queue> result_queue;
			if (!is_default_channel(prefer_channel))
			{
				auto cur_channel_iter = m_tasks_by_channel.find(prefer_channel);
				if (cur_channel_iter != m_tasks_by_channel.end())
				{
					auto& cur_queue = cur_channel_iter->second;
					if (!(cur_queue->queue.empty()))
					{
						return cur_queue;
					}
					else
					{
						cur_queue->executor_id = 0;
					}

				}
			}

			if (!m_tasks_without_channel->queue.empty())
			{
				return m_tasks_without_channel;
			}
			for (auto& [k, v] : m_tasks_by_channel)
			{
				if (v->executor_id || v->queue.empty())
				{
					continue;
				}
				return v;
			}
			return {};
		}
	public:
		void add_task(task_ptr task, bool front = false)
		{
			if (threading)
			{
				std::lock_guard<std::mutex> task_lock(m_task_mutex);
				add_task_impl(task, front);
			}
			else
			{
				add_task_impl(task, front);
			}
			
		}
		task_channels()
		: m_tasks_without_channel(std::make_shared<task_queue>())
		, m_default_channel_id()
		{

		}
		task_channels(const task_channels& other) = delete;
		task_channels& operator=(const task_channels& other) = delete;

		task_ptr poll_one_task(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			if (threading)
			{
				std::lock_guard<std::mutex> task_lock(m_task_mutex);
				return poll_one_task_impl(prefer_channel, cur_executor_id);
			}
			else
			{
				return poll_one_task_impl(prefer_channel, cur_executor_id);
			}
			
			
		}
		std::vector<task_ptr> dump_tasks()
		{
			std::vector<task_ptr> result;

			if (threading)
			{
				std::lock_guard<std::mutex> task_lock = std::lock_guard<std::mutex>(m_task_mutex);
				while (!m_tasks_without_channel->queue.empty())
				{
					result.push_back(m_tasks_without_channel->queue.front());
					m_tasks_without_channel->queue.pop_front();
				}
				for (auto& one_pair : m_tasks_by_channel)
				{
					while (!one_pair.second->queue.empty())
					{
						result.push_back(one_pair.second->queue.front());
						one_pair.second->queue.pop_front();
					}
				}
				return result;
			}
			else
			{
				while (!m_tasks_without_channel->queue.empty())
				{
					result.push_back(m_tasks_without_channel->queue.front());
					m_tasks_without_channel->queue.pop_front();
				}
				for (auto& one_pair : m_tasks_by_channel)
				{
					while (!one_pair.second->queue.empty())
					{
						result.push_back(one_pair.second->queue.front());
						one_pair.second->queue.pop_front();
					}
				}
				return result;
			}
			
		}
		
		void remove_empty_channels()
		{
			auto iter_begin = m_tasks_by_channel.begin();
			while(iter_begin != m_tasks_by_channel.end())
			{
				auto cur_iter = iter_begin;
				iter_begin++;
				if(cur_iter->second->queue.empty())
				{
					m_tasks_by_channel.erase(cur_iter);
				}
			}
		}
		virtual void finish_task(task_ptr cur_task)
		{
			m_finish_task_count++;
		}
		bool tasks_all_finished() const
		{
			return m_finish_task_count == m_add_task_count;
		}

	};
}