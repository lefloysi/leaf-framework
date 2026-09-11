#include "protocol.hpp"
#include <algorithm>
#include <array>

namespace lf::lockstep {
	HostSession::Connection::Connection(net::Peer peer, u64 channel, ID player)
		: channel(std::move(peer), channel), player(player) {}

	HostSession::HostSession(net::Socket socket, Options options)
		: Session(ID{ 0 }, State::joined), socket(std::move(socket)), options(options) {}

	HostSession::~HostSession() = default;

	void HostSession::set_input_handler(std::function<report<vector<byte>>(ID, span<const byte>)> handler) {
		input_handler = std::move(handler);
	}

	Session::Input::ID HostSession::submit(span<const byte> bytes) {
		inputs.push_back(make_input(bytes));
		return inputs.back().id;
	}

	void HostSession::accept_login(ID player) {
		for (auto& connection : connections) {
			if (connection.player != player || connection.state != State::logging_in) { continue; }
			protocol::send(connection.channel, protocol::Type::welcome, field("player", player));
			connection.acceptance = connection.channel.sent_bytes();
			connection.state = State::accepting;
			connection.channel.update(socket);
			return;
		}
	}

	void HostSession::send_snapshot(ID player, span<const byte> snapshot) {
		for (auto& connection : connections) {
			if (connection.player != player || connection.state != State::preparing_snapshot) { continue; }
			const u64 size = snapshot.size();
			protocol::send(connection.channel, protocol::Type::snapshot_begin,
				field("tick", current_tick), field("size", size));
			const auto begin = connection.channel.sent_bytes();
			protocol::send_bytes(connection.channel, protocol::Type::snapshot, snapshot);
			connection.snapshot = Connection::Upload{ begin, connection.channel.sent_bytes() };
			connection.state = State::downloading_snapshot;
			return;
		}
	}

	void HostSession::reject_login(ID player) { disconnect_peer(player); }

	void HostSession::close(Connection& connection) {
		if (connection.state == State::disconnected) { return; }
		protocol::send(connection.channel, protocol::Type::disconnect);
		connection.channel.update(socket);
		connection.state = State::disconnected;
		connection.snapshot.reset();
		events.push_back(Event{ Event::Type::peer_disconnected, connection.player, {} });
	}

	void HostSession::disconnect_peer(ID player) {
		for (auto& connection : connections) {
			if (connection.player == player) { close(connection); return; }
		}
	}

	void HostSession::disconnect() {
		if (status == State::disconnected) { return; }
		for (auto& connection : connections) { close(connection); }
		status = State::disconnected;
	}

	void HostSession::receive(Connection& connection, span<const byte> bytes) {
		bin::read_stream stream{ bytes };
		protocol::Type type;
		if (stream(field("type", type))) { close(connection); return; }
		switch (type) {
		case protocol::Type::join: {
			if (connection.state != State::connecting) { return; }
			const auto active = std::count_if(connections.begin(), connections.end(), [](const auto& peer) {
				return peer.state != State::connecting && peer.state != State::disconnected;
			});
			if (options.max_clients && active >= options.max_clients) { close(connection); return; }
			connection.state = State::logging_in;
			const auto login = bytes.last(stream.remaining());
			events.push_back(Event{ Event::Type::login_requested, connection.player, { login.begin(), login.end() } });
			break;
		}
		case protocol::Type::loaded:
			if (stream.remaining() || connection.state != State::downloading_snapshot) { close(connection); return; }
			connection.state = State::joined;
			connection.snapshot.reset();
			break;
		case protocol::Type::input: {
			if (connection.state != State::joined) { close(connection); return; }
			Input::ID id;
			if (stream(field("id", id)) || !id) { close(connection); return; }
			const auto input = bytes.last(stream.remaining());
			auto approved = input_handler ? input_handler(connection.player, input)
				: report<vector<byte>>{ vector<byte>{ input.begin(), input.end() } };
			if (!approved) { close(connection); return; }
			inputs.push_back(Input{ id, connection.player, std::move(*approved) });
			break;
		}
		case protocol::Type::disconnect:
			close(connection);
			break;
		default:
			close(connection);
			break;
		}
	}

	void HostSession::update() {
		if (status == State::disconnected) { return; }
		std::array<byte, 65507> buffer;
		for (usize index = 0; index < 512; ++index) {
			const auto message = socket.recv(buffer);
			if (!message) { break; }
			const auto packet = net::Channel::read(message->second);
			if (!packet) { continue; }
			auto found = std::find_if(connections.begin(), connections.end(), [&](const auto& connection) {
				return connection.channel.id() == packet->channel && protocol::same_peer(connection.channel.peer(), message->first);
			});
			if (found == connections.end()) {
				if (packet->offset || packet->bytes.empty()) { continue; }
				connections.emplace_back(message->first, packet->channel, ID{ connections.size() + 1 });
				found = connections.end() - 1;
			}
			if (found->state == State::disconnected) { continue; }
			if (found->channel.receive(*packet)) { close(*found); continue; }
			for (const auto& bytes : found->channel.take_messages()) { receive(*found, bytes); }
		}
		for (auto& connection : connections) {
			if (connection.state == State::disconnected) { continue; }
			if (connection.state != State::preparing_snapshot && connection.state != State::downloading_snapshot &&
				now() - connection.channel.last_received() >= options.connect_timeout) { close(connection); continue; }
			connection.channel.update(socket);
			if (connection.state == State::accepting && connection.channel.acknowledged_bytes() >= connection.acceptance) {
				connection.state = State::preparing_snapshot;
				events.push_back(Event{ Event::Type::snapshot_requested, connection.player, {} });
			}
		}
	}

	void HostSession::advance() {
		if (!joined()) { return; }
		std::stable_sort(inputs.begin(), inputs.end(), [](const Input& a, const Input& b) {
			if (a.source != b.source) { return a.source.get() < b.source.get(); }
			return a.id.get() < b.id.get();
		});
		Frame frame{ ++current_tick, std::exchange(inputs, {}) };
		for (auto& connection : connections) {
			if (connection.state == State::downloading_snapshot || connection.state == State::joined) {
				protocol::send(connection.channel, protocol::Type::frame, field("frame", frame));
			}
		}
		ready.push_back(std::move(frame));
	}

	vector<HostSession::SnapshotProgress> HostSession::outgoing_snapshots() const {
		vector<SnapshotProgress> result;
		for (const auto& connection : connections) {
			if (!connection.snapshot) { continue; }
			const auto& upload = *connection.snapshot;
			const auto acknowledged = std::clamp(connection.channel.acknowledged_bytes(), upload.begin, upload.end);
			result.push_back({ connection.player, acknowledged - upload.begin, upload.end - upload.begin });
		}
		return result;
	}
}
