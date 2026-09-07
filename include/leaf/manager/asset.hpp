#pragma once

#include <leaf/core/filesystem.hpp>
#include <leaf/core/identifier.hpp>
#include <leaf/core/math/rect.hpp>
#include <leaf/core/optional.hpp>
#include <leaf/core/progress.hpp>
#include <leaf/core/vector.hpp>
#include <leaf/graphics/graphics_program.hpp>
#include <leaf/graphics/resource.hpp>

namespace lf::asset {
	enum class lifetime { shared,
						  startup,
						  menu,
						  session };
	enum class state { idle,
					   loading,
					   ready,
					   failed,
					   cancelled };

	struct image {
		using ID = identifier<image, u32, u32>;

		struct description {
			fs::path source;
			optional<rect<u32>> crop;
			lifetime storage{ lifetime::shared };
		};
	};

	struct image_view {
		rt::view<rt::texture_view> texture;
		rt::view<rt::sampler> sampler;
		rect<f32> region;
		dim2<u32> size;
	};

	struct shader {
		using ID = identifier<shader, u32, u32>;

		struct description {
			fs::path source;
			vector<u08> bytes;
			string entry_point;
			vector<rt::vertex_input> inputs;
			rt::cull_mode cull{ rt::cull_mode::none };
			rt::front_face front{ rt::front_face::ccw };
			rt::fill_mode fill{ rt::fill_mode::solid };
			bool blend_enabled{};
			rt::blend_factor source_color{ rt::blend_factor::one };
			rt::blend_factor destination_color{ rt::blend_factor::zero };
			rt::blend_op color_operation{ rt::blend_op::add };
			rt::blend_factor source_alpha{ rt::blend_factor::one };
			rt::blend_factor destination_alpha{ rt::blend_factor::zero };
			rt::blend_op alpha_operation{ rt::blend_op::add };

			description() = default;
			description(fs::path source, string_view entry_point);
			description(rt::program_bytes bytes, string_view entry_point);
			void set_embedded(rt::program_bytes bytes);
		};
	};

	class group {
	  public:
		group() = default;
		~group();
		group(const group&) = delete;
		group& operator=(const group&) = delete;

		void include(image::ID image);
		void include(shader::ID shader);

	  private:
		friend void load(group& group, Progress progress);
		friend void unload(group& group);
		friend state poll(group& group);
		friend error failure(const group& group);

		vector<image::ID> image_declarations;
		vector<image::ID> image_demands;
		vector<image::ID> completed_images;
		vector<shader::ID> shader_declarations;
		vector<shader::ID> shader_demands;
		vector<shader::ID> completed_shaders;
		optional<Progress> progress;
		state status{ state::idle };
		error result;
	};

	error init(usize workers);
	void exit();

	image::ID add(image::description description);
	report<shader::ID> add(shader::description description);

	void load(group& group, Progress progress);
	void unload(group& group);
	state poll(group& group);
	error failure(const group& group);
	optional<image_view> get(image::ID image);
	optional<rt::view<rt::program>> get(shader::ID shader);

	void update(rt::view<rt::command_buffer> commands, usize maximum_images = 1);
	void submitted(rt::timepoint completion);
	void collect();
	usize resident_pages();
} // namespace lf::asset
