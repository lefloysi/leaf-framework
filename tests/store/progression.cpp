#include <catch2/catch_test_macros.hpp>

#include <leaf/progression/progression.hpp>
#include <leaf/store/progression.hpp>

#include <limits>

namespace leaf_test::store_progression {
	struct Backend final : lf::store::ProgressionBackend {
		bool available = true;
		bool reject_commit = false;
		bool reject_read = false;
		i32 remote_count = 0;
		f32 remote_float = 0;
		bool remote_achievement = false;
		usize integer_writes = 0;
		usize float_writes = 0;
		usize rate_writes = 0;
		usize achievement_writes = 0;
		usize commits = 0;
		i32 written_integer = 0;
		f32 written_float = 0;
		f32 written_amount = 0;
		f64 written_seconds = 0;
		lf::store::progression_commit_state current = lf::store::progression_commit_state::idle;
		bool supported() const override { return available; }
		lf::report<i32> read_integer(lf::string_view) override {
			if (reject_read) { return lf::unexpected(lf::error("Undefined stat")); }
			return remote_count;
		}
		lf::report<f32> read_float(lf::string_view) override { return remote_float; }
		lf::report<bool> read_achievement(lf::string_view) override { return remote_achievement; }
		lf::error set_integer(lf::string_view id, i32 value) override {
			REQUIRE(id == "COUNT");
			++integer_writes;
			written_integer = value;
			return {};
		}
		lf::error set_float(lf::string_view id, f32 value) override {
			REQUIRE(id == "FLOAT");
			++float_writes;
			written_float = value;
			return {};
		}
		lf::error update_average(lf::string_view id, f32 amount, f64 seconds) override {
			REQUIRE(id == "RATE");
			++rate_writes;
			written_amount = amount;
			written_seconds = seconds;
			return {};
		}
		lf::error unlock(lf::string_view id) override {
			REQUIRE(id == "AWARD");
			++achievement_writes;
			return {};
		}
		lf::error commit() override {
			++commits;
			current = reject_commit ? lf::store::progression_commit_state::failed : lf::store::progression_commit_state::pending;
			return reject_commit ? last_error() : lf::error{};
		}
		lf::store::progression_commit_state status() const override { return current; }
		lf::error last_error() const override { return lf::error("Upload failed"); }
	};

	void add_stat(lf::string_view id, lf::string_view type, lf::string_view store_id) {
		lf::dict data;
		data.emplace("name", lf::string(id));
		data.emplace("value_type", lf::string(type));
		data.emplace("store_id", lf::string(store_id));
		data.emplace("increment_only", id == "count");
		lf::Database<lf::StatPrototype>::create(id);
		lf::Database<lf::StatPrototype>::init(id, data);
	}
	void add_achievement(lf::string_view id, lf::string_view store_id) {
		lf::dict data;
		data.emplace("name", lf::string(id));
		data.emplace("store_id", lf::string(store_id));
		lf::Database<lf::AchievementPrototype>::create(id);
		lf::Database<lf::AchievementPrototype>::init(id, data);
	}
	lf::Progression create() {
		lf::Database<lf::StatPrototype>::clear();
		lf::Database<lf::AchievementPrototype>::clear();
		add_stat("count", "integer", "COUNT");
		add_stat("local", "integer", "");
		add_stat("floating", "float", "FLOAT");
		add_stat("rate", "average_rate", "RATE");
		add_achievement("award", "AWARD");
		add_achievement("local_award", "");
		lf::Progression progression;
		REQUIRE_FALSE(progression.initialize());
		return progression;
	}
}

TEST_CASE("store progression keeps updates local until upload acknowledgement", "[progression]") {
	using namespace leaf_test::store_progression;
	auto progression = create();
	Backend backend;
	backend.remote_count = 3;
	lf::store::StoreProgression sync(backend);
	REQUIRE_FALSE(progression.set_stat("count", i64(5)));
	REQUIRE_FALSE(progression.set_stat("local", i64(20)));
	REQUIRE_FALSE(progression.set_stat("floating", f64(1.5)));
	REQUIRE_FALSE(progression.complete_achievement("award"));
	REQUIRE_FALSE(progression.complete_achievement("local_award"));
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(backend.integer_writes == 1);
	CHECK(backend.written_integer == 5);
	CHECK(backend.float_writes == 1);
	CHECK(backend.written_float == 1.5f);
	CHECK(backend.achievement_writes == 1);
	CHECK(progression.find_stat("count")->synced_revision == 0);
	REQUIRE_FALSE(progression.add_stat("count", i64(1)));
	backend.current = lf::store::progression_commit_state::succeeded;
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(backend.integer_writes == 2);
	CHECK(backend.written_integer == 6);
	CHECK(progression.find_stat("count")->synced_revision < progression.find_stat("count")->revision);
	backend.current = lf::store::progression_commit_state::succeeded;
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK_FALSE(sync.pending());
	CHECK(progression.find_stat("count")->synced_revision == progression.find_stat("count")->revision);
	CHECK(progression.find_stat("local")->synced_revision == 0);
}

