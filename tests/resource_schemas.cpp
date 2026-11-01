#include <leaf/resource/prototypes/cursor.hpp>
#include <leaf/resource/prototypes/sound.hpp>
#include <leaf/resource/prototypes/texture.hpp>
#include <leaf/resource/prototypes/icon.hpp>
#include <type_traits>

template<class T>
concept world_sized_image = requires(T& value) { value.distance; };

static_assert(!std::is_same_v<lf::IconPrototype::ID, lf::TexturePrototype::ID>);
static_assert(!world_sized_image<lf::IconPrototype>);
static_assert(world_sized_image<lf::TexturePrototype>);
static_assert(requires(lf::IconPrototype& value) { lf::schema_trait<lf::IconPrototype>::get(value); });

static_assert(requires(lf::TexturePrototype& value) { lf::schema_trait<lf::TexturePrototype>::get(value); });
static_assert(requires(lf::CursorPrototype& value) { lf::schema_trait<lf::CursorPrototype>::get(value); });
static_assert(requires(lf::SoundPrototype& value) { lf::schema_trait<lf::SoundPrototype>::get(value); });
static_assert(requires(lf::SoundGroupPrototype& value) { lf::schema_trait<lf::SoundGroupPrototype>::get(value); });
