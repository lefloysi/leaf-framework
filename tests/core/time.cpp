#include <catch2/catch_test_macros.hpp>

#include <leaf/core/dynamic_object.hpp>
#include <leaf/core/time.hpp>

#include <chrono>
#include <limits>
#include <type_traits>

TEST_CASE("timespan preserves elapsed-time arithmetic") {
	const lf::timespan vector = lf::timespan::from_chrono(std::chrono::milliseconds(250));
	const lf::timespan point = lf::timespan::from_quantum(1'000'000'000);
	const lf::timespan advanced = point + vector;

	REQUIRE(vector.to_chrono<i64, std::milli>().count() == 250);
	REQUIRE((advanced - point) == vector);
	REQUIRE(lf::timespan::from_quantum(250'000'000).quantum_count() == vector.quantum_count());
}

TEST_CASE("frequency preserves integral and fractional hertz values") {
	const lf::frequency integral = lf::frequency{ lf::hertz{ 60 } };

	REQUIRE(integral.in_hertz().quantum_count().raw() == 60 * lf::fixed::scale);
	REQUIRE(lf::frequency{ lf::hertz{ 2.5 } }.in_hertz().as_f64() == 2.5);
	REQUIRE(lf::hertz::from_raw(2'500'000'000).quantum_count().raw() == 2'500'000'000);
}

TEST_CASE("dynamic fixed values preserve exact and numeric prototype values") {
	REQUIRE(lf::object{ "0.05" }.parse<lf::fixed>().raw() == 50'000'000);
	REQUIRE(lf::object{ 0.125 }.parse<lf::fixed>().raw() == 125'000'000);
	REQUIRE(lf::object{ -3 }.parse<lf::fixed>().raw() == -3 * lf::fixed::scale);
	REQUIRE_THROWS(lf::object{ std::numeric_limits<f64>::infinity() }.parse<lf::fixed>());
}

TEST_CASE("frequency periods preserve session timing") {
	REQUIRE(lf::frequency{ lf::hertz{ 62.5 } }.period().quantum_count() == 16'000'000);
	REQUIRE(lf::frequency{ lf::hertz{ 125 } }.period().quantum_count() == 8'000'000);
	REQUIRE_THROWS(lf::frequency{}.period());
	REQUIRE_THROWS(lf::frequency{ lf::hertz{ -1 } }.period());
}

TEST_CASE("native sleeping waits for monotonic deadlines") {
	const auto delay = lf::timespan::from_quantum(2'000'000);
	const auto start = lf::now();
	lf::sleep_for(delay);
	REQUIRE(lf::now() - start >= delay);

	const auto deadline = lf::now() + delay;
	lf::sleep_until(deadline);
	REQUIRE(lf::now() >= deadline);

	lf::sleep_until(start);
	lf::sleep_for(lf::timespan{});
	lf::sleep_for(-delay);
	REQUIRE(lf::now() >= deadline);
}

TEST_CASE("native wall clock uses the Unix epoch") {
	const auto before = std::chrono::system_clock::now().time_since_epoch();
	const auto value = lf::wall_now().since_unix_epoch();
	const auto after = std::chrono::system_clock::now().time_since_epoch();
	REQUIRE(value >= lf::timespan::from_chrono(before));
	REQUIRE(value <= lf::timespan::from_chrono(after));
}
