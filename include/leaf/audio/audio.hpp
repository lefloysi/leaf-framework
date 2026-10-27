#pragma once

#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/span.hpp"
#include "leaf/resource/prototypes/sound.hpp"
#include "leaf/script/state.hpp"

#include <memory>

namespace lf {
	enum class audio_output {
		device,
		offline,
	};

	class Audio final {
	public:
		explicit Audio(audio_output output = audio_output::device);
		~Audio();
		Audio(const Audio&) = delete;
		Audio& operator=(const Audio&) = delete;

		bool play(string_view name, f32 volume = 1.0f);
		bool play(SoundPrototype::ID sound, f32 volume = 1.0f);
		void set_volume(f32 volume);
		f32 get_volume() const;
		void stop_all();
		void update();
		usize active_count() const;
		bool available();
		u64 render(span<f32> interleaved_stereo);

	private:
		struct State;
		std::unique_ptr<State> state;
	};

	void InstallAudioScript(sol::state& lua, Audio& audio);
} // namespace lf
