#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/store/user.hpp"

namespace lf::net {
	class Socket;

	class Peer {
	  public:
		static Peer Address(string_view address, u16 port);
		static report<Peer> ParseAddress(string_view endpoint);
		static Peer User(const store::User& user, u16 channel);

		Peer(string_view address, u16 port);

		string_view id() const;
		u16 channel() const;

	  private:
		friend class Socket;

		string m_id;
		u16 m_channel;
	};
} // namespace lf::net
