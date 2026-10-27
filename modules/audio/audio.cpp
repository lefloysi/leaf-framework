#include "leaf/audio/audio.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/filesystem.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/resource/database.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace lf {
	f32 audio_volume(f32 volume) {
		if (!std::isfinite(volume)) {
			throw runtime_exception("sound volume must be finite");
		}
		return std::clamp(volume, 0.0f, 1.0f);
	}

	struct Audio::State {
		struct Voice {
			vector<u08> bytes;
			ma_decoder decoder{};
			ma_sound sound{};
			bool decoded = false;
			bool initialized = false;

			~Voice() {
				if (initialized) {
					ma_sound_uninit(&sound);
				}
				if (decoded) {
					ma_decoder_uninit(&decoder);
				}
			}
		};

		explicit State(audio_output output) : output(output) {
		}
		~State() {
			voices.clear();
			if (status == device_status::ready) {
				ma_engine_uninit(&engine);
			}
		}

		enum class device_status {
			unopened,
			ready,
			unavailable,
		};
		audio_output output;
		device_status status = device_status::unopened;
		ma_engine engine{};
		f32 volume = 1.0f;
		std::vector<std::unique_ptr<Voice>> voices;

		bool open() {
			if (status != device_status::unopened) {
				return status == device_status::ready;
			}
			ma_engine_config config = ma_engine_config_init();
			config.channels = 2;
			config.sampleRate = 48000;
			config.noDevice = output == audio_output::offline;
			const auto result = ma_engine_init(&config, &engine);
			if (result != MA_SUCCESS) {
				status = device_status::unavailable;
				log::Warning("[audio] output unavailable: {}", ma_result_description(result));
				return false;
			}
			status = device_status::ready;
			ma_engine_set_volume(&engine, volume);
			return true;
		}
	};

	Audio::Audio(audio_output output) : state(std::make_unique<State>(output)) {
	}
	Audio::~Audio() = default;

	bool Audio::play(string_view name, f32 volume) {
		const auto id = Database<SoundPrototype>::find(name);
		if (!id) {
			throw runtime_exception(lf::format("unknown sound prototype '{}'", name));
		}
		return play(id, volume);
	}

	bool Audio::play(SoundPrototype::ID sound, f32 volume) {
		const auto& prototype = Database<SoundPrototype>::get(sound);
		f32 gain = audio_volume(volume) * audio_volume(prototype.volume);
		if (prototype.group) {
			gain *= audio_volume(Database<SoundGroupPrototype>::get(prototype.group).volume);
		}
		const auto path = fs::path::parse(prototype.path);
		if (!path) {
			throw runtime_exception(path.error().message);
		}
		auto bytes = fs::read_all(*path);
		if (!bytes) {
			throw runtime_exception(bytes.error().message);
		}

		if (!state->open()) {
			return false;
		}
		update();
		auto voice = std::make_unique<State::Voice>();
		voice->bytes = std::move(*bytes);
		const auto config = ma_decoder_config_init(ma_format_f32, 2, 48000);
		if (ma_decoder_init_memory(voice->bytes.data(), voice->bytes.size(), &config, &voice->decoder) != MA_SUCCESS) {
			throw runtime_exception(lf::format("cannot decode sound '{}'", prototype.path));
		}
		voice->decoded = true;
		if (ma_sound_init_from_data_source(&state->engine, &voice->decoder, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &voice->sound) != MA_SUCCESS) {
			throw runtime_exception(lf::format("cannot initialize sound '{}'", prototype.path));
		}
		voice->initialized = true;
		ma_sound_set_volume(&voice->sound, gain);
		if (ma_sound_start(&voice->sound) != MA_SUCCESS) {
			throw runtime_exception("cannot start sound");
		}
		state->voices.emplace_back(std::move(voice));
		return true;
	}

	void Audio::set_volume(f32 volume) {
		state->volume = audio_volume(volume);
		if (state->status == State::device_status::ready) {
			ma_engine_set_volume(&state->engine, state->volume);
		}
	}

	f32 Audio::get_volume() const {
		return state->volume;
	}

	bool Audio::available() {
		return state->open();
	}

	void Audio::stop_all() {
		state->voices.clear();
	}

	void Audio::update() {
		std::erase_if(state->voices, [](const auto& voice) { return ma_sound_at_end(&voice->sound) == MA_TRUE; });
	}

	usize Audio::active_count() const {
		return std::ranges::count_if(state->voices, [](const auto& voice) { return ma_sound_is_playing(&voice->sound) == MA_TRUE; });
	}

	u64 Audio::render(span<f32> output) {
		if (state->output != audio_output::offline) {
			throw runtime_exception("offline rendering requires offline audio output");
		}
		if (output.size() % 2) {
			throw runtime_exception("offline audio requires interleaved stereo frames");
		}
		if (!state->open()) {
			return 0;
		}
		std::ranges::fill(output, 0.0f);
		ma_uint64 frames = 0;
		if (ma_engine_read_pcm_frames(&state->engine, output.data(), output.size() / 2, &frames) != MA_SUCCESS) {
			throw runtime_exception("offline audio rendering failed");
		}
		update();
		return frames;
	}
} // namespace lf
