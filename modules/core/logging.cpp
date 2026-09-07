#include "leaf/core/logging.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace lf::log {

	string_view console_color(Level level) {
		switch (level) {
		case Level::Trace: return "\x1b[90m";
		case Level::Debug: return "\x1b[37m";
		case Level::Info: return "\x1b[97m";
		case Level::Warning: return "\x1b[33m";
		case Level::Error: return "\x1b[31m";
		case Level::FATAL: return "\x1b[35m";
		case Level::Assert: return "\x1b[36m";
		case Level::None: return "";
		}
		return "";
	}
	static string_view level_name(Level level) {
		switch (level) {
		case Level::Trace: return "TRACE";
		case Level::Debug: return "DEBUG";
		case Level::Info: return "INFO";
		case Level::Warning: return "WARN";
		case Level::Error: return "ERROR";
		case Level::FATAL: return "FATAL";
		case Level::Assert: return "ASSRT";
		case Level::None: return "None";
		}
		return "";
	}

	static string render_timestamp(std::chrono::system_clock::time_point tp) {
		const std::time_t t = std::chrono::system_clock::to_time_t(tp);

		std::tm tm{};
#if defined(_WIN32)
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif

		const auto ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				tp.time_since_epoch()
			)
				.count() %
			1000;

		std::ostringstream out;
		out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		out << '.' << std::setw(3) << std::setfill('0') << ms;
		return out.str();
	}

	static string render_line(const Record& r) {
		string line = "[";
		line += render_timestamp(r.timestamp);
		line += "] [";
		line += level_name(r.level);
		line += "] ";
		line += r.message;
		return line;
	}

	Logger& Logger::instance() {
		static Logger instance;
		return instance;
	}

	Logger::Logger()
		: worker([this](std::stop_token st) {
			  this->worker_loop(st);
		  }) {}
	Logger::~Logger() {
		flush();

		{
			std::scoped_lock lock(mutex);
			stopping = true;
		}

		wake.notify_all();

		if (worker.joinable()) {
			worker.join();
		}
	}

	void Logger::set_minimum_level(Level level) {
		std::scoped_lock lock(mutex);
		min_level = level;
	}

	Level Logger::get_minimum_level() {
		std::scoped_lock lock(mutex);
		return min_level;
	}

	void Logger::write(Record record) {
		{
			std::scoped_lock lock(mutex);

			if (record.level == Level::None ||
				min_level == Level::None ||
				static_cast<int>(record.level) < static_cast<int>(min_level)) {
				return;
			}

			record.sequence = sequence.fetch_add(1, std::memory_order_relaxed);
			record.timestamp = std::chrono::system_clock::now();

			queue.push_back({ std::move(record), false });
			++pending;
		}

		wake.notify_one();
	}

	void write(Record record) {
		Logger::instance().write(std::move(record));
	}

	void Logger::flush() {
		std::unique_lock lock(mutex);

		queue.push_back({ {}, true });
		wake.notify_one();

		drained.wait(lock, [&] {
			return pending == 0 && queue.empty();
		});
	}

	void Logger::add_sink(unique_ptr<Sink> sink) {
		if (!sink) throw runtime_exception("NULL sink");

		std::scoped_lock lock(mutex);
		sinks.emplace_back(std::move(sink));
	}

	void Logger::clear_sinks() {
		flush();
		std::scoped_lock lock(mutex);
		sinks.clear();
	}

	void Logger::flush_sinks() {
		for (auto& s : sinks) {
			s->flush();
		}
	}

	void Logger::worker_loop(std::stop_token token) {
		while (!token.stop_requested()) {

			Item item;

			{
				std::unique_lock lock(mutex);

				wake.wait(lock, [&] {
					return stopping || !queue.empty();
				});

				if (queue.empty() && stopping) {
					flush_sinks();
					drained.notify_all();
					return;
				}

				item = std::move(queue.front());
				queue.pop_front();
			}

			if (item.flush) {
				flush_sinks();
				drained.notify_all();
				continue;
			}

			const string line = ::lf::log::render_line(item.record);

			for (auto& s : sinks) {
				s->write(item.record, line);
			}

			{
				std::scoped_lock lock(mutex);
				if (pending > 0) --pending;
				if (pending == 0 && queue.empty()) {
					drained.notify_all();
				}
			}
		}
	}

	void ConsoleSink::write(const Record& record, string_view line) {
		std::ostream& out =
			static_cast<int>(record.level) >= static_cast<int>(Level::Warning)
				? std::cerr
				: std::cout;

		out << console_color(record.level) << line << "\x1b[0m\n";
	}

} // namespace lf::log
