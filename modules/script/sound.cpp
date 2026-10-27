#include "leaf/resource/prototypes/sound.hpp"

#include <cmath>

namespace lf {
	SoundGroupPrototype::SoundGroupPrototype(const dict& data) : Prototype{ data } {
		data.assign(schema(*this));
		if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
			throw runtime_exception("sound-group volume must be between zero and one");
		}
	}

	SoundPrototype::SoundPrototype(const dict& data) : Prototype{ data } {
		data.assign(schema(*this));
		if (path.empty()) {
			throw runtime_exception("sound path must not be empty");
		}
		if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
			throw runtime_exception("sound volume must be between zero and one");
		}
		const auto found = data.find("sound_group");
		if (found != data.end() && !found->second.as<string>().empty() && !group) {
			throw runtime_exception("sound references an unknown sound-group");
		}
	}
} // namespace lf
