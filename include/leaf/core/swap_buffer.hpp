#pragma once

#include <atomic>
#include <mutex>
#include <utility>

namespace lf {
	template<typename T>
	struct Swapbuffer {
		class FrontAccess;
		class BackAccess;

		FrontAccess front() const;
		BackAccess back();
		void swap();

	  private:
		mutable std::mutex mutex;
		T a;
		T b;
		std::atomic<bool> a_is_front{};
	};

	template<typename T>
	class Swapbuffer<T>::FrontAccess {
	  public:
		FrontAccess(const T& value, std::unique_lock<std::mutex> lock);
		operator const T&() const;
		const T& operator*() const;
		const T* operator->() const;

	  private:
		const T& value;
		std::unique_lock<std::mutex> lock;
	};

	template<typename T>
	class Swapbuffer<T>::BackAccess {
	  public:
		BackAccess(T& value, std::unique_lock<std::mutex> lock);
		operator T&();
		T& operator*();
		T* operator->();

	  private:
		T& value;
		std::unique_lock<std::mutex> lock;
	};

	template<typename T>
	Swapbuffer<T>::FrontAccess::FrontAccess(const T& value, std::unique_lock<std::mutex> lock) : value{ value }, lock{ std::move(lock) } {}

	template<typename T>
	Swapbuffer<T>::FrontAccess::operator const T&() const {
		return value;
	}

	template<typename T>
	const T& Swapbuffer<T>::FrontAccess::operator*() const {
		return value;
	}

	template<typename T>
	const T* Swapbuffer<T>::FrontAccess::operator->() const {
		return &value;
	}

	template<typename T>
	Swapbuffer<T>::BackAccess::BackAccess(T& value, std::unique_lock<std::mutex> lock) : value{ value }, lock{ std::move(lock) } {}

	template<typename T>
	Swapbuffer<T>::BackAccess::operator T&() {
		return value;
	}

	template<typename T>
	T& Swapbuffer<T>::BackAccess::operator*() {
		return value;
	}

	template<typename T>
	T* Swapbuffer<T>::BackAccess::operator->() {
		return &value;
	}

	template<typename T>
	typename Swapbuffer<T>::FrontAccess Swapbuffer<T>::front() const {
		for (;;) {
			const bool a_is_front = this->a_is_front.load();
			std::unique_lock lock{ mutex };
			if (this->a_is_front.load() == a_is_front) {
				return FrontAccess{ a_is_front ? a : b, std::move(lock) };
			}
		}
	}

	template<typename T>
	typename Swapbuffer<T>::BackAccess Swapbuffer<T>::back() {
		for (;;) {
			const bool a_is_front = this->a_is_front.load();
			std::unique_lock lock{ mutex };
			if (this->a_is_front.load() == a_is_front) {
				return BackAccess{ a_is_front ? b : a, std::move(lock) };
			}
		}
	}

	template<typename T>
	void Swapbuffer<T>::swap() {
		std::unique_lock lock{ mutex };
		const bool next_a_is_front = !a_is_front.load();
		a_is_front.store(next_a_is_front);
		T& back = next_a_is_front ? b : a;
		if constexpr (requires(T& value) { value.clear(); }) {
			back.clear();
		} else {
			back = T{};
		}
	}
} // namespace lf
