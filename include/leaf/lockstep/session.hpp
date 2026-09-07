#pragma once

#include "leaf/core/memory.hpp"
#include "leaf/core/optional.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/time.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/network/peer.hpp"
#include "leaf/network/socket.hpp"

namespace lf::lockstep {
	using Tick = u64;
	using SessionId = u64;
	using PayloadId = u64;
	using PacketSequence = u64;

	enum class mode : u08 {
		offline = 0,
		host = 1,
		client = 2,
	};

	enum class state : u08 {
		connecting = 0,
		logging_in = 1,
		downloading_snapshot = 2,
		catching_up = 3,
		joined = 4,
		disconnected = 5,
	};

	struct Command {
		PayloadId id = 0;
		SessionId source = 0;
		u64 hash = 0;
		vector<byte> bytes;
	};

	struct ReadyTick {
		Tick tick = 0;
		vector<Command> commands;
	};

	struct PendingPayload {
		PayloadId id = 0;
		u64 hash = 0;
		span<const byte> bytes;
	};

	enum class SessionEventKind : u08 {
		login_requested = 1,
		snapshot_received = 2,
		peer_disconnected = 3,
		disconnected = 4,
	};

	struct SessionEvent {
		SessionEventKind kind = SessionEventKind::disconnected;
		SessionId session_id = 0;
		Tick tick = 0;
		vector<byte> bytes;
	};

	// Snapshot transfer progress for one peer. Host: one per client that's
	// currently downloading the map. Client: at most one (the incoming
	// transfer from the host). Used by the Factorio-style "Saving the world
	// for <name>" host overlay and the "Downloading map" client loading bar
	// to display real, measured progress rather than a hardcoded fraction.
	struct SnapshotProgress {
		SessionId session_id = 0;
		u32 chunks_done = 0;
		u32 chunks_total = 0;
		u64 bytes_done = 0;
		u64 bytes_total = 0;
	};

	struct Options {
		Tick initial_tick{};
		std::function<instant()> clock{ lf::now };
		u64 max_snapshot_bytes{ 512 * 1024 * 1024 };
		u32 max_buffered_ticks{ 1800 };
		u32 max_command_bytes{ 128 * 1024 };
		u16 snapshot_chunk_bytes = 1000;
		u16 max_clients = 0;
		duration handshake_interval = duration::from_quantum(500'000'000);
		duration nack_interval = duration::from_quantum(50'000'000);
		duration connect_timeout = duration::from_quantum(15'000'000'000);
		u32 max_missing_per_nack = 64;
		u32 heartbeat_request_min_delay = 3;
		u32 heartbeat_request_max_delay = 32;
		u32 heartbeat_request_interval = 10;
		u32 max_resends_per_update = 128;
		u32 max_outgoing_packets_per_update = 128;
		u32 max_incoming_packets_per_update = 128;
		u32 max_snapshot_chunks_per_update = 16;
		u32 snapshot_request_window = 32;
		u32 snapshot_request_retry_delay = 4;
		u32 command_latency_ticks = 2;
		u32 max_tick_steps_per_update = 4;
	};

	class Session {
	  public:
		struct Impl;
		struct ImplDeleter {
			void operator()(Impl* impl) const noexcept;
		};

		Session() = default;

		static Session Offline(Options options = {});
		static Session Host(net::Socket socket, Options options = {});
		static Session Client(net::Socket socket, net::Peer host, Options options = {});

		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
		Session(Session&& other) noexcept;
		Session& operator=(Session&& other) noexcept;
		~Session();

		explicit operator bool() const noexcept;

		void update();
		void advance();
		void disconnect();
<<<<<<< Updated upstream
		void disconnect_peer(SessionId session_id);
		PayloadId submit(span<const byte> bytes);
		vector<ReadyTick> take_ready_ticks();
		vector<SessionEvent> take_events();
		void set_login_payload(span<const byte> bytes);
		void accept_login(SessionId session_id, span<const byte> snapshot);
		void reject_login(SessionId session_id);
=======
		void disconnect_peer(ID session_id);
		identifier<Command, u64, void> submit(span<const byte> bytes);
		void set_command_admitter(std::function<report<vector<byte>>(ID, span<const byte>)> admitter);
		identifier<Command, u64, void> submit_authoritative(span<const byte> bytes);
		vector<ReadyTick> take_ready_ticks();
		vector<SessionEvent> take_events();
		void set_login_payload(span<const byte> bytes);
		void accept_login(ID session_id, Tick baseline, span<const byte> snapshot);
		void reject_login(ID session_id, string_view reason = "login rejected");
>>>>>>> Stashed changes
		void finish_snapshot_load();

		lockstep::mode mode() const;
		lockstep::state state() const;
		SessionId local_session_id() const;
		Tick tick() const;
		bool joined() const;
		bool waiting_for_response() const;
		span<const PendingPayload> pending() const;

		// Snapshot transfer progress. Client variant returns the in-flight
		// download if one is active. Host variant returns one entry per
		// peer that's currently in downloading_snapshot — drives the
		// "Saving the world for <name>" overlay during a join handshake.
		optional<SnapshotProgress> incoming_snapshot() const;
		vector<SnapshotProgress> outgoing_snapshots() const;

	  private:
		explicit Session(unique_ptr<Impl> impl);

		std::unique_ptr<Impl, ImplDeleter> impl;
	};
<<<<<<< Updated upstream
=======

	struct Command {
		using ID = identifier<Command, u64, void>;

		ID id{};
		Session::ID source{};
		u64 hash = 0;
		vector<byte> bytes;
	};

	struct ReadyTick {
		Tick tick = 0;
		vector<Command> commands;
	};

	struct PendingPayload {
		Command::ID id{};
		u64 hash = 0;
		span<const byte> bytes;
	};

	enum class SessionEventKind : u08 {
		login_requested = 1,
		snapshot_received = 2,
		peer_disconnected = 3,
		disconnected = 4,
		command_rejected = 5,
	};

	struct SessionEvent {
		SessionEventKind kind = SessionEventKind::disconnected;
		Session::ID session_id{};
		Tick tick = 0;
		vector<byte> bytes;
		Command::ID command_id{};
		string message;
	};

	struct SnapshotProgress {
		Session::ID session_id{};
		u32 chunks_done = 0;
		u32 chunks_total = 0;
		u64 bytes_done = 0;
		u64 bytes_total = 0;
	};
>>>>>>> Stashed changes
} // namespace lf::lockstep
