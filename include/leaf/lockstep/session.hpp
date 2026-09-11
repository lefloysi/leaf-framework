#pragma once

#include "leaf/core/identifier.hpp"
#include "leaf/network/channel.hpp"
#include <functional>

namespace lf::lockstep {
	struct Options {
		u16 max_clients = 0;
		duration connect_timeout = duration::from_quantum(15'000'000'000);
	};

	class Session {
	 public:
		using ID = identifier<Session, u64, void>;

		struct Input {
			using ID = identifier<Input, u64, void>;
			ID id;
			Session::ID source;
			vector<byte> bytes;
		};

		struct Frame {
			u64 tick = 0;
			vector<Input> inputs;
		};

		struct Event {
			enum class Type { login_requested, snapshot_requested, snapshot_received, peer_disconnected, disconnected };
			Type type;
			ID session_id;
			vector<byte> bytes;
		};

		enum class State { connecting, logging_in, accepting, preparing_snapshot, downloading_snapshot, catching_up, joined, disconnected };

		virtual ~Session();
		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
		virtual void update() = 0;
		virtual void advance() = 0;
		virtual void disconnect() = 0;
		virtual Input::ID submit(span<const byte> bytes) = 0;

		vector<Frame> take_ready_ticks();
		vector<Event> take_events();
		ID local_session_id() const;
		u64 tick() const;
		State state() const;
		bool joined() const;

	 protected:
		Session(ID owner, State state);
		Input make_input(span<const byte> bytes);

		ID owner;
		State status;
		u64 current_tick = 0;
		Input::ID next_input{ 0 };
		vector<Frame> ready;
		vector<Event> events;
	};

	class LocalSession final : public Session {
	 public:
		LocalSession();
		void update() override;
		void advance() override;
		void disconnect() override;
		Input::ID submit(span<const byte> bytes) override;

	 private:
		vector<Input> inputs;
	};

	class HostSession final : public Session {
	 public:
		struct SnapshotProgress {
			ID session_id;
			u64 bytes_done = 0;
			u64 bytes_total = 0;
		};

		HostSession(net::Socket socket, Options options = {});
		~HostSession() override;
		void set_input_handler(std::function<report<vector<byte>>(ID, span<const byte>)> handler);
		void accept_login(ID player);
		void send_snapshot(ID player, span<const byte> snapshot);
		void reject_login(ID player);
		void disconnect_peer(ID player);
		vector<SnapshotProgress> outgoing_snapshots() const;
		void update() override;
		void advance() override;
		void disconnect() override;
		Input::ID submit(span<const byte> bytes) override;

	 private:
		struct Connection {
			Connection(net::Peer peer, u64 channel, ID player);
			net::Channel channel;
			ID player;
			State state = State::connecting;
			u64 acceptance = 0;
			struct Upload {
				u64 begin;
				u64 end;
			};
			optional<Upload> snapshot;
		};

		void receive(Connection& connection, span<const byte> bytes);
		void close(Connection& connection);

		net::Socket socket;
		Options options;
		vector<Connection> connections;
		vector<Input> inputs;
		std::function<report<vector<byte>>(ID, span<const byte>)> input_handler;
	};

	class RemoteSession final : public Session {
	 public:
		struct SnapshotProgress {
			u64 bytes_done = 0;
			u64 bytes_total = 0;
		};

		RemoteSession(net::Socket socket, net::Peer host, Options options = {});
		~RemoteSession() override;
		void set_login_payload(span<const byte> bytes);
		void finish_snapshot_load();
		optional<SnapshotProgress> incoming_snapshot() const;
		f64 connection_wait() const;
		void update() override;
		void advance() override;
		void disconnect() override;
		Input::ID submit(span<const byte> bytes) override;

	 private:
		void receive(vector<byte> bytes);
		void close();
		void drain();

		net::Socket socket;
		net::Channel channel;
		Options options;
		instant started = now();
		optional<u64> snapshot_size;
		vector<Frame> buffered;
	};
}
