#pragma once

#include "leaf/core/binary.hpp"
#include "leaf/core/time.hpp"
#include "leaf/network/socket.hpp"
#include <map>

namespace lf::net {
	// An ordered message stream over a datagram socket. The channel owns bytes
	// until acknowledged; callers only lend their input during send().
	class Channel {
	 public:
		struct Datagram {
			u64 channel = 0;
			u64 offset = 0;
			u64 acknowledged = 0;
			span<const byte> bytes;
		};

		static constexpr usize PACKET_BYTES = 1000;
		static constexpr usize WINDOW_BYTES = 128 * PACKET_BYTES;
		static report<Datagram> read(span<const byte> bytes);

		Channel(Peer peer, u64 id);
		void send(span<const byte> message);
		error receive(const Datagram& datagram);
		void update(Socket& socket);
		vector<vector<byte>> take_messages();
		const Peer& peer() const;
		u64 id() const;
		instant last_received() const;
		u64 received_message_bytes() const;
		u64 incoming_message_bytes() const;
		u64 sent_bytes() const;
		u64 acknowledged_bytes() const;

	 private:
		void transmit(Socket& socket, u64 offset, span<const byte> bytes);
		error assemble();

		Peer endpoint;
		u64 identity;
		vector<byte> outgoing;
		u64 outgoing_base = 0;
		u64 acknowledged = 0;
		u64 transmitted = 0;
		vector<byte> incoming;
		u64 received = 0;
		std::map<u64, vector<byte>> reordered;
		vector<vector<byte>> messages;
		instant activity = now();
		instant retry = now();
		instant heartbeat = now();
		bool acknowledgement_due = false;
	};
}
