#include "leaf/core/progress.hpp"

#include <algorithm>
#include <mutex>
#include <stdexcept>

namespace lf {
	struct Progress::Shared {
		std::mutex mutex;
	};

	struct Progress::Node {
		struct Phase {
			string label;
			u64 weight;
		};

		Shared* shared = nullptr;
		string label;
		Node* parent = nullptr;
		vector<Phase> phases;
		vector<Node*> active;
		size_t next_phase = 0;
		u64 completed_weight = 0;
		u64 parent_weight = 0;
		u64 work_total = 0;
		u64 work_done = 0;
		f32 explicit_value = 0.0f;

		~Node();
		void child_finished(Node* child);
	};

	Progress::Node::~Node() {
		if (parent) {
			std::scoped_lock lock(shared->mutex);
			parent->child_finished(this);
		}
	}

	void Progress::Node::child_finished(Node* child) {
		auto it = std::find(active.begin(), active.end(), child);
		if (it != active.end()) {
			active.erase(it);
			completed_weight += child->parent_weight;
		}
	}

	Progress::Progress(string_view label) : shared(std::make_shared<Shared>()), owned(make_unique<Node>()) {
		owned->shared = shared.get();
		owned->label = string(label);
		node = owned.get();
	}

	Progress::Progress(Progress&& other) noexcept
		: stop(other.stop), shared(std::move(other.shared)), owned(std::move(other.owned)), node(other.node) {
		other.stop = {};
		other.node = nullptr;
	}

	Progress& Progress::operator=(Progress&& other) noexcept {
		if (this != &other) {
			owned.reset();
			stop = other.stop;
			shared = std::move(other.shared);
			owned = std::move(other.owned);
			node = other.node;
			other.stop = {};
			other.node = nullptr;
		}
		return *this;
	}

	Progress::~Progress() = default;

	void Progress::add(string_view label, u64 weight) {
		if (!node) {
			throw std::logic_error("lf::Progress: invalid handle");
		}
		if (weight == 0) {
			throw std::invalid_argument("lf::Progress: phase weight must be non-zero");
		}
		std::scoped_lock lock(shared->mutex);
		node->phases.push_back({ string(label), weight });
	}

	Progress Progress::operator()() {
		if (!node) {
			throw std::logic_error("lf::Progress: invalid handle");
		}
		std::scoped_lock lock(shared->mutex);
		if (node->next_phase == node->phases.size()) {
			throw std::logic_error("lf::Progress: no phase remains");
		}
		const Node::Phase& phase = node->phases[node->next_phase++];
		auto child = make_unique<Node>();
		child->shared = shared.get();
		child->label = phase.label;
		child->parent = node;
		child->parent_weight = phase.weight;
		Node* child_ptr = child.get();
		node->active.push_back(child_ptr);
		return Progress(shared, std::move(child), child_ptr, stop);
	}

	vector<Progress> Progress::split(string_view label, size_t count, u64 weight) {
		vector<Progress> result;
		result.reserve(count);
		for (size_t index = 0; index < count; ++index) {
			add(label, weight);
		}
		for (size_t index = 0; index < count; ++index) {
			result.emplace_back((*this)());
		}
		return result;
	}

	void Progress::add_total(u64 amount) {
		if (!node) {
			throw std::logic_error("lf::Progress: invalid handle");
		}
		std::scoped_lock lock(shared->mutex);
		node->work_total += amount;
		node->explicit_value = node->work_total == 0 ? 0.0f : static_cast<f32>(node->work_done) / static_cast<f32>(node->work_total);
	}

	void Progress::advance(u64 amount) {
		if (!node) {
			throw std::logic_error("lf::Progress: invalid handle");
		}
		std::scoped_lock lock(shared->mutex);
		node->work_done += amount;
		node->explicit_value = node->work_total == 0 ? 0.0f : static_cast<f32>(node->work_done) / static_cast<f32>(node->work_total);
	}

	void Progress::set(normalized<u32> value) {
		if (!node) {
			throw std::logic_error("lf::Progress: invalid handle");
		}
		std::scoped_lock lock(shared->mutex);
		node->explicit_value = static_cast<f32>(value);
	}

	vector<string> Progress::path() const {
		if (!node) {
			return {};
		}
		std::scoped_lock lock(shared->mutex);
		vector<string> result;
		const Node* current = node;
		while (current) {
			if (!current->label.empty()) {
				result.emplace_back(current->label);
			}
			if (current->active.empty()) {
				break;
			}
			current = current->active.back();
		}
		return result;
	}

	f32 Progress::value() const {
		if (!node) {
			return 0.0f;
		}
		std::scoped_lock lock(shared->mutex);
		return value_locked(*node);
	}

	f32 Progress::active_value() const {
		if (!node) {
			return 0.0f;
		}
		std::scoped_lock lock(shared->mutex);
		return value_locked(node->active.empty() ? *node : *node->active.back());
	}

	string_view Progress::label() const {
		return node ? string_view(node->label) : string_view{};
	}

	bool Progress::valid() const {
		return node != nullptr;
	}

	bool Progress::cancelled() const {
		return stop.stop_requested();
	}

	Progress::Progress(std::shared_ptr<Shared> shared, unique_ptr<Node> owned, Node* node, std::stop_token stop)
		: stop(stop), shared(std::move(shared)), owned(std::move(owned)), node(node) {}

	f32 Progress::value_locked(const Node& value_node) {
		if (value_node.phases.empty()) {
			return value_node.explicit_value;
		}
		long double completed = value_node.completed_weight;
		long double total = 0.0;
		for (const Node::Phase& phase : value_node.phases) {
			total += phase.weight;
		}
		for (const Node* child : value_node.active) {
			completed += static_cast<long double>(child->parent_weight) * value_locked(*child);
		}
		return static_cast<f32>(completed / total);
	}
} // namespace lf