TEST_CASE("failed store batches retry without double applying average samples", "[progression]") {
	using namespace leaf_test::store_progression;
	auto progression = create();
	Backend backend;
	backend.reject_commit = true;
	lf::store::StoreProgression sync(backend);
	REQUIRE_FALSE(progression.update_rate("rate", 10, 2));
	REQUIRE_FALSE(progression.update_rate("rate", 20, 3));
	CHECK(sync.synchronize(progression));
	CHECK(sync.pending());
	CHECK(backend.rate_writes == 1);
	CHECK(backend.written_amount == 30);
	CHECK(backend.written_seconds == 5);
	REQUIRE_FALSE(progression.update_rate("rate", 7, 1));
	backend.reject_commit = false;
	CHECK(sync.synchronize(progression));
	CHECK(backend.commits == 2);
	CHECK(backend.rate_writes == 1);
	backend.current = lf::store::progression_commit_state::succeeded;
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(backend.rate_writes == 2);
	CHECK(backend.written_amount == 7);
	CHECK(backend.written_seconds == 1);
	CHECK(progression.find_stat("rate")->pending_rates.size() == 1);
	backend.current = lf::store::progression_commit_state::failed;
	CHECK(sync.synchronize(progression));
	CHECK(backend.rate_writes == 2);
	backend.current = lf::store::progression_commit_state::succeeded;
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(progression.find_stat("rate")->pending_rates.empty());
}

TEST_CASE("store import preserves monotonic progress and standalone never claims upload", "[progression]") {
	using namespace leaf_test::store_progression;
	auto progression = create();
	Backend backend;
	backend.remote_count = 25;
	backend.remote_achievement = true;
	lf::store::StoreProgression sync(backend);
	REQUIRE_FALSE(progression.set_stat("count", i64(5)));
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(std::get<i64>(progression.find_stat("count")->value) == 25);
	CHECK(progression.find_achievement("award")->completed);
	CHECK_FALSE(progression.find_achievement("local_award")->completed);
	Backend none;
	none.available = false;
	lf::store::StoreProgression offline(none);
	REQUIRE_FALSE(offline.synchronize(progression));
	CHECK(none.commits == 0);
	CHECK(progression.find_stat("count")->synced_revision == 0);
}

TEST_CASE("store schema errors and numeric overflow retain local pending values", "[progression]") {
	using namespace leaf_test::store_progression;
	auto progression = create();
	Backend backend;
	backend.reject_read = true;
	lf::store::StoreProgression sync(backend);
	REQUIRE_FALSE(progression.set_stat("count", i64(5)));
	CHECK(sync.synchronize(progression));
	CHECK(backend.integer_writes == 0);
	CHECK(backend.commits == 0);
	CHECK(progression.find_stat("count")->synced_revision == 0);
	backend.reject_read = false;
	REQUIRE_FALSE(progression.set_stat("floating", f64(std::numeric_limits<f32>::max()) * 2));
	CHECK(sync.synchronize(progression));
	CHECK(backend.integer_writes == 1);
	CHECK(backend.float_writes == 0);
}

TEST_CASE("first store import fills untouched floating and average-rate stats", "[progression]") {
	using namespace leaf_test::store_progression;
	auto progression = create();
	Backend backend;
	backend.remote_float = 2.5f;
	lf::store::StoreProgression sync(backend);
	REQUIRE_FALSE(sync.synchronize(progression));
	CHECK(std::get<f64>(progression.find_stat("floating")->value) == 2.5);
	CHECK(std::get<lf::AverageRate>(progression.find_stat("rate")->value).value == 2.5);
	CHECK(backend.rate_writes == 0);
}
