#include "protocol.hpp"
#include "leaf/core/random.hpp"
#include <algorithm>
#include <array>

namespace lf::lockstep {
	RemoteSession::RemoteSession(net::Socket socket, net::Peer host, Options options)
		: Session(ID::null, State::connecting), socket(std::move(socket)),
		  channel(std::move(host), random_seed() | 1), options(options) {}

	RemoteSession::~RemoteSession() = default;

	void RemoteSession::set_login_payload(span<const byte> bytes) {
		if (status != State::connecting) { throw runtime_exception("Login already submitted"); }
		protocol::send_bytes(channel, protocol::Type::join, bytes);
		status = State::logging_in;
	}

	Session::Input::ID RemoteSession::submit(span<const byte> bytes) {
		if (!joined()) { throw runtime_exception("Session is not ready for input"); }
		const auto id = next_input;
		next_input = Input::ID::from_raw(id.get() + 1);
		bin::write_stream stream;
		const auto type = protocol::Type::input;
		protocol::check(stream(field("type", type), field("id", id)));
		protocol::check(stream.bytes(bytes.data(), bytes.size()));
		channel.send(stream.written());
		return id;
	}

	void RemoteSession::close() {
		if (status == State::disconnected) { return; }
		status = State::disconnected;
		events.push_back(Event{ Event::Type::disconnected, owner, {} });
	}

	void RemoteSession::disconnect() {
		if (status == State::disconnected) { return; }
		protocol::send(channel, protocol::Type::disconnect);
		channel.update(socket);
		close();
	}

	void RemoteSession::receive(vector<byte> bytes) {
		bin::read_stream stream{ bytes };
		protocol::Type type;
		if (stream(field("type", type))) { close(); return; }
		switch (type) {
		case protocol::Type::welcome:
			if (status != State::logging_in || stream(field("player", owner)) || stream.remaining() || !owner) {
				close(); return;
			}
			status = State::preparing_snapshot;
			break;
		case protocol::Type::snapshot_begin: {
			if (status != State::preparing_snapshot) { close(); return; }
			u64 size = 0;
			if (stream(field("tick", current_tick), field("size", size)) || stream.remaining()) { close(); return; }
			snapshot_size = size;
			status = State::downloading_snapshot;
			break;
		}
		case protocol::Type::snapshot:
			if (status != State::downloading_snapshot || !snapshot_size || stream.remaining() != *snapshot_size) {
				close(); return;
			}
			bytes.erase(bytes.begin(), bytes.end() - stream.remaining());
			events.push_back(Event{ Event::Type::snapshot_received, owner, std::move(bytes) });
			break;
		case protocol::Type::frame: {
			Frame frame;
			if (stream(field("frame", frame)) || stream.remaining()) { close(); return; }
			buffered.push_back(std::move(frame));
			break;
		}
		case protocol::Type::disconnect:
			close();
			break;
		default:
			close();
			break;
		}
	}

	void RemoteSession::finish_snapshot_load() {
		if (status != State::downloading_snapshot) { throw runtime_exception("No snapshot to finish loading"); }
		snapshot_size.reset();
		status = State::catching_up;
		protocol::send(channel, protocol::Type::loaded);
		drain();
	}

	void RemoteSession::drain() {
		if (status != State::catching_up && status != State::joined) { return; }
		for (auto& frame : buffered) {
			if (frame.tick != current_tick + 1) { close(); return; }
			current_tick = frame.tick;
			ready.push_back(std::move(frame));
			status = State::joined;
		}
		buffered.clear();
	}

	void RemoteSession::advance() { drain(); }

	void RemoteSession::update() {
		if (status == State::disconnected) { return; }
		std::array<byte, 65507> buffer;
		for (usize index = 0; index < 512; ++index) {
			const auto message = socket.recv(buffer);
			if (!message) { break; }
			if (!protocol::same_peer(message->first, channel.peer())) { continue; }
			const auto packet = net::Channel::read(message->second);
			if (!packet || packet->channel != channel.id()) { continue; }
			if (channel.receive(*packet)) { close(); return; }
			for (auto& bytes : channel.take_messages()) {
				receive(std::move(bytes));
				if (status == State::disconnected) { return; }
			}
		}
		if (status != State::preparing_snapshot && now() - channel.last_received() >= options.connect_timeout) { close(); return; }
		channel.update(socket);
		drain();
	}

	optional<RemoteSession::SnapshotProgress> RemoteSession::incoming_snapshot() const {
		if (!snapshot_size) { return {}; }
		const auto received = channel.received_message_bytes();
		return SnapshotProgress{ std::min(*snapshot_size, received ? received - 1 : 0), *snapshot_size };
	}

	f64 RemoteSession::connection_wait() const {
		const auto timeout = options.connect_timeout.quantum_count();
		if (timeout <= 0) { return 1; }
		return std::clamp(f64((now() - started).quantum_count()) / f64(timeout), 0.0, 1.0);
	}
}
