#pragma once

#include "leaf/core/exception.hpp"
#include "leaf/core/format.hpp"

#include <vector>

namespace lf {
	template<typename T, typename Alloc = std::allocator<T>>
	class vector : public std::vector<T, Alloc> {
	  public:
		using std::vector<T, Alloc>::vector;
		using std::vector<T, Alloc>::operator=;
		using size_type = typename std::vector<T, Alloc>::size_type;

		T& at(size_type index) {
			if (index >= this->size()) {
				throw lf::out_of_range_exception(lf::format("index {} out of range (size is {})", index, this->size()));
			}
			return std::vector<T, Alloc>::operator[](index);
		}
		const T& at(size_type index) const {
			if (index >= this->size()) {
				throw lf::out_of_range_exception(lf::format("index {} out of range (size is {})", index, this->size()));
			}
			return std::vector<T, Alloc>::operator[](index);
		}
	};
} // namespace lf
