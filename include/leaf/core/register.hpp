#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/singleton.hpp>
#include <leaf/core/vector.hpp>

#include <functional>
#include <utility>

namespace lf {
	template<typename T>
	class Register : public Singleton<Register<T>> {
	  public:
		using Installer = std::function<error(T&)>;

		static void add(Installer installer) {
			Register::instance().installers.emplace_back(std::move(installer));
		}

		static error install(T& value) {
			for (Installer& installer : Register::instance().installers) {
				if (auto err = installer(value); err) {
					return err;
				}
			}
			return {};
		}

	  private:
		vector<Installer> installers;
	};
} // namespace lf
