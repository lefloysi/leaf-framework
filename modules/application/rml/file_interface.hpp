#pragma once

#include "leaf/core/types.hpp"
#include "leaf/core/vector.hpp"

#include <RmlUi/Core/FileInterface.h>

namespace lf {
	class RmlFile final : public Rml::FileInterface {
	  public:
		Rml::FileHandle Open(const Rml::String& path) override;
		void Close(Rml::FileHandle file) override;
		size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
		bool Seek(Rml::FileHandle file, long offset, int origin) override;
		size_t Tell(Rml::FileHandle file) override;

	  private:
		struct File {
			vector<u08> bytes;
			usize cursor = 0;
		};
	};
} // namespace lf
