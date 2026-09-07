#include "leaf/resource/prototypes/sound.hpp"

namespace lf {
	SoundPrototype::SoundPrototype(const dict& data) : Prototype{ data } {
		data.assign(schema(*this));
	}

} // namespace lf

