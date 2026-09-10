#include "leaf/core/time.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/scope.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#endif

namespace lf {
	instant now() {
#if defined(_WIN32)
		static const i64 frequency = [] {
			LARGE_INTEGER value;
			if (!QueryPerformanceFrequency(&value)) {
				throw runtime_exception("QueryPerformanceFrequency failed");
			}
			return value.QuadPart;
		}();
		LARGE_INTEGER counter;
		if (!QueryPerformanceCounter(&counter)) {
			throw runtime_exception("QueryPerformanceCounter failed");
		}
		const i64 seconds = counter.QuadPart / frequency;
		const i64 remainder = counter.QuadPart % frequency;
		return instant::from_quantum(seconds * 1'000'000'000 + static_cast<i64>(static_cast<long double>(remainder) * 1'000'000'000 / frequency));
#else
		timespec value{};
		if (clock_gettime(CLOCK_MONOTONIC, &value)) {
			throw runtime_exception("clock_gettime failed");
		}
		return instant::from_quantum(static_cast<i64>(value.tv_sec) * 1'000'000'000 + value.tv_nsec);
#endif
	}

	timepoint wall_now() {
#if defined(_WIN32)
		FILETIME value;
		GetSystemTimePreciseAsFileTime(&value);
		ULARGE_INTEGER ticks;
		ticks.LowPart = value.dwLowDateTime;
		ticks.HighPart = value.dwHighDateTime;
		return timepoint::from_unix_epoch(duration::from_quantum((static_cast<i64>(ticks.QuadPart) - 116'444'736'000'000'000) * 100));
#else
		timespec value{};
		if (clock_gettime(CLOCK_REALTIME, &value)) {
			throw runtime_exception("clock_gettime failed");
		}
		return timepoint::from_unix_epoch(duration::from_quantum(static_cast<i64>(value.tv_sec) * 1'000'000'000 + value.tv_nsec));
#endif
	}

	duration frequency::period() const {
		if (value.raw() <= 0) {
			throw invalid_argument_exception("frequency must be positive to have a period");
		}
		return duration::from_quantum(1'000'000'000 * fixed::scale / value.raw());
	}

	void sleep_for(duration duration) {
		if (duration.quantum_count() <= 0) {
			return;
		}
		sleep_until(now() + duration);
	}

	void sleep_until(instant deadline) {
		auto remaining = deadline - now();
		if (remaining.quantum_count() <= 0) {
			return;
		}
#if defined(_WIN32)
		// Reuse one native timer per sleeping thread; release it when the thread exits.
		thread_local const HANDLE timer = [] {
			HANDLE value = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
			if (!value && GetLastError() == ERROR_INVALID_PARAMETER) {
				value = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
			}
			if (!value) {
				throw runtime_exception("CreateWaitableTimerExW failed");
			}
			return value;
		}();
		thread_local const scope_exit release_timer{ [] {
			CloseHandle(timer);
		} };
		while (remaining.quantum_count() > 0) {
			LARGE_INTEGER due;
			const i64 nanoseconds = remaining.quantum_count();
			due.QuadPart = -(nanoseconds / 100 + (nanoseconds % 100 != 0));
			if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
				throw runtime_exception("SetWaitableTimer failed");
			}
			if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
				throw runtime_exception("WaitForSingleObject failed");
			}
			remaining = deadline - now();
		}
#else
		while (remaining.quantum_count() > 0) {
			timespec request{ static_cast<time_t>(remaining.quantum_count() / 1'000'000'000), static_cast<long>(remaining.quantum_count() % 1'000'000'000) };
			if (nanosleep(&request, nullptr) && errno != EINTR) {
				throw runtime_exception("nanosleep failed");
			}
			remaining = deadline - now();
		}
#endif
	}

	string pretty_string_trait<duration>::to_string(const duration& value) {
		i64 rem = value.quantum_count();
		string result;
		for (const auto& u : duration_units) {
			i64 n = rem / u.first;
			if (n > 0) {
				if (!result.empty()) {
					result += " ";
				}
				result += lf::format("{}{}", n, u.second);
				rem %= u.first;
			}
		}
		if (result.empty()) {
			result = "0s";
		}
		return result;
	}
	duration pretty_string_trait<duration>::from_string(string_view str) {
		auto p = string_parser{ str };
		i64 total_ns = 0;

		while (!p.eof()) {
			p.skip_whitespace();
			if (p.eof()) {
				break;
			}

			i64 value = p.parse_int();
			p.skip_whitespace();

			string_view unit = p.parse_alpha();

			bool matched = false;
			for (const auto& u : duration_units) {
				if (u.second == unit) {
					total_ns += value * u.first;
					matched = true;
					break;
				}
			}

			if (!matched) {
				throw std::runtime_error(lf::format("unknown time unit '{}' in '{}'", unit, str));
			}
		}

		return duration::from_quantum(total_ns);
	}
} // namespace lf
