#include <catch2/catch_test_macros.hpp>

#include <leaf/core/time.hpp>

#include <chrono>
#include <type_traits>

static_assert(!std::is_same_v<lf::instant, lf::duration>);
static_assert(!std::is_same_v<lf::instant, lf::timespan>);
static_assert(!std::is_same_v<lf::duration, lf::timespan>);
static_assert(!std::is_convertible_v<lf::instant, lf::duration>);
static_assert(!std::is_convertible_v<lf::duration, lf::timespan>);

TEST_CASE("time types preserve point and duration geometry") {
	const lf::duration vector = lf::duration::from_chrono(std::chrono::milliseconds(250));
	const lf::instant point = lf::instant::from_quantum(1'000'000'000);
	const lf::instant advanced = point + vector;

	REQUIRE(vector.to_chrono<i64, std::milli>().count() == 250);
	REQUIRE((advanced - point) == vector);
	REQUIRE(lf::timespan::from_quantum(250'000'000).quantum_count() == vector.quantum_count());
}

TEST_CASE("frequency preserves integral and fractional hertz values") {
	const lf::frequency integral = lf::frequency::from_hertz(60);

	REQUIRE(integral.hertz().raw() == 60 * lf::fixed::scale);
	REQUIRE(lf::frequency::from_hertz(2.5).hertz_value() == 2.5);
}

TEST_CASE("frequency periods preserve session timing") {
	REQUIRE(lf::frequency::from_hertz(62.5).period().quantum_count() == 16'000'000);
	REQUIRE(lf::frequency::from_hertz(125).period().quantum_count() == 8'000'000);
	REQUIRE_THROWS(lf::frequency{}.period());
	REQUIRE_THROWS(lf::frequency::from_hertz(-1).period());
}

TEST_CASE("native sleeping waits for monotonic deadlines") {
	const auto delay = lf::duration::from_quantum(2'000'000);
	const auto start = lf::now();
	lf::sleep_for(delay);
	REQUIRE(lf::now() - start >= delay);

	const auto deadline = lf::now() + delay;
	lf::sleep_until(deadline);
	REQUIRE(lf::now() >= deadline);

	lf::sleep_until(start);
	lf::sleep_for(lf::duration{});
	lf::sleep_for(-delay);
	REQUIRE(lf::now() >= deadline);
}

TEST_CASE("native wall clock uses the Unix epoch") {
	const auto before = std::chrono::system_clock::now().time_since_epoch();
	const auto value = lf::wall_now().since_unix_epoch();
	const auto after = std::chrono::system_clock::now().time_since_epoch();
	REQUIRE(value >= lf::duration::from_chrono(before));
	REQUIRE(value <= lf::duration::from_chrono(after));
}
