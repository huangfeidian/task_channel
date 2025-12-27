#pragma once
#include <array>
#include <unordered_map>
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <functional>
namespace spiritsaway::concurrency
{
	// 假设T是一个任务类，包含channel_id()方法
	template <typename T, bool threading = true>
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

		static constexpr std::size_t HASH_BUCKET_COUNT = 32;
		static constexpr std::size_t HASH_MASK = HASH_BUCKET_COUNT - 1;

	protected:
		const channel_type m_default_channel_id;
		std::array<task_queue, HASH_BUCKET_COUNT> m_task_buckets;
		task_queue m_tasks_without_channel;
		mutable std::mutex m_task_mutex;

		std::atomic<std::size_t> m_add_task_count = 0;
		std::atomic<std::size_t> m_run_task_count = 0;
		std::atomic<std::size_t> m_finish_task_count = 0;

	protected:
		// 哈希函数，将channel散列到0-31范围
		std::size_t hash_channel(const channel_type &channel_id) const
		{
			// 使用std::hash进行哈希，然后与MASK取模得到0-31范围
			return std::hash<channel_type>()(channel_id) & HASH_MASK;
		}

		bool is_default_channel(const channel_type &channel_id) const
		{
			return channel_id == m_default_channel_id;
		}

		void add_task_impl(task_ptr task)
		{
			auto cur_channel_id = task->channel_id();

			if (!is_default_channel(cur_channel_id))
			{
				// 计算channel的哈希值，确定要使用的bucket
				std::size_t bucket_index = hash_channel(cur_channel_id);
				auto &task_queue = m_task_buckets[bucket_index];
				task_queue.queue.push_back(task);
			}
			else
			{
				m_tasks_without_channel.queue.push_back(task);
			}
			m_add_task_count++;
		}

		task_ptr poll_one_task_impl(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			auto result_queue = select_task_queue(prefer_channel, cur_executor_id);
			if (!result_queue)
			{
				return {};
			}

			// 设置当前执行器ID
			result_queue->executor_id = cur_executor_id;

			// 取出队列头部的任务
			auto cur_task = result_queue->queue.front();
			result_queue->queue.pop_front();
			m_run_task_count++;
			return cur_task;
		}

		task_queue *select_task_queue(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{

			// 1. 如果指定了非默认channel，优先检查对应的bucket
			if (!is_default_channel(prefer_channel))
			{
				std::size_t prefer_bucket = hash_channel(prefer_channel);
				auto &queue = m_task_buckets[prefer_bucket];
				if (!queue.queue.empty() && (queue.executor_id == 0 || queue.executor_id == cur_executor_id))
				{
					return &queue;
				}
			}

			// 2. 检查是否有默认channel的任务
			if (!m_tasks_without_channel.queue.empty())
			{
				return &m_tasks_without_channel;
			}

			// 3. 遍历所有bucket，找到一个非空且未被其他worker占用的队列
			// 每个worker会处理一个队列直到为空
			// 使用随机起点避免每次都从第一个bucket开始 减少饥饿概率
			auto random_start = m_run_task_count % HASH_BUCKET_COUNT;
			for (std::size_t i = 0; i < HASH_BUCKET_COUNT; ++i)
			{
				auto &backet = m_task_buckets[(random_start + i) % HASH_BUCKET_COUNT];
				if (!backet.queue.empty() && backet.executor_id == 0)
				{
					return &backet;
				}
			}

			return nullptr;
		}
		void finish_task_impl(task_ptr cur_task)
		{
			m_finish_task_count++;
			// 如果队列现在为空，重置executor_id
			if (!is_default_channel(cur_task->channel_id()))
			{
				std::size_t bucket_index = hash_channel(cur_task->channel_id());
				auto &task_queue = m_task_buckets[bucket_index];
				if (task_queue.queue.empty())
				{
					task_queue.executor_id = 0;
				}
			}
		}

	public:
		void add_task(task_ptr task)
		{
			if (threading)
			{
				std::lock_guard<std::mutex> task_lock(m_task_mutex);
				add_task_impl(task);
			}
			else
			{
				add_task_impl(task);
			}
		}

		task_channels()
			: m_default_channel_id()
		{
		}

		task_channels(const task_channels &other) = delete;
		task_channels &operator=(const task_channels &other) = delete;

		task_ptr poll_one_task(channel_type prefer_channel, std::uint32_t cur_executor_id)
		{
			if (cur_executor_id == 0)
			{
				// executor_id不能为0
				assert(false);
				return {};
			}
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

		virtual void finish_task(task_ptr cur_task)
		{
			if (threading)
			{
				std::lock_guard<std::mutex> task_lock(m_task_mutex);
				finish_task_impl(cur_task);
			}
			else
			{
				finish_task_impl(cur_task);
			}
		}

		bool tasks_all_finished() const
		{
			return m_finish_task_count == m_add_task_count;
		}

		// 添加一个方法用于检查特定bucket的状态（可选）
		bool bucket_empty(std::size_t bucket_index) const
		{

			if (bucket_index >= HASH_BUCKET_COUNT)
				return true;
			if (threading)
			{
				std::lock_guard<std::mutex> task_lock(m_task_mutex);
				return m_task_buckets[bucket_index].queue.empty();
			}
			else
			{
				return m_task_buckets[bucket_index].queue.empty();
			}
		}
	};
}