#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/progression/progression.hpp>
#include <limits>

namespace leaf_test::progression {
	void reset() {
		lf::Database<lf::StatPrototype>::clear();
		lf::Database<lf::AchievementPrototype>::clear();
	}
	void stat(lf::string id, lf::string kind, bool monotonic = false) {
		lf::dict fields;
		fields.emplace("name", id);
		fields.emplace("value_type", kind);
		fields.emplace("increment_only", monotonic);
		fields.emplace("window_seconds", 10.0);
		if (kind == "average_rate") {
			fields.emplace("store_id", id);
		}
		lf::Database<lf::StatPrototype>::create(id);
		lf::Database<lf::StatPrototype>::init(id, fields);
	}
	void achievement(lf::string stat_id, i64 target = 5) {
		lf::dict condition;
		condition.emplace("type", "stat_threshold");
		condition.emplace("stat", stat_id);
		condition.emplace("threshold", target);
		lf::list conditions;
		conditions.emplace_back(condition);
		lf::dict fields;
		fields.emplace("name", "first-five");
		fields.emplace("conditions", conditions);
		lf::Database<lf::AchievementPrototype>::create("first-five");
		lf::Database<lf::AchievementPrototype>::init("first-five", fields);
	}
} // namespace leaf_test::progression

TEST_CASE("stats drive achievement progress and unlock only once", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("levels", "integer", true);
	achievement("levels");
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.set_stat("levels", i64(3)));
	REQUIRE(state.find_achievement("first-five")->progress == Catch::Approx(0.6));
	REQUIRE_FALSE(state.add_stat("levels", i64(2)));
	REQUIRE(state.find_achievement("first-five")->completed);
	const u64 revision = state.find_achievement("first-five")->revision;
	REQUIRE_FALSE(state.set_stat("levels", i64(5)));
	REQUIRE_FALSE(state.evaluate());
	REQUIRE(state.find_achievement("first-five")->revision == revision);
	REQUIRE(state.set_stat("levels", i64(4)));
	REQUIRE(state.set_stat("levels", 4.0));
	REQUIRE(std::get<i64>(state.find_stat("levels")->value) == 5);
	reset();
}

TEST_CASE("missing condition dependencies reject initialization atomically", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("levels", "integer");
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.set_stat("levels", i64(3)));
	achievement("absent");
	REQUIRE(state.initialize());
	REQUIRE(std::get<i64>(state.find_stat("levels")->value) == 3);
	REQUIRE(state.achievements().empty());
	reset();
}

TEST_CASE("average rate trims elapsed windows and preserves pending samples", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("rate", "average_rate");
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.update_rate("rate", 100, 10));
	REQUIRE_FALSE(state.update_rate("rate", 0, 5));
	REQUIRE(std::get<lf::AverageRate>(state.find_stat("rate")->value).value == Catch::Approx(5));
	const u64 first_batch = state.find_stat("rate")->revision;
	REQUIRE_FALSE(state.update_rate("rate", 20, 5));
	state.acknowledge_stat("rate", first_batch, 2);
	REQUIRE(state.find_stat("rate")->pending_rates.size() == 1);
	REQUIRE(std::get<lf::AverageRate>(state.find_stat("rate")->value).value == Catch::Approx(2));
	REQUIRE(state.update_rate("rate", 1, 0));
	lf::Progression restored;
	REQUIRE_FALSE(restored.initialize());
	REQUIRE_FALSE(restored.restore(state.snapshot()));
	REQUIRE_FALSE(restored.update_rate("rate", 40, 5));
	REQUIRE(std::get<lf::AverageRate>(restored.find_stat("rate")->value).value == Catch::Approx(6));
	reset();
}

TEST_CASE("typed snapshot rejects malformed state without partial changes", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("integer", "integer");
	stat("float", "float");
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.set_stat("integer", i64(9007199254740993LL)));
	REQUIRE_FALSE(state.set_stat("float", 1.25));
	const auto saved = state.snapshot();
	auto bytes = lf::bin::write(saved);
	REQUIRE(bytes);
	auto read = lf::bin::read<lf::ProgressionSnapshot>(*bytes);
	REQUIRE(read);
	lf::Progression restored;
	REQUIRE_FALSE(restored.initialize());
	REQUIRE_FALSE(restored.restore(*read));
	REQUIRE(std::get<i64>(restored.find_stat("integer")->value) == 9007199254740993LL);
	REQUIRE(std::get<f64>(restored.find_stat("float")->value) == 1.25);
	auto corrupt = saved;
	corrupt.stats[0].integer = 42;
	corrupt.stats[1].value_type = "integer";
	REQUIRE(restored.restore(corrupt));
	REQUIRE(std::get<i64>(restored.find_stat("integer")->value) == 9007199254740993LL);
	corrupt = saved;
	corrupt.stats.push_back(corrupt.stats.front());
	REQUIRE(restored.restore(corrupt));
	reset();
}

