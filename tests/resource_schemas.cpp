#include <leaf/resource/prototypes/cursor.hpp>
#include <leaf/resource/prototypes/sound.hpp>
#include <leaf/resource/prototypes/texture.hpp>

static_assert(requires(lf::TexturePrototype& value) { lf::schema_trait<lf::TexturePrototype>::get(value); });
static_assert(requires(lf::CursorPrototype& value) { lf::schema_trait<lf::CursorPrototype>::get(value); });
static_assert(requires(lf::SoundPrototype& value) { lf::schema_trait<lf::SoundPrototype>::get(value); });
static_assert(requires(lf::SoundGroupPrototype& value) { lf::schema_trait<lf::SoundGroupPrototype>::get(value); });
