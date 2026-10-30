#include <leaf/progression/progression.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace lf {
	f64 progression_number(const StatNumber& number) {
		return std::visit([](auto value) { return static_cast<f64>(value); }, number);
	}

	f64 progression_value(const StatValue& number) {
		if (const auto* value = std::get_if<AverageRate>(&number)) {
			return value->value;
		}
		if (const auto* value = std::get_if<i64>(&number)) {
			return static_cast<f64>(*value);
		}
		return std::get<f64>(number);
	}

	string_view StatPrototype::type() noexcept {
		return "stat";
	}

	string_view AchievementPrototype::type() noexcept {
		return "achievement";
	}

	StatPrototype::StatPrototype(const dict& data)
		: Prototype(data) {
		data.assign(field("value_type", value_type, value_type), field("store_id", store_id, store_id), field("increment_only", increment_only, false), field("window_seconds", window_seconds, 3600.0), field("min_value", min_value, std::optional<f64>{}), field("max_value", max_value, std::optional<f64>{}));
		if (value_type != "integer" && value_type != "float" && value_type != "average_rate") {
			throw runtime_exception("invalid stat value_type");
		}
		if (value_type == "integer") {
			i64 initial = 0;
			data.assign(field("default_value", initial, i64(0)));
			default_value = initial;
		}
		else {
			f64 initial = 0;
			data.assign(field("default_value", initial, 0.0));
			default_value = initial;
		}
		const f64 initial = progression_number(default_value);
		if (!std::isfinite(initial) || !std::isfinite(window_seconds) || window_seconds <= 0 || (min_value && !std::isfinite(*min_value)) || (max_value && !std::isfinite(*max_value)) || (min_value && max_value && *min_value > *max_value) || (min_value && initial < *min_value) || (max_value && initial > *max_value)) {
			throw runtime_exception("invalid stat constraints/default");
		}
		if (!store_id.empty() && value_type == "integer" && (std::get<i64>(default_value) < std::numeric_limits<i32>::min() || std::get<i64>(default_value) > std::numeric_limits<i32>::max())) {
			throw runtime_exception("store integer default exceeds 32 bits");
		}
	}

	AchievementPrototype::AchievementPrototype(const dict& data)
		: Prototype(data) {
		data.assign(field("store_id", store_id, store_id), field("conditions", conditions, conditions));
	}

	Progression::Progression() {
		register_condition("stat_threshold", [](const dict& condition, const Progression& state) -> report<ConditionResult> {
			const string id = condition.parse_field<string>("stat");
			const auto* stat = state.find_stat(id);
			if (!stat) {
				return unexpected(error(generic_errc::not_found, "achievement references missing stat"));
			}
			const object threshold = condition.at("threshold");
			if (const auto* integer = std::get_if<i64>(&stat->value); integer && threshold.is<i64>()) {
				const i64 target = threshold.as<i64>();
				return ConditionResult{ *integer >= target, target > 0 ? std::clamp(static_cast<f64>(*integer) / static_cast<f64>(target), 0.0, 1.0) : (*integer >= target ? 1.0 : 0.0) };
			}
			const f64 target = threshold.as<f64>();
			if (!std::isfinite(target)) {
				return unexpected(error(generic_errc::invalid_argument, "nonfinite achievement threshold"));
			}
			const f64 value = progression_value(stat->value);
			return ConditionResult{ value >= target, target > 0 ? std::clamp(value / target, 0.0, 1.0) : (value >= target ? 1.0 : 0.0) };
		});
	}

	error Progression::register_condition(string id, ConditionEvaluator evaluator) {
		if (id.empty() || !evaluator) {
			return error(generic_errc::invalid_argument, "invalid condition evaluator");
		}
		for (const auto& entry : evaluators_) {
			if (entry.first == id) {
				return error(generic_errc::conflict, "duplicate condition evaluator");
			}
		}
		evaluators_.emplace_back(std::move(id), std::move(evaluator));
		return {};
	}

	error Progression::initialize() {
		Progression candidate = *this;
		candidate.stats_.clear();
		candidate.achievements_.clear();
		std::unordered_set<string> stat_bindings;
		std::unordered_set<string> achievement_bindings;
		for (const auto& proto : Database<StatPrototype>::prototypes) {
			if (!proto.store_id.empty() && !stat_bindings.insert(proto.store_id).second) {
				return error(generic_errc::conflict, "duplicate stat store binding");
			}
			StatValue value;
			if (proto.value_type == "integer") {
				value = std::get<i64>(proto.default_value);
			}
			else if (proto.value_type == "float") {
				value = std::get<f64>(proto.default_value);
			}
			else {
				value = AverageRate{ std::get<f64>(proto.default_value), {} };
			}
			candidate.stats_.push_back(Stat{ string(Database<StatPrototype>::name(proto.id)), proto, std::move(value), 0, 0, {} });
		}
		for (const auto& proto : Database<AchievementPrototype>::prototypes) {
			if (!proto.store_id.empty() && !achievement_bindings.insert(proto.store_id).second) {
				return error(generic_errc::conflict, "duplicate achievement store binding");
			}
			candidate.achievements_.push_back(Achievement{ string(Database<AchievementPrototype>::name(proto.id)), proto });
		}
		IF_ERROR_RETURN_ERROR(candidate.evaluate());
		*this = std::move(candidate);
		return {};
	}

	const vector<Stat>& Progression::stats() const noexcept {
		return stats_;
	}

	const vector<Achievement>& Progression::achievements() const noexcept {
		return achievements_;
	}

	const Stat* Progression::find_stat(string_view id) const noexcept {
		for (const auto& stat : stats_) {
			if (stat.id == id) {
				return &stat;
			}
		}
		return nullptr;
	}

	const Achievement* Progression::find_achievement(string_view id) const noexcept {
		for (const auto& achievement : achievements_) {
			if (achievement.id == id) {
				return &achievement;
			}
		}
		return nullptr;
	}

	error progression_validate(const Stat& stat, const StatValue& value) {
		const f64 numeric = progression_value(value);
		if (!std::isfinite(numeric) || (stat.prototype.min_value && numeric < *stat.prototype.min_value) || (stat.prototype.max_value && numeric > *stat.prototype.max_value)) {
			return error(generic_errc::out_of_range, "stat value violates constraints");
		}
		if (stat.prototype.increment_only && ((std::holds_alternative<i64>(value) && std::holds_alternative<i64>(stat.value)) ? std::get<i64>(value) < std::get<i64>(stat.value) : numeric < progression_value(stat.value))) {
			return error(generic_errc::out_of_range, "stat is increment-only");
		}
		if (!stat.prototype.store_id.empty() && std::holds_alternative<i64>(value) && (std::get<i64>(value) < std::numeric_limits<i32>::min() || std::get<i64>(value) > std::numeric_limits<i32>::max())) {
			return error(generic_errc::out_of_range, "store integer exceeds 32 bits");
		}
		return {};
	}

	error Progression::set_stat(string_view id, StatNumber value) {
		for (auto& stat : stats_) {
			if (stat.id != id) {
				continue;
			}
			if (std::holds_alternative<AverageRate>(stat.value) || stat.value.index() != value.index()) {
				return error(generic_errc::type_mismatch, "stat numeric type mismatch");
			}
			StatValue next = std::visit([](auto v) -> StatValue { return v; }, value);
			IF_ERROR_RETURN_ERROR(progression_validate(stat, next));
			if (progression_value(stat.value) == progression_value(next)) {
				if (!std::holds_alternative<i64>(next) || std::get<i64>(next) == std::get<i64>(stat.value)) {
					return {};
				}
			}
			Progression candidate = *this;
			const usize index = static_cast<usize>(&stat - stats_.data());
			candidate.stats_[index].value = next;
			++candidate.stats_[index].revision;
			IF_ERROR_RETURN_ERROR(candidate.evaluate());
			commit_values(std::move(candidate));
			return {};
		}
		return error(generic_errc::not_found, "missing stat");
	}

	error Progression::add_stat(string_view id, StatNumber amount) {
		const auto* stat = find_stat(id);
		if (!stat) {
			return error(generic_errc::not_found, "missing stat");
		}
		if (stat->value.index() != amount.index()) {
			return error(generic_errc::type_mismatch, "stat numeric type mismatch");
		}
		if (const auto* integer = std::get_if<i64>(&amount)) {
			const i64 current = std::get<i64>(stat->value);
			if ((*integer > 0 && current > std::numeric_limits<i64>::max() - *integer) || (*integer < 0 && current < std::numeric_limits<i64>::min() - *integer)) {
				return error(generic_errc::arithmetic_error, "integer stat overflow");
			}
			return set_stat(id, current + *integer);
		}
		return set_stat(id, std::get<f64>(stat->value) + std::get<f64>(amount));
	}

	error Progression::update_rate(string_view id, f64 amount, f64 elapsed_seconds) {
		if (!std::isfinite(amount) || !std::isfinite(elapsed_seconds) || elapsed_seconds <= 0) {
			return error(generic_errc::invalid_argument, "invalid rate sample");
		}
		Progression candidate = *this;
		for (auto& stat : candidate.stats_) {
			if (stat.id != id) {
				continue;
			}
			auto* rate = std::get_if<AverageRate>(&stat.value);
			if (!rate) {
				return error(generic_errc::type_mismatch, "not an average rate stat");
			}
			rate->samples.push_back({ amount, elapsed_seconds });
			f64 duration = 0;
			for (const auto& sample : rate->samples) {
				duration += sample.seconds;
			}
			f64 excess = duration - stat.prototype.window_seconds;
			while (excess > 0 && !rate->samples.empty()) {
				auto& sample = rate->samples.front();
				if (sample.seconds <= excess) {
					excess -= sample.seconds;
					rate->samples.erase(rate->samples.begin());
				}
				else {
					sample.amount *= (sample.seconds - excess) / sample.seconds;
					sample.seconds -= excess;
					excess = 0;
				}
			}
			f64 total = 0;
			duration = 0;
			for (const auto& sample : rate->samples) {
				total += sample.amount;
				duration += sample.seconds;
			}
			rate->value = total / duration;
			IF_ERROR_RETURN_ERROR(progression_validate(*find_stat(id), stat.value));
			if (!stat.prototype.store_id.empty()) {
				stat.pending_rates.push_back({ amount, elapsed_seconds });
			}
			++stat.revision;
			IF_ERROR_RETURN_ERROR(candidate.evaluate());
			commit_values(std::move(candidate));
			return {};
		}
		return error(generic_errc::not_found, "missing stat");
	}

	error Progression::merge_stat(string_view id, StatNumber value) {
		const auto* stat = find_stat(id);
		if (!stat) {
			return error(generic_errc::not_found, "missing stat");
		}
		if (stat->prototype.increment_only && ((std::holds_alternative<i64>(value) && std::holds_alternative<i64>(stat->value)) ? std::get<i64>(value) <= std::get<i64>(stat->value) : progression_number(value) <= progression_value(stat->value))) {
			return {};
		}
		if (std::holds_alternative<AverageRate>(stat->value)) {
			if (!std::get<AverageRate>(stat->value).samples.empty()) {
				return {};
			}
			Progression candidate = *this;
			for (auto& local : candidate.stats_) {
				if (local.id != id) {
					continue;
				}
				std::get<AverageRate>(local.value).value = progression_number(value);
				IF_ERROR_RETURN_ERROR(progression_validate(*stat, local.value));
			}
			IF_ERROR_RETURN_ERROR(candidate.evaluate());
			commit_values(std::move(candidate));
			return {};
		}
		return set_stat(id, value);
	}

	error Progression::complete_achievement(string_view id) {
		for (auto& achievement : achievements_) {
			if (achievement.id == id) {
				if (!achievement.completed) {
					achievement.completed = true;
					achievement.progress = 1;
					++achievement.revision;
				}
				return {};
			}
		}
		return error(generic_errc::not_found, "missing achievement");
	}

	error Progression::evaluate() {
		try {
			vector<ConditionResult> results;
			for (const auto& achievement : achievements_) {
				ConditionResult result{ !achievement.prototype.conditions.empty(), 1 };
				if (achievement.prototype.conditions.empty()) {
					result.progress = 0;
				}
				for (const auto& condition : achievement.prototype.conditions) {
					const string type = condition.parse_field<string>("type");
					const ConditionEvaluator* evaluator = nullptr;
					for (const auto& entry : evaluators_) {
						if (entry.first == type) {
							evaluator = &entry.second;
							break;
						}
					}
					if (!evaluator) {
						return error(generic_errc::not_found, "unknown achievement condition type");
					}
					auto value = (*evaluator)(condition, *this);
					if (!value) {
						return value.error();
					}
					if (!std::isfinite(value->progress) || value->progress < 0 || value->progress > 1) {
						return error(generic_errc::invalid_argument, "invalid condition progress");
					}
					result.completed = result.completed && value->completed;
					result.progress = std::min(result.progress, value->progress);
				}
				results.push_back(result);
			}
			for (usize i = 0; i < achievements_.size(); ++i) {
				auto& achievement = achievements_[i];
				if (achievement.completed) {
					continue;
				}
				achievement.progress = results[i].progress;
				if (results[i].completed) {
					achievement.completed = true;
					achievement.progress = 1;
					++achievement.revision;
				}
			}
			return {};
		} catch (const std::exception& e) {
			return error(generic_errc::invalid_argument, e.what());
		}
	}

	void Progression::acknowledge_stat(string_view id, u64 revision, usize rate_samples) {
		for (auto& stat : stats_) {
			if (stat.id != id || revision > stat.revision || revision <= stat.synced_revision) {
				continue;
			}
			stat.synced_revision = revision;
			stat.pending_rates.erase(stat.pending_rates.begin(), stat.pending_rates.begin() + std::min(rate_samples, stat.pending_rates.size()));
		}
	}

	void Progression::acknowledge_achievement(string_view id, u64 revision) {
		for (auto& achievement : achievements_) {
			if (achievement.id == id && revision <= achievement.revision) {
				achievement.synced_revision = std::max(revision, achievement.synced_revision);
			}
		}
	}

	ProgressionSnapshot Progression::snapshot() const {
		ProgressionSnapshot result;
		for (const auto& stat : stats_) {
			StatSnapshot record;
			record.id = stat.id;
			record.value_type = stat.prototype.value_type;
			if (const auto* integer = std::get_if<i64>(&stat.value)) {
				record.integer = *integer;
			}
			else if (const auto* floating = std::get_if<f64>(&stat.value)) {
				record.floating = *floating;
			}
			else {
				record.rate = std::get<AverageRate>(stat.value);
			}
			record.pending_rates = stat.pending_rates;
			result.stats.push_back(std::move(record));
		}
		for (const auto& achievement : achievements_) {
			result.achievements.push_back({ achievement.id, achievement.completed });
		}
		return result;
	}

	error Progression::restore(const ProgressionSnapshot& snapshot) {
		if (snapshot.format != 1) {
			return error(generic_errc::parse_error, "unknown progression format");
		}
		Progression candidate = *this;
		std::unordered_set<string> seen;
		for (const auto& record : snapshot.stats) {
			if (!seen.insert(record.id).second) {
				return error(generic_errc::parse_error, "duplicate saved stat");
			}
			for (auto& stat : candidate.stats_) {
				if (record.id != stat.id) {
					continue;
				}
				StatValue saved_value;
				if (record.value_type == "integer") {
					saved_value = record.integer;
				}
				else if (record.value_type == "float") {
					saved_value = record.floating;
				}
				else if (record.value_type == "average_rate") {
					saved_value = record.rate;
				}
				else {
					return error(generic_errc::parse_error, "unknown saved stat type");
				}
				if (saved_value.index() != stat.value.index()) {
					return error(generic_errc::type_mismatch, "saved stat type changed");
				}
				IF_ERROR_RETURN_ERROR(progression_validate(stat, saved_value));
				if (const auto* rate = std::get_if<AverageRate>(&saved_value)) {
					f64 duration = 0;
					f64 amount = 0;
					for (const auto& sample : rate->samples) {
						if (!std::isfinite(sample.amount) || !std::isfinite(sample.seconds) || sample.seconds <= 0) {
							return error(generic_errc::parse_error, "invalid saved rate sample");
						}
						duration += sample.seconds;
						amount += sample.amount;
					}
					if (!std::isfinite(duration) || !std::isfinite(amount) || duration > stat.prototype.window_seconds + 1e-6 || (duration > 0 && std::abs(rate->value - amount / duration) > std::max(1.0, std::abs(rate->value)) * 1e-9)) {
						return error(generic_errc::parse_error, "inconsistent saved rate");
					}
				}
				else if (!record.pending_rates.empty()) {
					return error(generic_errc::parse_error, "rate samples on scalar stat");
				}
				for (const auto& sample : record.pending_rates) {
					if (!std::isfinite(sample.amount) || !std::isfinite(sample.seconds) || sample.seconds <= 0) {
						return error(generic_errc::parse_error, "invalid pending rate");
					}
				}
				stat.value = saved_value;
				stat.pending_rates = record.pending_rates;
				++stat.revision;
			}
		}
		seen.clear();
		for (const auto& record : snapshot.achievements) {
			if (!seen.insert(record.id).second) {
				return error(generic_errc::parse_error, "duplicate saved achievement");
			}
			if (record.completed && candidate.find_achievement(record.id)) {
				IF_ERROR_RETURN_ERROR(candidate.complete_achievement(record.id));
			}
		}
		IF_ERROR_RETURN_ERROR(candidate.evaluate());
		commit_values(std::move(candidate));
		return {};
	}

	error Progression::save(fs::path_view path) const {
		auto bytes = bin::write(snapshot());
		if (!bytes) {
			return bytes.error();
		}
		auto result = fs::write_all_atomic(path, span<const u08>(reinterpret_cast<const u08*>(bytes->data()), bytes->size()), fs::atomic_write_options(fs::existing_action::replace, fs::parent_action::create));
		return result ? error{} : result.error();
	}

	error Progression::load(fs::path_view path) {
		auto bytes = fs::read_all(path, 16 * 1024 * 1024);
		if (!bytes) {
			return bytes.error();
		}
		auto state = bin::read<ProgressionSnapshot>(span<const byte>(reinterpret_cast<const byte*>(bytes->data()), bytes->size()), bin::read_options{ bin::read_limits{ 1024 * 1024, 100000 }, {} });
		if (!state) {
			return state.error();
		}
		return restore(*state);
	}
	void Progression::commit_values(Progression&& candidate) {
		for (usize index = 0; index < stats_.size(); ++index) {
			auto& destination = stats_[index];
			auto& source = candidate.stats_[index];
			destination.value = std::move(source.value);
			destination.pending_rates = std::move(source.pending_rates);
			destination.revision = source.revision;
			destination.synced_revision = source.synced_revision;
		}
		for (usize index = 0; index < achievements_.size(); ++index) {
			auto& destination = achievements_[index];
			const auto& source = candidate.achievements_[index];
			destination.completed = source.completed;
			destination.progress = source.progress;
			destination.revision = source.revision;
			destination.synced_revision = source.synced_revision;
		}
	}
} // namespace lf
