#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/format.hpp>
#include <leaf/core/memory.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>
#include <leaf/core/vector.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <source_location>
#include <thread>
#include <utility>

namespace lf::log {

	enum class Level {
		Trace,
		Debug,
		Info,
		Warning,
		Error,
		FATAL,
		Assert,
		None,
	};

	struct Text {
		string_view value;
		std::source_location location;

		template<std::size_t N>
		consteval Text(const char (&v)[N], std::source_location loc = std::source_location::current())
			: value(v, N - 1), location(loc) {}
	};

	struct Record {
		Level level;
		string message;
		std::source_location location;
		u64 sequence;
		std::chrono::system_clock::time_point timestamp;
	};

	struct Sink {
		virtual ~Sink() = default;
		virtual void write(const Record&, string_view line) = 0;
		virtual void flush() {}
	};

	struct ConsoleSink : Sink {
		void write(const Record&, string_view line) override;
	};

	struct Logger {
		static Logger& instance();

		void set_minimum_level(Level level);
		Level get_minimum_level();

		void add_sink(unique_ptr<Sink> sink);
		void clear_sinks();

		void flush();
		void write(Record record);

	  private:
		Logger();
		~Logger();
		void worker_loop(std::stop_token token);
		void flush_sinks();

		struct Item {
			Record record;
			bool flush;
		};

		std::mutex mutex;
		std::condition_variable wake;
		std::condition_variable drained;

		std::deque<Item> queue;
		vector<unique_ptr<Sink>> sinks;

		std::jthread worker;

		std::atomic<u64> sequence = 0;
		Level min_level = Level::Info;

		bool stopping = false;
		u64 pending = 0;
	};

	void write(Record record);

	// =========================
	// FREE FUNCTION API
	// =========================

	template<typename... Args>
	void log(Level level, Text text, Args&&... args) {
		string message;

		if constexpr (sizeof...(args) == 0) {
			message = string(text.value);
		} else {
			message = lf::format(text.value, std::forward<Args>(args)...);
		}

		Logger::instance().write({ level,
								   std::move(message),
								   text.location,
								   0,
								   std::chrono::system_clock::now() });
	}

	template<typename... Args>
	void Trace(Text t, Args&&... a) {
		log(Level::Trace, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void Debug(Text t, Args&&... a) {
		log(Level::Debug, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void Info(Text t, Args&&... a) {
		log(Level::Info, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void Warning(Text t, Args&&... a) {
		log(Level::Warning, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void Error(Text t, Args&&... a) {
		log(Level::Error, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void FATAL(Text t, Args&&... a) {
		log(Level::FATAL, t, std::forward<Args>(a)...);
	}
	template<typename... Args>
	void Assert(Text t, Args&&... a) {
		log(Level::Assert, t, std::forward<Args>(a)...);
	}

} // namespace lf::log
