#include <leaf/audio/audio.hpp>
#include <leaf/core/filesystem.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/resource/database.hpp>
#include <leaf/resource/registry.hpp>
#include <leaf/script/mod_loader.hpp>
#include <leaf/script/mod_enabled.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace lf::audio_tests {
	vector<u08> sample_wave() {
		vector<u08> bytes;
		const auto text = [&](string_view text) { bytes.insert(bytes.end(), text.begin(), text.end()); };
		const auto number = [&](u32 value, u32 count) {
			for (u32 index = 0; index < count; ++index) { bytes.push_back(static_cast<u08>(value >> (8 * index))); }
		};
		text("RIFF"); number(36 + 9600, 4); text("WAVEfmt "); number(16, 4);
		number(1, 2); number(1, 2); number(48000, 4); number(96000, 4); number(2, 2); number(16, 2);
		text("data"); number(9600, 4);
		for (u32 index = 0; index < 4800; ++index) { number(8192, 2); }
		return bytes;
	}

	void initialize_prototypes() {
		Database<SoundGroupPrototype>::clear();
		Database<SoundPrototype>::clear();
		Database<SoundGroupPrototype>::create("effects");
		dict group;
		group.emplace("name", "effects");
		group.emplace("volume", 0.5f);
		Database<SoundGroupPrototype>::init("effects", group);
		Database<SoundPrototype>::create("tap");
		dict sound;
		sound.emplace("name", "tap");
		sound.emplace("path", "/audio-test/tap.wav");
		sound.emplace("sound_group", "effects");
		sound.emplace("volume", 0.5f);
		Database<SoundPrototype>::init("tap", sound);
	}
}

TEST_CASE("audio mixes VFS prototype samples with independent overlapping voices and volume", "[audio]") {
	using namespace lf;
	auto bytes = audio_tests::sample_wave();
	auto mounted = fs::mount("/audio-test/tap.wav", fs::borrow(span<u08>(bytes.data(), bytes.size())));
	REQUIRE(mounted);
	audio_tests::initialize_prototypes();
	scope_exit cleanup([] { Database<SoundPrototype>::clear(); Database<SoundGroupPrototype>::clear(); });
	Audio audio(audio_output::offline);
	audio.set_volume(0.5f);
	REQUIRE(audio.play("tap", 0.5f));
	REQUIRE(audio.active_count() == 1);
	vector<f32> output(2048);
	REQUIRE(audio.render(output) == 1024);
	// Input amplitude 1/4, prototype 1/2, group 1/2, play 1/2, master 1/2.
	REQUIRE(output[1024] == Catch::Approx(0.015625f).margin(0.00001f));
	REQUIRE(audio.play("tap", 0.5f));
	REQUIRE(audio.active_count() == 2);
	REQUIRE(audio.render(output) == 1024);
	REQUIRE(output[1024] == Catch::Approx(0.03125f).margin(0.00001f));
	audio.stop_all();
	REQUIRE(audio.active_count() == 0);
	REQUIRE(audio.render(output) == 0);
	REQUIRE(std::ranges::all_of(output, [](f32 sample) { return sample == 0.0f; }));
	REQUIRE(audio.play("tap"));
	vector<f32> tail(16000);
	REQUIRE(audio.render(tail) > 0);
	REQUIRE(audio.active_count() == 0);
	REQUIRE_THROWS(audio.play("missing"));
	REQUIRE_THROWS(audio.set_volume(std::numeric_limits<f32>::quiet_NaN()));
	audio.set_volume(2.0f);
	REQUIRE(audio.get_volume() == 1.0f);
	audio.set_volume(-1.0f);
	REQUIRE(audio.get_volume() == 0.0f);
}

TEST_CASE("audio Lua exposes sound prototypes and per-call playback volume", "[audio]") {
	using namespace lf;
	auto bytes = audio_tests::sample_wave();
	auto mounted = fs::mount("/audio-test/tap.wav", fs::borrow(span<u08>(bytes.data(), bytes.size())));
	REQUIRE(mounted);
	audio_tests::initialize_prototypes();
	scope_exit cleanup([] { Database<SoundPrototype>::clear(); Database<SoundGroupPrototype>::clear(); });
	Audio audio(audio_output::offline);
	auto lua = CreateState();
	InstallAudioScript(lua, audio);
	REQUIRE(lua.safe_script("assert(prototypes.sound.tap.path == '/audio-test/tap.wav'); assert(prototypes.sound.tap.sound_group == 'effects'); assert(prototypes['sound-group'].effects.volume == 0.5); audio.set_volume(0.5); assert(audio.get_volume() == 0.5); assert(audio.play('tap', 0.5))", sol::script_pass_on_error).valid());
	vector<f32> output(2048);
	REQUIRE(audio.render(output) == 1024);
	REQUIRE(output[1024] == Catch::Approx(0.015625f).margin(0.00001f));
	REQUIRE_FALSE(lua.safe_script("audio.play('missing')", sol::script_pass_on_error).valid());
	REQUIRE(lua.safe_script("audio.stop_all(); assert(audio.play('tap'))", sol::script_pass_on_error).valid());
	REQUIRE(audio.active_count() == 1);
}

