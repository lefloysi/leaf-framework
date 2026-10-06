#include "protocol.hpp"

namespace lf::lockstep::protocol {
	void check(error error) {
		if (error) {
			throw runtime_exception(error.message);
		}
	}

	void send_bytes(net::Channel& channel, Type type, span<const byte> bytes) {
		bin::write_stream stream;
		check(stream(field("type", type)));
		check(stream.bytes(bytes.data(), bytes.size()));
		channel.send(stream.written());
	}

	bool same_peer(const net::Peer& a, const net::Peer& b) {
		return a.id() == b.id() && a.channel() == b.channel();
	}
} // namespace lf::lockstep::protocol
