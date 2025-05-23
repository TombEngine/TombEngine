#pragma once

namespace TEN::Utils
{
	using ParallelTask	= std::function<void()>;
	using ParallelTasks = std::vector<ParallelTask>;

	class ParallelTaskManager
	{
	private:
		// Fields

		std::vector<std::thread> _threads	   = {};
		std::queue<ParallelTask> _tasks		   = {};
		std::mutex				 _taskMutex	   = {};
		std::condition_variable	 _taskCond	   = {};
		bool					 _deinitialize = false;

		// Constructors, destructors

		ParallelTaskManager();
		ParallelTaskManager(const ParallelTaskManager& manager) = delete;
		~ParallelTaskManager();

	public:
		// Getters

		static ParallelTaskManager& Get();

		unsigned int GetThreadCount() const;
		unsigned int GetCoreCount() const;

		// Utilities

		void Initialize();

		std::future<void> AddTask(const ParallelTask& task);
		std::future<void> AddTasks(const ParallelTasks& tasks);
		std::future<void> AddTasks(int elementCount, const std::function<void(int, int)>& splitTask);

	private:
		// Helpers

		void Worker();
		void AddTask(const ParallelTask& task, std::shared_ptr<std::atomic<int>> counter, std::shared_ptr<std::promise<void>> promise);
		void HandleTask(const ParallelTask& task, std::atomic<int>& counter, std::promise<void>& promise);

		// Operators

		ParallelTaskManager& operator =(const ParallelTaskManager& manager) = delete;
	};

	extern ParallelTaskManager& g_Parallel;
}
