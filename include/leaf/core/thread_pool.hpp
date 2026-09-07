#pragma once

#include "leaf/core/types.hpp"

#include "leaf/core/vector.hpp"
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace lf {
	class ThreadPool {
	  public:
		explicit ThreadPool(size_t worker_count) {
			workers.reserve(worker_count);
			for (size_t index = 0; index < worker_count; ++index) {
				workers.emplace_back([this](std::stop_token stop) {
					while (!stop.stop_requested()) {
						std::function<void()> task;
						{
							std::unique_lock lock{ mutex };
							condition.wait(lock, [this, &stop] {
								return stop.stop_requested() || stopping || !tasks.empty();
							});
							if (stop.stop_requested() || (stopping && tasks.empty())) {
								return;
							}
							task = std::move(tasks.front());
							tasks.pop();
						}
						task();
					}
				});
			}
		}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		~ThreadPool() {
			{
				std::scoped_lock lock{ mutex };
				stopping = true;
			}
			condition.notify_all();
			for (auto& worker : workers) {
				worker.join();
			}
		}

		template<typename Function>
		auto submit(Function function) -> std::future<std::invoke_result_t<Function>> {
			using Result = std::invoke_result_t<Function>;
			auto task = std::make_shared<std::packaged_task<Result()>>(std::move(function));
			std::future<Result> result = task->get_future();
			{
				std::scoped_lock lock{ mutex };
				if (stopping) {
					throw std::runtime_error("cannot submit work to a stopped thread pool");
				}
				tasks.emplace([task] { (*task)(); });
			}
			condition.notify_one();
			return result;
		}

	  private:
		lf::vector<std::jthread> workers;
		std::mutex mutex;
		std::condition_variable condition;
		std::queue<std::function<void()>> tasks;
		bool stopping = false;
	};
} // namespace lf

