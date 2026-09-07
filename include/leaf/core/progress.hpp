#pragma once

#include "leaf/core/normalized.hpp"
#include "leaf/core/memory.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/vector.hpp"

#include <memory>
#include <stop_token>
#include <utility>

namespace lf {
	struct Progress {
		std::stop_token stop;

		explicit Progress(string_view label = {});
		Progress(const Progress&) = delete;
		Progress& operator=(const Progress&) = delete;
		Progress(Progress&& other) noexcept;
		Progress& operator=(Progress&& other) noexcept;
		~Progress();

		// Defines phase slots. Their weights determine the parent progress value.
		void add(string_view label, u64 weight = 1);

		// Creates the next live child. Its Progress handle owns the child node.
		Progress operator()();

		vector<Progress> split(string_view label, size_t count, u64 weight = 1);

		void add_total(u64 amount);
		void advance(u64 amount = 1);
		void set(normalized<u32> value);

		vector<string> path() const;

		f32 value() const;
		f32 active_value() const;
		string_view label() const;
		bool valid() const;
		bool cancelled() const;

	  private:
		struct Shared;
		struct Node;

		std::shared_ptr<Shared> shared;
		unique_ptr<Node> owned;
		Node* node = nullptr;
		Progress(std::shared_ptr<Shared> shared, unique_ptr<Node> owned, Node* node, std::stop_token stop);

		static f32 value_locked(const Node& value_node);
	};
} // namespace lf
