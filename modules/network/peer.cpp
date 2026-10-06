#include "leaf/network/peer.hpp"

#include <charconv>
#include <limits>
#include <system_error>

namespace lf::net {
	Peer Peer::Address(string_view host, u16 port) {
		return { string(host), port };
	}

	report<Peer> Peer::ParseAddress(string_view endpoint) {
		const size_t separator = endpoint.rfind(':');
		if (separator == string_view::npos || separator == 0 || separator + 1 == endpoint.size()) {
			return unexpected(error(generic_errc::parse_error, "endpoint must have the form host:port"));
		}
		const string_view host = endpoint.substr(0, separator);
		const string_view port_text = endpoint.substr(separator + 1);
		unsigned parsed_port = 0;
		const auto [end, parse_error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), parsed_port);
		if (parse_error != std::errc{} || end != port_text.data() + port_text.size() || parsed_port == 0 || parsed_port > std::numeric_limits<u16>::max()) {
			return unexpected(error(generic_errc::parse_error, "endpoint port must be an integer from 1 to 65535"));
		}
		return Address(host, static_cast<u16>(parsed_port));
	}

	Peer Peer::User(const store::User& user, u16 channel) {
		return { user.id, channel };
	}

	Peer::Peer(string_view host, u16 port) : m_id(host), m_channel(port) {}

	string_view Peer::id() const {
		return m_id;
	}

	u16 Peer::channel() const {
		return m_channel;
	}
} // namespace lf::net
