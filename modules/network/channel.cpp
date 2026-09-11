#include "leaf/network/channel.hpp"
#include "leaf/core/exception.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace lf::net {
	namespace {
		constexpr u32 PROTOCOL = 0x09464c43;
		constexpr auto RETRY = duration::from_quantum(50'000'000);
		constexpr auto HEARTBEAT = duration::from_quantum(500'000'000);

		void check(error error) {
			if (error) { throw runtime_exception(error.message); }
		}
	}

	Channel::Channel(Peer peer, u64 id) : endpoint(std::move(peer)), identity(id) {}

	report<Channel::Datagram> Channel::read(span<const byte> bytes) {
		bin::read_stream stream{ bytes };
		u32 protocol = 0;
		Datagram packet;
		if (auto error = stream(field("protocol", protocol), field("channel", packet.channel),
			field("offset", packet.offset), field("acknowledged", packet.acknowledged))) {
			return unexpected(error);
		}
		if (protocol != PROTOCOL || !packet.channel || stream.remaining() > PACKET_BYTES) {
			return unexpected(error{ generic_errc::input_error, "Invalid channel packet" });
		}
		packet.bytes = bytes.last(stream.remaining());
		return packet;
	}

	void Channel::send(span<const byte> message) {
		std::array<byte, sizeof(u64)> header;
		bin::fixed_write_stream stream{ header };
		const u64 size = message.size();
		check(stream(field("size", size)));
		outgoing.insert(outgoing.end(), header.begin(), header.end());
		outgoing.insert(outgoing.end(), message.begin(), message.end());
	}

	error Channel::receive(const Datagram& packet) {
		if (packet.channel != identity || packet.acknowledged > transmitted) {
			return { generic_errc::input_error, "Invalid channel acknowledgement" };
		}
		activity = now();
		acknowledged = std::max(acknowledged, packet.acknowledged);
		if (packet.bytes.empty()) { return {}; }
		acknowledgement_due = true;
		if (packet.offset < received || packet.offset - received >= WINDOW_BYTES) { return {}; }
		if (packet.offset != received) {
			reordered.try_emplace(packet.offset, packet.bytes.begin(), packet.bytes.end());
			return {};
		}
		incoming.insert(incoming.end(), packet.bytes.begin(), packet.bytes.end());
		received += packet.bytes.size();
		for (auto found = reordered.find(received); found != reordered.end(); found = reordered.find(received)) {
			incoming.insert(incoming.end(), found->second.begin(), found->second.end());
			received += found->second.size();
			reordered.erase(found);
		}
		reordered.erase(reordered.begin(), reordered.lower_bound(received));
		return assemble();
	}

	error Channel::assemble() {
		usize consumed = 0;
		while (incoming.size() - consumed >= sizeof(u64)) {
			bin::read_stream stream{ span<const byte>{ incoming }.subspan(consumed) };
			u64 size = 0;
			if (auto error = stream(field("size", size))) { return error; }
			if (size > std::numeric_limits<usize>::max() - sizeof(u64)) {
				return { generic_errc::input_error, "Message size is not representable" };
			}
			if (size > stream.remaining()) { break; }
			const auto begin = incoming.begin() + consumed + sizeof(u64);
			messages.emplace_back(begin, begin + usize(size));
			consumed += sizeof(u64) + usize(size);
		}
		if (consumed) { incoming.erase(incoming.begin(), incoming.begin() + consumed); }
		return {};
	}

	void Channel::transmit(Socket& socket, u64 offset, span<const byte> bytes) {
		std::array<byte, PACKET_BYTES + 32> buffer;
		bin::fixed_write_stream stream{ buffer };
		check(stream(field("protocol", PROTOCOL), field("channel", identity),
			field("offset", offset), field("acknowledged", received)));
		check(stream.bytes(bytes.data(), bytes.size()));
		socket.send(endpoint, stream.written());
		acknowledgement_due = false;
	}

	void Channel::update(Socket& socket) {
		const auto time = now();
		const u64 end = outgoing_base + outgoing.size();
		u64 offset = transmitted;
		if (time >= retry) {
			offset = acknowledged;
			retry = time + RETRY;
		}
		const u64 limit = std::min(end, acknowledged + WINDOW_BYTES);
		while (offset < limit) {
			const usize count = usize(std::min<u64>(PACKET_BYTES, limit - offset));
			transmit(socket, offset, span<const byte>{ outgoing }.subspan(usize(offset - outgoing_base), count));
			offset += count;
			transmitted = std::max(transmitted, offset);
		}
		if (acknowledgement_due || time >= heartbeat) {
			transmit(socket, transmitted, {});
			heartbeat = time + HEARTBEAT;
		}
		if (acknowledged == end || acknowledged - outgoing_base >= WINDOW_BYTES) {
			outgoing.erase(outgoing.begin(), outgoing.begin() + usize(acknowledged - outgoing_base));
			outgoing_base = acknowledged;
		}
	}

	vector<vector<byte>> Channel::take_messages() { return std::exchange(messages, {}); }
	const Peer& Channel::peer() const { return endpoint; }
	u64 Channel::id() const { return identity; }
	instant Channel::last_received() const { return activity; }
	u64 Channel::sent_bytes() const { return outgoing_base + outgoing.size(); }
	u64 Channel::acknowledged_bytes() const { return acknowledged; }
	u64 Channel::received_message_bytes() const {
		return incoming.size() > sizeof(u64) ? incoming.size() - sizeof(u64) : 0;
	}
	u64 Channel::incoming_message_bytes() const {
		if (incoming.size() < sizeof(u64)) { return 0; }
		bin::read_stream stream{ incoming };
		u64 size = 0;
		check(stream(field("size", size)));
		return size;
	}
}
