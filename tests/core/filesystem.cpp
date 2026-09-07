#include "leaf/core/filesystem.hpp"
#include "leaf/core/zip.hpp"
#include "leaf/core/scope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace lf::tests {
	TEST_CASE("filesystem system roots overlay appdata on installation", "[filesystem]") {
		const auto root{ std::filesystem::absolute("filesystem-root-test-work") };
		REQUIRE(root.parent_path() == std::filesystem::current_path());
		auto storage{ fs::native_volume(root, fs::native_volume_options{ fs::access_mode::read_write, fs::missing_action::create }) };
		REQUIRE(storage);
		scope_exit cleanup{ [&] { std::error_code error; std::filesystem::remove_all(root, error); } };
		auto setup{ fs::mount("/test-setup", *storage) };
		REQUIRE(setup);
		REQUIRE(fs::create_directories("/test-setup/install/data"));
		REQUIRE(fs::create_directories("/test-setup/appdata"));
		const vector<u08> installed{ 'b', 'a', 's', 'e' };
		const vector<u08> user{ 'u', 's', 'e', 'r' };
		REQUIRE(fs::write_all("/test-setup/install/data/value", installed));
		REQUIRE_FALSE(fs::init(root / "install", root / "appdata"));
		scope_exit shutdown{ fs::exit };
		REQUIRE(fs::read_all("/data/value") == installed);
		const auto entries{ fs::list("/data") };
		REQUIRE(entries);
		REQUIRE(entries->size() == 1);
		REQUIRE(fs::create_directories("/data"));
		REQUIRE(fs::write_all("/data/value", user));
		REQUIRE(fs::read_all("/data/value") == user);
		REQUIRE(fs::read_all("/test-setup/install/data/value") == installed);
		REQUIRE(fs::read_all("/test-setup/appdata/data/value") == user);
		REQUIRE_FALSE(fs::exists("/install"));
		REQUIRE_FALSE(fs::exists("/appdata"));
		REQUIRE_FALSE(fs::exists("/current"));
	}

	TEST_CASE("filesystem paths validate views and preserve owning values", "[filesystem]") {
		REQUIRE(fs::path::parse("mods/core"));
		const fs::path relative{ "mods/core" };
		REQUIRE_FALSE(relative.is_absolute());
		REQUIRE(relative.parent().text() == "mods");
		REQUIRE(relative.parent().parent().empty());
		REQUIRE((fs::path{ "/" } / relative).text() == "/mods/core");
		REQUIRE((fs::path{ "base" } / relative).text() == "base/mods/core");
		REQUIRE((relative / "/settings").text() == "/settings");
		REQUIRE(fs::path{ "/core" }.parent().is_root());
		REQUIRE_FALSE(fs::open_file(relative));
		REQUIRE_FALSE(fs::path::parse("/mods/../core"));
		REQUIRE_FALSE(fs::path::parse("/mods\\core"));
		REQUIRE_FALSE(fs::path::parse("C:/escape"));
		REQUIRE_FALSE(fs::path::parse("C:escape"));

		auto location = fs::path("/mods/core");
		const auto view = fs::path_view(location);
		REQUIRE(view.text() == "/mods/core");
		REQUIRE(view.filename() == "core");
		REQUIRE(view.owning() == location);
		REQUIRE(location.append("data.lua").text() == "/mods/core/data.lua");
	}

	TEST_CASE("filesystem mounts borrowed files without copying bytes", "[filesystem]") {
		auto bytes = vector<u08>(4);
		bytes[0] = 'l';
		bytes[1] = 'e';
		bytes[2] = 'a';
		bytes[3] = 'f';

		auto source = fs::borrow(span<u08>(bytes.data(), bytes.size()));
		report<fs::mapping> mounted = fs::mount("/runtime/message.txt", source);
		REQUIRE(mounted);
		REQUIRE(fs::exists("/runtime"));
		REQUIRE(fs::exists("/runtime/message.txt"));

		report<fs::file> opened = fs::open_file("/runtime/message.txt", fs::open_options(fs::file_access::read_write));
		REQUIRE_FALSE(opened);

		report<fs::file> read_only = fs::open_file("/runtime/message.txt");
		REQUIRE(read_only);
		report<fs::stream> stream = read_only->open(fs::file_access::read_write);
		REQUIRE(stream);
		REQUIRE(stream->seek(0, fs::seek_origin::begin));
		const auto replacement = vector<u08>{ 'L', 'e', 'a', 'f' };
		REQUIRE(stream->write_all(span<const u08>(replacement.data(), replacement.size())));
		REQUIRE(bytes[0] == 'L');
	}

	TEST_CASE("filesystem unmount hides new lookups while open files survive", "[filesystem]") {
		auto bytes = vector<u08>{ 'o', 'p', 'e', 'n' };
		auto source = fs::borrow(span<const u08>(bytes.data(), bytes.size()));
		report<fs::mapping> mounted = fs::mount("/runtime/open.txt", source);
		REQUIRE(mounted);
		report<fs::file> opened = fs::open_file("/runtime/open.txt");
		REQUIRE(opened);

		mounted->unmount();
		REQUIRE_FALSE(fs::exists("/runtime/open.txt"));
		report<vector<u08>> data = opened->read_all();
		REQUIRE(data);
		REQUIRE(*data == bytes);
	}

	TEST_CASE("filesystem layers direct files last-mounted-first", "[filesystem]") {
		auto lower = vector<u08>{ 'l', 'o', 'w' };
		auto upper = vector<u08>{ 'h', 'i', 'g', 'h' };
		auto lower_source = fs::borrow(span<const u08>(lower.data(), lower.size()));
		auto upper_source = fs::borrow(span<const u08>(upper.data(), upper.size()));

		report<fs::mapping> lower_mount = fs::mount("/layers/value.txt", lower_source);
		report<fs::mapping> upper_mount = fs::mount("/layers/value.txt", upper_source);
		REQUIRE(lower_mount);
		REQUIRE(upper_mount);
		REQUIRE(fs::read_all("/layers/value.txt") == upper);

		upper_mount->unmount();
		REQUIRE(fs::read_all("/layers/value.txt") == lower);
	}

	TEST_CASE("filesystem mounts native writable volumes", "[filesystem]") {
		const auto root = fs::native_path("filesystem-test-work");
		std::error_code system_error;
		std::filesystem::remove_all(root, system_error);

		report<fs::volume> source = fs::native_volume(root, fs::native_volume_options(fs::access_mode::read_write, fs::missing_action::create));
		REQUIRE(source);
		report<fs::mapping> mounted = fs::mount("/native", *source);
		REQUIRE(mounted);
		REQUIRE(fs::create_directories("/native/config"));

		const auto text = vector<u08>{ 'v', 'a', 'l', 'u', 'e' };
		REQUIRE(fs::write_all("/native/config/value.txt", span<const u08>(text.data(), text.size())));
		REQUIRE(fs::read_all("/native/config/value.txt") == text);
		report<void> atomically_written = fs::write_all_atomic("/native/config/value.txt", span<const u08>(text.data(), text.size()));
		string atomic_error;
		if (!atomically_written) {
			atomic_error = atomically_written.error().message;
		}
		INFO(atomic_error);
		REQUIRE(atomically_written);
		REQUIRE(fs::copy("/native/config/value.txt", "/native/config/copy.txt"));
		REQUIRE(fs::move("/native/config/copy.txt", "/native/config/moved.txt"));
		REQUIRE(fs::exists("/native/config/moved.txt"));

		mounted->unmount();
		std::filesystem::remove_all(root, system_error);
	}

	TEST_CASE("filesystem mounts ZIP package volumes", "[filesystem]") {
		const auto package_bytes = vector<u08>{
			0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xa6,
			0x10, 0x36, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 'h', 'e',
			'l', 'l', 'o', '.', 't', 'x', 't', 'h', 'e', 'l', 'l', 'o', 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00,
			0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0xa6, 0x10, 0x36, 0x05, 0x00,
			0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o', '.', 't', 'x', 't',
			0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x37, 0x00, 0x00, 0x00,
			0x2c, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		auto source = fs::borrow(span<const u08>(package_bytes.data(), package_bytes.size()));
		report<fs::volume> zip_package = zip_decoder().open(source);
		REQUIRE(zip_package);
		auto zip_mount = fs::mount("/packages/zip", *zip_package);
		REQUIRE(zip_mount);
		const auto expected = vector<u08>{ 'h', 'e', 'l', 'l', 'o' };
		REQUIRE(fs::read_all("/packages/zip/hello.txt") == expected);
	}
} // namespace lf::tests