TEST_CASE("engine conditions can be registered without Lua callbacks", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	lf::dict condition;
	condition.emplace("type", "quest_count");
	condition.emplace("required", i64(3));
	lf::list conditions;
	conditions.emplace_back(condition);
	lf::dict fields;
	fields.emplace("name", "quests");
	fields.emplace("conditions", conditions);
	lf::Database<lf::AchievementPrototype>::create("quests");
	lf::Database<lf::AchievementPrototype>::init("quests", fields);
	lf::Progression state;
	REQUIRE_FALSE(state.register_condition("quest_count", [](const lf::dict& params, const lf::Progression&) -> lf::report<lf::ConditionResult> {
		return lf::ConditionResult{ params.parse_field<i64>("required") == 3, 1 };
	}));
	REQUIRE_FALSE(state.initialize());
	REQUIRE(state.find_achievement("quests")->completed);
	reset();
}

TEST_CASE("local progression file survives reload and rejects truncated saves", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("levels", "integer", true);
	achievement("levels");
	const auto directory = std::filesystem::absolute("progression-save-test-work");
	REQUIRE(directory.parent_path() == std::filesystem::current_path());
	auto volume = lf::fs::native_volume(directory, lf::fs::native_volume_options(lf::fs::access_mode::read_write, lf::fs::missing_action::create));
	REQUIRE(volume);
	lf::scope_exit cleanup([&] { std::error_code ignored; std::filesystem::remove_all(directory,ignored); });
	auto mount = lf::fs::mount("/progression-test", *volume);
	REQUIRE(mount);
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.set_stat("levels", i64(5)));
	REQUIRE_FALSE(state.save("/progression-test/profile.bin"));
	lf::Progression restored;
	REQUIRE_FALSE(restored.initialize());
	REQUIRE_FALSE(restored.load("/progression-test/profile.bin"));
	REQUIRE(std::get<i64>(restored.find_stat("levels")->value) == 5);
	REQUIRE(restored.find_achievement("first-five")->completed);
	const lf::vector<u08> truncated{ 1, 2, 3 };
	REQUIRE(lf::fs::write_all("/progression-test/profile.bin", truncated));
	REQUIRE(restored.load("/progression-test/profile.bin"));
	REQUIRE(std::get<i64>(restored.find_stat("levels")->value) == 5);
	reset();
}

TEST_CASE("integer statistics preserve precision and reject arithmetic overflow", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("count", "integer", true);
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE_FALSE(state.set_stat("count", i64(9007199254740993LL)));
	REQUIRE(state.set_stat("count", i64(9007199254740992LL)));
	REQUIRE_FALSE(state.set_stat("count", std::numeric_limits<i64>::max()));
	REQUIRE(state.add_stat("count", i64(1)));
	REQUIRE(std::get<i64>(state.find_stat("count")->value) == std::numeric_limits<i64>::max());
	reset();
}

TEST_CASE("local average rates do not accumulate storefront work", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	lf::dict fields;
	fields.emplace("name", "local-rate");
	fields.emplace("value_type", "average_rate");
	fields.emplace("window_seconds", 10.0);
	fields.emplace("max_value", 10.0);
	lf::Database<lf::StatPrototype>::create("local-rate");
	lf::Database<lf::StatPrototype>::init("local-rate", fields);
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE(state.merge_stat("local-rate", 11.0));
	REQUIRE(std::get<lf::AverageRate>(state.find_stat("local-rate")->value).value == 0);
	for (usize index = 0; index < 50; ++index) {
		REQUIRE_FALSE(state.update_rate("local-rate", 1.0, 1.0));
	}
	REQUIRE(state.find_stat("local-rate")->pending_rates.empty());
	REQUIRE(std::get<lf::AverageRate>(state.find_stat("local-rate")->value).samples.size() == 10);
	reset();
}

TEST_CASE("store constraints and local scalar types reject invalid updates", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	lf::dict fields;
	fields.emplace("name", "store-count");
	fields.emplace("value_type", "integer");
	fields.emplace("store_id", "STORE_COUNT");
	fields.emplace("min_value", 0.0);
	fields.emplace("max_value", 100.0);
	lf::Database<lf::StatPrototype>::create("store-count");
	lf::Database<lf::StatPrototype>::init("store-count", fields);
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	REQUIRE(state.set_stat("store-count", i64(-1)));
	REQUIRE(state.set_stat("store-count", i64(101)));
	REQUIRE(state.set_stat("store-count", std::numeric_limits<i64>::max()));
	REQUIRE(state.set_stat("store-count", 1.0));
	REQUIRE(std::get<i64>(state.find_stat("store-count")->value) == 0);
	reset();
}

TEST_CASE("runtime object identity survives stat mutations and restore", "[progression]") {
	using namespace leaf_test::progression;
	reset();
	stat("levels", "integer");
	achievement("levels");
	lf::Progression state;
	REQUIRE_FALSE(state.initialize());
	const auto* count = state.find_stat("levels");
	const auto* unlock = state.find_achievement("first-five");
	REQUIRE_FALSE(state.set_stat("levels", i64(5)));
	REQUIRE(count == state.find_stat("levels"));
	REQUIRE(unlock == state.find_achievement("first-five"));
	REQUIRE(std::get<i64>(count->value) == 5);
	REQUIRE(unlock->completed);
	REQUIRE_FALSE(state.restore(state.snapshot()));
	REQUIRE(count == state.find_stat("levels"));
	REQUIRE(unlock == state.find_achievement("first-five"));
	reset();
}
