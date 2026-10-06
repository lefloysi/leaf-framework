#pragma once

#include "leaf/core/exception.hpp"
#include "leaf/lockstep/session.hpp"

namespace lf::lockstep {
	template<bin::byte_stream Stream, bin::data<Session::Input> Value>
	error process(Stream& stream, Value& value) {
		return stream(field("id", value.id), field("source", value.source), field("bytes", value.bytes));
	}

	template<bin::byte_stream Stream, bin::data<Session::Frame> Value>
	error process(Stream& stream, Value& value) {
		return stream(field("tick", value.tick), field("inputs", value.inputs));
	}
} // namespace lf::lockstep

namespace lf::lockstep::protocol {
	enum class Type : u08 { join,
							welcome,
							snapshot_begin,
							snapshot,
							loaded,
							input,
							frame,
							disconnect };

	void check(error error);

	template<typename... Fields>
	void send(net::Channel& channel, Type type, Fields&&... fields) {
		bin::write_stream stream;
		check(stream(field("type", type), std::forward<Fields>(fields)...));
		channel.send(stream.written());
	}

	void send_bytes(net::Channel& channel, Type type, span<const byte> bytes);

	bool same_peer(const net::Peer& a, const net::Peer& b);
} // namespace lf::lockstep::protocol