TEST_CASE("standard mod loader creates sound prototypes and resolves mod sample paths", "[audio][mods]") {
	using namespace lf;
	const auto root = std::filesystem::absolute("audio-mod-test-work");
	REQUIRE(root.parent_path() == std::filesystem::current_path());
	auto storage = fs::native_volume(root, fs::native_volume_options(fs::access_mode::read_write, fs::missing_action::create));
	REQUIRE(storage);
	scope_exit cleanup([&] { std::error_code error; std::filesystem::remove_all(root, error); });
	auto appdata = fs::mount("/", *storage);
	REQUIRE(appdata);
	scope_exit unload([] { Mod::unload(); PrototypeTypeRegistry::functions.clear(); });
	PrototypeTypeRegistry::functions.clear();
	PrototypeTypeRegistry::RegisterType<SoundGroupPrototype>();
	PrototypeTypeRegistry::RegisterType<SoundPrototype>();
	REQUIRE(fs::create_directories("/mods/audio-test/sounds"));
	const string info = "name: audio-test\nmod_version: 1.0.0\n";
	const string data = "data:extend({ {type='sound-group',name='effects',volume=0.5}, {type='sound',name='tap',sound_group='effects',path='/audio-test/sounds/tap.wav',volume=0.5} })";
	REQUIRE(fs::write_all("/mods/audio-test/info.yaml", span<const u08>(reinterpret_cast<const u08*>(info.data()), info.size())));
	REQUIRE(fs::write_all("/mods/audio-test/data.lua", span<const u08>(reinterpret_cast<const u08*>(data.data()), data.size())));
	const auto bytes = audio_tests::sample_wave();
	REQUIRE(fs::write_all("/mods/audio-test/sounds/tap.wav", bytes));
	REQUIRE_FALSE(save_enabled_mods(fs::path{ "/enabled_mods.yaml" }, { { "audio-test", { true, version{ 1, 0, 0 } } } }));
	const Mod::Source source[]{ { .path = "/mods" } };
	Mod::load(source);
	const auto error = Mod::result().get();
	INFO(error.message);
	REQUIRE_FALSE(error);
	REQUIRE(Database<SoundPrototype>::count() == 1);
	REQUIRE(Database<SoundGroupPrototype>::count() == 1);
	Audio audio(audio_output::offline);
	auto lua = CreateState();
	InstallAudioScript(lua, audio);
	REQUIRE(lua.safe_script("assert(prototypes.sound.tap.path == '/audio-test/sounds/tap.wav'); assert(audio.play('tap', 0.5))", sol::script_pass_on_error).valid());
	vector<f32> output(2048);
	REQUIRE(audio.render(output) == 1024);
	REQUIRE(output[1024] == Catch::Approx(0.03125f).margin(0.00001f));
	audio.stop_all();
	Mod::unload();
	REQUIRE(Database<SoundPrototype>::count() == 0);
}

TEST_CASE("sound prototypes preserve defaults and reject invalid authored values", "[audio]") {
	using namespace lf;
	dict group;
	group.emplace("name", "default");
	REQUIRE(SoundGroupPrototype(group).volume == 1.0f);
	group.emplace("volume", -0.5f);
	REQUIRE_THROWS(SoundGroupPrototype(group));
	dict sound;
	sound.emplace("name", "default");
	sound.emplace("path", "/audio-test/default.wav");
	REQUIRE(SoundPrototype(sound).volume == 1.0f);
	sound.emplace("sound_group", "missing");
	REQUIRE_THROWS(SoundPrototype(sound));
	sound.erase("sound_group");
	sound.emplace("volume", std::numeric_limits<f32>::infinity());
	REQUIRE_THROWS(SoundPrototype(sound));
}
