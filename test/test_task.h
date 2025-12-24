#pragma once
#include <string>

class TestTask
{
public:
	using channel_type = int;

	TestTask(int channel_id, int task_id, const std::string &data)
		: m_channel_id(channel_id), m_task_id(task_id), m_data(data)
	{
	}

	channel_type channel_id() const
	{
		return m_channel_id;
	}

	int task_id() const
	{
		return m_task_id;
	}

	const std::string &data() const
	{
		return m_data;
	}

private:
	const channel_type m_channel_id;
	const int m_task_id;
	const std::string m_data;
};
