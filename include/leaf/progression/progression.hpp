#pragma once

#include <functional>
#include <leaf/core/filesystem.hpp>
#include <leaf/resource/prototype.hpp>
#include <optional>
#include <variant>

namespace lf {
	using StatNumber = std::variant<i64, f64>;
	struct RateSample {
		f64 amount = 0;
		f64 seconds = 0;
	};
	struct AverageRate {
		f64 value = 0;
		vector<RateSample> samples;
	};
	using StatValue = std::variant<i64, f64, AverageRate>;

	struct StatPrototype final : Prototype<identifier<StatPrototype, u16, void>> {
		static string_view type() noexcept;
		explicit StatPrototype(const dict& data);
		string value_type = "integer";
		StatNumber default_value = i64(0);
		std::optional<f64> min_value;
		std::optional<f64> max_value;
		bool increment_only = false;
		f64 window_seconds = 3600;
		string store_id;
	};
	struct AchievementPrototype final : Prototype<identifier<AchievementPrototype, u16, void>> {
		static string_view type() noexcept;
		explicit AchievementPrototype(const dict& data);
		vector<dict> conditions;
		string store_id;
	};
	struct Stat {
		string id;
		StatPrototype prototype;
		StatValue value;
		u64 revision = 0;
		u64 synced_revision = 0;
		vector<RateSample> pending_rates;
	};
	struct Achievement {
		string id;
		AchievementPrototype prototype;
		bool completed = false;
		f64 progress = 0;
		u64 revision = 0;
		u64 synced_revision = 0;
	};
	class Progression;
	struct ConditionResult {
		bool completed = false;
		f64 progress = 0;
	};
	using ConditionEvaluator = std::function<report<ConditionResult>(const dict&, const Progression&)>;
	struct StatSnapshot {
		string id;
		string value_type;
		i64 integer = 0;
		f64 floating = 0;
		AverageRate rate;
		vector<RateSample> pending_rates;
	};
	struct AchievementSnapshot {
		string id;
		bool completed = false;
	};
	struct ProgressionSnapshot {
		u32 format = 1;
		vector<StatSnapshot> stats;
		vector<AchievementSnapshot> achievements;
	};
	class Progression {
	  public:
		Progression();
		error initialize();
		error register_condition(string id, ConditionEvaluator evaluator);
		const vector<Stat>& stats() const noexcept;
		const vector<Achievement>& achievements() const noexcept;
		const Stat* find_stat(string_view id) const noexcept;
		const Achievement* find_achievement(string_view id) const noexcept;
		error set_stat(string_view id, StatNumber value);
		error add_stat(string_view id, StatNumber amount);
		error update_rate(string_view id, f64 amount, f64 elapsed_seconds);
		error merge_stat(string_view id, StatNumber value);
		error complete_achievement(string_view id);
		error evaluate();
		void acknowledge_stat(string_view id, u64 revision, usize rate_samples = 0);
		void acknowledge_achievement(string_view id, u64 revision);
		ProgressionSnapshot snapshot() const;
		error restore(const ProgressionSnapshot& snapshot);
		error save(fs::path_view path) const;
		error load(fs::path_view path);

	  private:
		void commit_values(Progression&& candidate);
		vector<Stat> stats_;
		vector<Achievement> achievements_;
		vector<std::pair<string, ConditionEvaluator>> evaluators_;
	};

	template<>
	struct schema_trait<StatPrototype> {
		static auto get(auto& v) {
			return group(schema(PrototypeBase::base(v)), field("value_type", v.value_type), field("default_value", v.default_value), field("min_value", v.min_value), field("max_value", v.max_value), field("increment_only", v.increment_only), field("window_seconds", v.window_seconds), field("store_id", v.store_id));
		}
	};
	template<>
	struct schema_trait<AchievementPrototype> {
		static auto get(auto& v) {
			return group(schema(PrototypeBase::base(v)), field("conditions", v.conditions), field("store_id", v.store_id));
		}
	};
	template<>
	struct schema_trait<RateSample> {
		static auto get(auto& v) {
			return group(field("amount", v.amount), field("seconds", v.seconds));
		}
	};
	template<>
	struct schema_trait<AverageRate> {
		static auto get(auto& v) {
			return group(field("value", v.value), field("samples", v.samples));
		}
	};
	template<>
	struct schema_trait<StatSnapshot> {
		static auto get(auto& v) {
			return group(field("id", v.id), field("value_type", v.value_type), field("integer", v.integer), field("floating", v.floating), field("rate", v.rate), field("pending_rates", v.pending_rates));
		}
	};
	template<>
	struct schema_trait<AchievementSnapshot> {
		static auto get(auto& v) {
			return group(field("id", v.id), field("completed", v.completed));
		}
	};
	template<>
	struct schema_trait<ProgressionSnapshot> {
		static auto get(auto& v) {
			return group(field("format", v.format), field("stats", v.stats), field("achievements", v.achievements));
		}
	};
} // namespace lf
