#pragma once

#include "leaf/lockstep/session.hpp"
#include "leaf/core/exception.hpp"

namespace lf::lockstep {
	template<bin::byte_stream Stream, bin::data<Session::Input> Value>
	error process(Stream& stream, Value& value) {
		return stream(field("id", value.id), field("source", value.source), field("bytes", value.bytes));
	}

	template<bin::byte_stream Stream, bin::data<Session::Frame> Value>
	error process(Stream& stream, Value& value) {
		return stream(field("tick", value.tick), field("inputs", value.inputs));
	}
}

namespace lf::lockstep::protocol {
	enum class Type : u08 { join, welcome, snapshot_begin, snapshot, loaded, input, frame, disconnect };

	inline void check(error error) {
		if (error) { throw runtime_exception(error.message); }
	}

	template<typename... Fields>
	void send(net::Channel& channel, Type type, Fields&&... fields) {
		bin::write_stream stream;
		check(stream(field("type", type), std::forward<Fields>(fields)...));
		channel.send(stream.written());
	}

	inline void send_bytes(net::Channel& channel, Type type, span<const byte> bytes) {
		bin::write_stream stream;
		check(stream(field("type", type)));
		check(stream.bytes(bytes.data(), bytes.size()));
		channel.send(stream.written());
	}

	inline bool same_peer(const net::Peer& a, const net::Peer& b) {
		return a.id() == b.id() && a.channel() == b.channel();
	}
}
