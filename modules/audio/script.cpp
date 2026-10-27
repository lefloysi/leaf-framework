#include "leaf/audio/audio.hpp"

#include "leaf/script/prototype.hpp"

namespace lf {
	void InstallAudioScript(sol::state& lua, Audio& audio) {
		ExportPrototypeTable<SoundGroupPrototype>(lua);
		ExportPrototypeTable<SoundPrototype>(lua);
		auto interface = lua.create_named_table("audio");
		interface.set_function("play", [&audio](string_view name, sol::optional<f32> volume) { return audio.play(name, volume.value_or(1.0f)); });
		interface.set_function("set_volume", [&audio](f32 volume) { audio.set_volume(volume); });
		interface.set_function("get_volume", [&audio] { return audio.get_volume(); });
		interface.set_function("stop_all", [&audio] { audio.stop_all(); });
		interface.set_function("available", [&audio] { return audio.available(); });
		interface.set_function("refresh_prototypes", [&lua] {
			ExportPrototypeTable<SoundGroupPrototype>(lua);
			ExportPrototypeTable<SoundPrototype>(lua);
		});
	}
} // namespace lf
