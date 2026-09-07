#include "leaf/lockstep/session.hpp"

#include "leaf/core/binary.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/exception.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <thread>
#include <utility>

namespace lf::lockstep {
<<<<<<< Updated upstream
	constexpr u32 protocol_version = 6;
=======
	constexpr u32 protocol_version = 8;
>>>>>>> Stashed changes

	enum class packet_kind : u08 {
		connect_request = 1,
		connect_accept = 2,
		join_request = 3,
		snapshot_begin = 4,
		snapshot_chunk = 5,
		snapshot_end = 6,
		client_heartbeat = 7,
		server_heartbeat = 8,
		snapshot_request = 9,
		disconnect = 10,
		disconnect_ack = 11,
	};

	struct wire_payload {
		PayloadId id = 0;
		SessionId source = 0;
		u64 hash = 0;
		vector<byte> bytes;
	};

	struct tick_closure {
		Tick tick = 0;
		vector<wire_payload> commands;
	};

	struct stored_payload {
		PayloadId id = 0;
		SessionId source = 0;
		u64 hash = 0;
		vector<byte> bytes;
	};

	struct stored_tick {
		Tick tick = 0;
		vector<stored_payload> commands;
	};

	struct scheduled_payload {
		Tick tick = 0;
		stored_payload payload;
	};

	struct sent_packet {
		PacketSequence sequence = 0;
		vector<byte> bytes;
	};

	struct queued_send {
		net::Peer peer;
		vector<byte> bytes;
	};

	struct received_packets {
		PacketSequence next_expected = 1;
		PacketSequence latest_received = 0;
		u32 request_delay = 3;
		vector<PacketSequence> missing;
	};

	struct packet_record {
		bool fresh = false;
		vector<PacketSequence> missing;
	};

	struct heartbeat_request_record {
		PacketSequence sequence = 0;
		PacketSequence local_heartbeat = 0;
	};

	struct request_for_heartbeat {
		vector<PacketSequence> sequences;
	};

	struct transfer_block_requests {
		struct range {
			u32 first = 0;
			u32 count = 0;
		};

		u64 snapshot_id = 0;
		vector<range> ranges;
	};

	struct rejected_command {
		Command::ID id{};
		string message;
	};

	struct client_to_server_heartbeat {
		Tick client_tick = 0;
		request_for_heartbeat requests;
		vector<wire_payload> commands;
		vector<Command::ID> rejection_acks;
	};

	struct server_to_client_heartbeat {
		request_for_heartbeat requests;
		vector<tick_closure> tick_closures;
		vector<rejected_command> rejections;
	};

	struct pending_payload {
		PayloadId id = 0;
		u64 hash = 0;
		vector<byte> bytes;
	};

	struct snapshot_download {
		bool active = false;
		bool end_received = false;
		bool load_started = false;
		u64 snapshot_id = 0;
		Tick baseline_tick = 0;
		u16 chunk_size = 1000;
		u32 chunk_count = 0;
		u32 received_count = 0;
		u64 received_bytes = 0;
		u32 next_request_chunk = 0;
		u32 request_round = 0;
		vector<byte> bytes;
		vector<u08> received_chunks;
		vector<u32> requested_chunks;
	};

	struct host_connection {
		host_connection(const net::Peer& peer, SessionId session_id);

		net::Peer peer;
		SessionId session_id = 0;
		state connection_state = state::logging_in;
		bool snapshot_requested = false;
		bool snapshot_sending = false;
		u64 snapshot_id = 0;
		Tick snapshot_baseline_tick = 0;
		u16 snapshot_chunk_size = 1000;
		u32 snapshot_chunk_count = 0;
		u32 next_snapshot_chunk = 0;
		vector<byte> snapshot_bytes;
		PacketSequence next_send_sequence = 1;
		PacketSequence next_heartbeat_sequence = 1;
		vector<sent_packet> sent_packets;
		received_packets received_heartbeats;
		vector<sent_packet> sent_heartbeats;
		vector<PacketSequence> pending_heartbeat_requests;
		vector<heartbeat_request_record> requested_heartbeats;
		vector<PayloadId> received_payloads;
		bool disconnect_acknowledged = false;
		// True once we've received any client_heartbeat. Clients only start
		// heartbeating once they've decoded the snapshot and reached the
		// joined state on their side, so the absence of one means the client
		// is still receiving / loading the snapshot — even if WE've already
		// finished pushing all the bulk chunks and flipped connection_state
		// to joined. Used by Session::outgoing_snapshots() to keep the host
		// "Saving the world for X" overlay up until the peer is actually
		// playing.
		bool client_heartbeat_seen = false;
		vector<rejected_command> rejections;
		Command::ID highest_payload{};
	};

	struct Session::Impl {
		Impl(lockstep::mode initial_mode, lockstep::state initial_state, Options options);
		virtual ~Impl() = default;

		virtual void update() = 0;
		virtual void advance() = 0;
		virtual void disconnect() = 0;
		virtual PayloadId submit(span<const byte> bytes) = 0;
		virtual bool waiting_for_response() const;

		stored_payload make_payload(span<const byte> bytes, SessionId source, bool track_pending);
		void rebuild_pending_views();
		void push_ready_tick(Tick tick, vector<stored_payload>& commands);
		void push_event(SessionEvent event);

		lockstep::mode session_mode = lockstep::mode::offline;
		lockstep::state session_state = lockstep::state::disconnected;
		Options options;
		Tick current_tick = 0;
		SessionId local_session = 0;
		PayloadId next_payload_id = 1;
		vector<pending_payload> pending_payloads;
		vector<PendingPayload> pending_views;
		vector<ReadyTick> ready_ticks;
		vector<SessionEvent> events;
		vector<byte> login_payload;
<<<<<<< Updated upstream
=======
		std::function<report<vector<byte>>(Session::ID, span<const byte>)> command_admitter;
		bool admit(stored_payload& payload);
>>>>>>> Stashed changes
	};

	struct offline_session final : Session::Impl {
		explicit offline_session(Options options);

		void update() override;
		void advance() override;
		void disconnect() override;
		PayloadId submit(span<const byte> bytes) override;

		vector<stored_payload> staged_commands;
	};

	struct host_session final : Session::Impl {
		host_session(net::Socket socket, Options options);

		void update() override;
		void advance() override;
		void disconnect() override;
		PayloadId submit(span<const byte> bytes) override;
		void poll_socket();
		void receive_message(const net::Message& message);
		host_connection& find_or_create_connection(const net::Peer& peer);
		host_connection* find_connection(const net::Peer& peer, SessionId session_id);
		void send_connect_accept(host_connection& connection);
<<<<<<< Updated upstream
		void accept_login(SessionId session_id, span<const byte> snapshot);
		void reject_login(SessionId session_id);
		void disconnect_peer(SessionId session_id);
=======
		void accept_login(Session::ID session_id, Tick baseline, span<const byte> snapshot);
		void reject_login(Session::ID session_id, string_view reason = "login rejected");
		void disconnect_peer(Session::ID session_id);
>>>>>>> Stashed changes
		void continue_snapshots();
		void begin_snapshot(host_connection& connection);
		void continue_snapshot(host_connection& connection);
		void send_snapshot_chunk(host_connection& connection, u32 chunk_index);
		void send_snapshot_end(host_connection& connection);
		void resend_snapshot_chunks(host_connection& connection, const transfer_block_requests& requests);
		void request_heartbeats(host_connection& connection, vector<PacketSequence> missing);
		void resend_heartbeats(host_connection& connection, const vector<PacketSequence>& requested);
		void schedule_commands(vector<stored_payload> commands);
		void flush_send_queue();

		template<typename Writer>
		void send_connected(host_connection& connection, packet_kind kind, Writer writer);
		template<typename Writer>
		void send_connected(host_connection& connection, packet_kind kind, PacketSequence sequence, Writer writer);

		net::Socket socket;
		std::array<byte, 65507> receive_buffer{};
		vector<host_connection> connections;
		vector<queued_send> send_queue;
		vector<stored_payload> staged_commands;
		vector<scheduled_payload> scheduled_commands;
		SessionId next_session_id = 2;
		u64 next_snapshot_id = 1;
	};

	struct client_session final : Session::Impl {
		client_session(net::Socket socket, net::Peer host, Options options);

		void update() override;
		void advance() override;
		void disconnect() override;
		PayloadId submit(span<const byte> bytes) override;
		void finish_snapshot_load();
		void poll_socket();
		void receive_message(const net::Message& message);
		void buffer_tick(const tick_closure& closure);
		bool drain_buffered_ticks();
		void try_finish_snapshot();
		bool valid_session(SessionId packet_session_id) const;
		void send_connect_request();
		void send_join_request();
		void send_snapshot_request();
		void send_client_heartbeat();
		void resend_pending_payloads();
		void request_heartbeats(vector<PacketSequence> missing);
		void resend_heartbeats(const vector<PacketSequence>& requested);
		bool waiting_for_response() const override;

		template<typename Writer>
		void send_connected(packet_kind kind, Writer writer);
		template<typename Writer>
		void send_connected(packet_kind kind, PacketSequence sequence, Writer writer);

		net::Socket socket;
		net::Peer host;
		std::array<byte, 65507> receive_buffer{};
		SessionId session_id = 0;
		PacketSequence next_send_sequence = 1;
		PacketSequence next_heartbeat_sequence = 1;
		vector<sent_packet> sent_packets;
		received_packets received_heartbeats;
		vector<sent_packet> sent_heartbeats;
		vector<PacketSequence> pending_heartbeat_requests;
		vector<heartbeat_request_record> requested_heartbeats;
		vector<stored_payload> outgoing_commands;
		vector<Command::ID> rejection_acks;
		vector<stored_tick> buffered_ticks;
		snapshot_download snapshot;
		instant last_connect_activity = options.clock();
		instant next_connect_request = options.clock();
		instant next_join_request = options.clock();
		instant next_snapshot_request = options.clock();
		Tick next_pending_resend_tick = 0;
	};
} // namespace lf::lockstep

template<>
struct lf::bin::enum_validator<lf::lockstep::packet_kind> {
	static constexpr bool is_valid(lf::lockstep::packet_kind kind) {
		switch (kind) {
		case lf::lockstep::packet_kind::connect_request:
		case lf::lockstep::packet_kind::connect_accept:
		case lf::lockstep::packet_kind::join_request:
		case lf::lockstep::packet_kind::snapshot_begin:
		case lf::lockstep::packet_kind::snapshot_chunk:
		case lf::lockstep::packet_kind::snapshot_end:
		case lf::lockstep::packet_kind::client_heartbeat:
		case lf::lockstep::packet_kind::server_heartbeat:
		case lf::lockstep::packet_kind::snapshot_request:
		case lf::lockstep::packet_kind::disconnect:
		case lf::lockstep::packet_kind::disconnect_ack:
			return true;
		}
		return false;
	}
};

namespace lf::lockstep {
	constexpr SessionId host_session_id = 1;

	host_connection::host_connection(const net::Peer& peer, SessionId session_id)
		: peer(peer), session_id(session_id) {}

	Session::Impl::Impl(lockstep::mode initial_mode, lockstep::state initial_state, Options options)
		: session_mode{ initial_mode }, session_state{ initial_state }, options{ options }, current_tick{ options.initial_tick } {}

	bool Session::Impl::waiting_for_response() const {
		return false;
	}

	stored_payload Session::Impl::make_payload(span<const byte> bytes, SessionId source, bool track_pending) {
		const PayloadId id = next_payload_id;
		++next_payload_id;

		stored_payload payload;
		payload.id = id;
		payload.source = source;
		payload.hash = 14695981039346656037ull;
		payload.bytes.assign(bytes.begin(), bytes.end());
		for (byte value : bytes) {
			payload.hash ^= static_cast<u64>(std::to_integer<u08>(value));
			payload.hash *= 1099511628211ull;
		}

		if (track_pending) {
			pending_payloads.emplace_back(pending_payload{
				.id = payload.id,
				.hash = payload.hash,
				.bytes = payload.bytes,
			});
			rebuild_pending_views();
		}
		return payload;
	}

	void Session::Impl::rebuild_pending_views() {
		pending_views.clear();
		pending_views.reserve(pending_payloads.size());
		for (const pending_payload& pending : pending_payloads) {
			pending_views.emplace_back(PendingPayload{
				.id = pending.id,
				.hash = pending.hash,
				.bytes = span<const byte>(pending.bytes.data(), pending.bytes.size()),
			});
		}
	}

	void Session::Impl::push_ready_tick(Tick tick, vector<stored_payload>& commands) {
		ReadyTick ready;
		ready.tick = tick;
		ready.commands.reserve(commands.size());
		for (stored_payload& command : commands) {
			ready.commands.emplace_back(Command{
				.id = command.id,
				.source = command.source,
				.hash = command.hash,
				.bytes = std::move(command.bytes),
			});
		}
		ready_ticks.emplace_back(std::move(ready));
	}

	void Session::Impl::push_event(SessionEvent event) {
		events.emplace_back(std::move(event));
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, wire_payload& payload) {
		return stream(
			bin::field("id", payload.id),
			bin::field("source", payload.source),
			bin::field("hash", payload.hash),
			bin::field("bytes", payload.bytes)
		);
	}

<<<<<<< Updated upstream
	template<bin::byte_stream Stream>
	error process(Stream& stream, const wire_payload& payload) {
		return stream(
			bin::field("id", payload.id),
			bin::field("source", payload.source),
			bin::field("hash", payload.hash),
			bin::field("bytes", payload.bytes)
		);
	}

	error stream_tick(bin::write_stream& stream, const char* name, Tick tick) {
		const u32 wire_tick = static_cast<u32>(tick);
		return stream(bin::field(name, wire_tick));
	}

	error stream_tick(bin::read_stream& stream, const char* name, Tick& tick) {
		u32 wire_tick = 0;
		IF_ERROR_RETURN_ERROR(stream(bin::field(name, wire_tick)));
		tick = wire_tick;
		return {};
=======
	template<bin::byte_stream Stream, bin::data<Tick> Value>
	error stream_tick(Stream& stream, const char* name, Value& value) {
		return stream(field(name, value));
	}
	template<bin::byte_stream Stream, bin::data<PacketSequence> Value>
	error stream_packet_sequence(Stream& stream, const char* name, Value& value) {
		return stream(field(name, value));
>>>>>>> Stashed changes
	}

	error stream_packet_sequence(bin::write_stream& stream, const char* name, PacketSequence sequence) {
		const u32 wire_sequence = static_cast<u32>(sequence);
		return stream(bin::field(name, wire_sequence));
	}

	error stream_packet_sequence(bin::read_stream& stream, const char* name, PacketSequence& sequence) {
		u32 wire_sequence = 0;
		IF_ERROR_RETURN_ERROR(stream(bin::field(name, wire_sequence)));
		sequence = wire_sequence;
		return {};
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, tick_closure& closure) {
		IF_ERROR_RETURN_ERROR(stream_tick(stream, "tick", closure.tick));
		return stream(bin::field("commands", closure.commands));
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, const tick_closure& closure) {
		IF_ERROR_RETURN_ERROR(stream_tick(stream, "tick", closure.tick));
		return stream(bin::field("commands", closure.commands));
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, request_for_heartbeat& requests) {
<<<<<<< Updated upstream
		vector<u32> wire_sequences;
		IF_ERROR_RETURN_ERROR(stream(bin::field("sequences", wire_sequences)));
		requests.sequences.clear();
		requests.sequences.reserve(wire_sequences.size());
		for (u32 sequence : wire_sequences) {
			requests.sequences.emplace_back(sequence);
=======
		vector<u64> wire_sequences;
		if constexpr (bin::writable_byte_stream<Stream>) {
			wire_sequences.reserve(requests.sequences.size());
			for (PacketSequence sequence : requests.sequences) {
				wire_sequences.emplace_back(static_cast<u64>(sequence));
			}
		}
		if (auto err = stream(field("sequences", wire_sequences)); err) {
			return err;
		}
		if constexpr (bin::readable_byte_stream<Stream>) {
			requests.sequences.clear();
			requests.sequences.reserve(wire_sequences.size());
			for (u32 sequence : wire_sequences) {
				requests.sequences.emplace_back(sequence);
			}
>>>>>>> Stashed changes
		}
		return {};
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, const request_for_heartbeat& requests) {
		vector<u32> wire_sequences;
		wire_sequences.reserve(requests.sequences.size());
		for (PacketSequence sequence : requests.sequences) {
			wire_sequences.emplace_back(static_cast<u32>(sequence));
		}
		return stream(bin::field("sequences", wire_sequences));
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, transfer_block_requests::range& range) {
		return stream(
			bin::field("first", range.first),
			bin::field("count", range.count)
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, const transfer_block_requests::range& range) {
		return stream(
			bin::field("first", range.first),
			bin::field("count", range.count)
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, transfer_block_requests& requests) {
		return stream(
			bin::field("snapshot_id", requests.snapshot_id),
			bin::field("ranges", requests.ranges)
		);
	}

<<<<<<< Updated upstream
	template<bin::byte_stream Stream>
	error process(Stream& stream, const transfer_block_requests& requests) {
		return stream(
			bin::field("snapshot_id", requests.snapshot_id),
			bin::field("ranges", requests.ranges)
=======
	template<bin::byte_stream Stream, bin::data<rejected_command> Value>
	error process(Stream& stream, Value& value) {
		return stream(field("id", value.id), field("message", value.message));
	}

	template<bin::byte_stream Stream, bin::data<client_to_server_heartbeat> client_to_server_heartbeat>
	error process(Stream& stream, client_to_server_heartbeat& heartbeat) {
		u64 client_tick{ heartbeat.client_tick };
		if (auto err = stream(field("client_tick", client_tick)); err) {
			return err;
		}
		if constexpr (bin::readable_byte_stream<Stream>) {
			heartbeat.client_tick = client_tick;
		}
		return stream(
			field("requests", heartbeat.requests),
			field("commands", heartbeat.commands),
			field("rejection_acks", heartbeat.rejection_acks)
>>>>>>> Stashed changes
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, client_to_server_heartbeat& heartbeat) {
		IF_ERROR_RETURN_ERROR(stream_tick(stream, "client_tick", heartbeat.client_tick));
		return stream(
			bin::field("requests", heartbeat.requests),
			bin::field("commands", heartbeat.commands)
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, const client_to_server_heartbeat& heartbeat) {
		IF_ERROR_RETURN_ERROR(stream_tick(stream, "client_tick", heartbeat.client_tick));
		return stream(
			bin::field("requests", heartbeat.requests),
			bin::field("commands", heartbeat.commands)
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, server_to_client_heartbeat& heartbeat) {
		return stream(
<<<<<<< Updated upstream
			bin::field("requests", heartbeat.requests),
			bin::field("tick_closures", heartbeat.tick_closures)
		);
	}

	template<bin::byte_stream Stream>
	error process(Stream& stream, const server_to_client_heartbeat& heartbeat) {
		return stream(
			bin::field("requests", heartbeat.requests),
			bin::field("tick_closures", heartbeat.tick_closures)
=======
			field("requests", heartbeat.requests),
			field("tick_closures", heartbeat.tick_closures),
			field("rejections", heartbeat.rejections)
>>>>>>> Stashed changes
		);
	}

	bool same_peer(const net::Peer& lhs, const net::Peer& rhs) {
		return lhs.id() == rhs.id() && lhs.channel() == rhs.channel();
	}

	span<const byte> bytes_view(const vector<byte>& bytes) {
		return span<const byte>(bytes.data(), bytes.size());
	}

	void sort_commands(vector<stored_payload>& commands) {
		std::sort(commands.begin(), commands.end(), [](const stored_payload& lhs, const stored_payload& rhs) {
			if (lhs.source != rhs.source) {
				return lhs.source < rhs.source;
			}
			return lhs.id < rhs.id;
		});
	}

	wire_payload to_wire_payload(const stored_payload& payload) {
		wire_payload wire;
		wire.id = payload.id;
		wire.source = payload.source;
		wire.hash = payload.hash;
		wire.bytes = payload.bytes;
		return wire;
	}

	stored_payload from_wire_payload(const wire_payload& payload, SessionId source_override) {
		stored_payload stored;
		stored.id = payload.id;
		stored.source = source_override == 0 ? payload.source : source_override;
		stored.hash = payload.hash;
		stored.bytes = payload.bytes;
		return stored;
	}

	u32 clamp_request_delay(u32 delay, const Options& options) {
		const u32 min_delay = std::max<u32>(1, options.heartbeat_request_min_delay);
		const u32 max_delay = std::max(min_delay, options.heartbeat_request_max_delay);
		return std::clamp(delay, min_delay, max_delay);
	}

	void widen_request_delay(received_packets& received, PacketSequence late_by, const Options& options) {
		const u32 target = clamp_request_delay(static_cast<u32>(std::min<PacketSequence>(late_by + 1, 0xffff'ffffull)), options);
		received.request_delay = clamp_request_delay(std::max(received.request_delay, target), options);
	}

	void relax_request_delay(received_packets& received, const Options& options) {
		const u32 min_delay = std::max<u32>(1, options.heartbeat_request_min_delay);
		if (received.request_delay > min_delay) {
			--received.request_delay;
		}
		received.request_delay = clamp_request_delay(received.request_delay, options);
	}

	bool should_request_heartbeat(vector<heartbeat_request_record>& requested, PacketSequence sequence, PacketSequence local_heartbeat, const Options& options) {
		for (heartbeat_request_record& record : requested) {
			if (record.sequence != sequence) {
				continue;
			}
			if (local_heartbeat < record.local_heartbeat + options.heartbeat_request_interval) {
				return false;
			}
			record.local_heartbeat = local_heartbeat;
			return true;
		}
		requested.emplace_back(heartbeat_request_record{
			.sequence = sequence,
			.local_heartbeat = local_heartbeat,
		});
		if (requested.size() > 2048) {
			requested.erase(requested.begin());
		}
		return true;
	}

	packet_record record_received_heartbeat(
		received_packets& received,
		vector<heartbeat_request_record>& requested,
		PacketSequence sequence,
		PacketSequence local_heartbeat,
		const Options& options
	) {
		packet_record record;
		received.request_delay = clamp_request_delay(received.request_delay, options);
		auto append_missing = [&] {
			for (PacketSequence missing_sequence : received.missing) {
				if (record.missing.size() >= options.max_missing_per_nack) {
					break;
				}
				if (received.latest_received < missing_sequence + received.request_delay) {
					continue;
				}
				if (!should_request_heartbeat(requested, missing_sequence, local_heartbeat, options)) {
					continue;
				}
				record.missing.emplace_back(missing_sequence);
			}
		};

		if (sequence == 0) {
			return record;
		}
		if (sequence > received.next_expected + 2048) {
			throw runtime_exception("heartbeat sequence is outside the receive window");
		}
		received.latest_received = std::max(received.latest_received, sequence);

		if (sequence < received.next_expected) {
			auto missing = std::find(received.missing.begin(), received.missing.end(), sequence);
			if (missing != received.missing.end()) {
				widen_request_delay(received, received.latest_received - sequence, options);
				received.missing.erase(missing);
				record.fresh = true;
			}
			append_missing();
			return record;
		}

		if (sequence == received.next_expected) {
			relax_request_delay(received, options);
		}
		record.fresh = true;
		while (received.next_expected < sequence) {
			received.missing.emplace_back(received.next_expected);
			++received.next_expected;
		}
		received.next_expected = sequence + 1;
		append_missing();
		return record;
	}

	void remember_sent_packet(vector<sent_packet>& sent_packets, PacketSequence sequence, const vector<byte>& bytes) {
		sent_packets.emplace_back(sent_packet{
			.sequence = sequence,
			.bytes = bytes,
		});
		if (sent_packets.size() > 2048) {
			sent_packets.erase(sent_packets.begin());
		}
	}

	void erase_confirmed_pending(vector<pending_payload>& pending, SessionId local_session_id, const vector<stored_payload>& commands) {
		for (const stored_payload& command : commands) {
			if (command.source != local_session_id) {
				continue;
			}
			for (size_t index = 0; index < pending.size();) {
				if (pending[index].id == command.id) {
					pending.erase(pending.begin() + static_cast<i64>(index));
				} else {
					++index;
				}
			}
		}
	}

	bool remember_received_payload(host_connection& connection, PayloadId id) {
		for (PayloadId received_id : connection.received_payloads) {
			if (received_id == id) {
				return false;
			}
		}
		connection.received_payloads.emplace_back(id);
		if (connection.received_payloads.size() > 4096) {
			connection.received_payloads.erase(connection.received_payloads.begin());
		}
		return true;
	}

	template<typename Writer>
	vector<byte> write_packet(packet_kind kind, Writer writer) {
		bin::write_stream stream;
		if (error err = stream(bin::field("kind", kind))) {
			throw runtime_exception(err.message);
		}
		if (error err = writer(stream)) {
			throw runtime_exception(err.message);
		}
		return stream.take_written();
	}

	template<typename Writer>
	vector<byte> write_connected_packet(packet_kind kind, SessionId session_id, PacketSequence packet_sequence, Writer writer) {
		return write_packet(kind, [&](bin::write_stream& stream) -> error {
<<<<<<< Updated upstream
			const u32 wire_session_id = static_cast<u32>(session_id);
			const u32 wire_packet_sequence = static_cast<u32>(packet_sequence);
=======
			const u64 wire_session_id = session_id.get();
			const u64 wire_packet_sequence = packet_sequence;
>>>>>>> Stashed changes
			IF_ERROR_RETURN_ERROR(stream(
				bin::field("session_id", wire_session_id),
				bin::field("packet_sequence", wire_packet_sequence)
			));
			return writer(stream);
		});
	}

<<<<<<< Updated upstream
	error read_connected_header(bin::read_stream& stream, SessionId& session_id, PacketSequence& packet_sequence) {
		u32 wire_session_id = 0;
		u32 wire_packet_sequence = 0;
=======
	error read_connected_header(bin::read_stream& stream, Session::ID& session_id, PacketSequence& packet_sequence) {
		u64 wire_session_id = 0;
		u64 wire_packet_sequence = 0;
>>>>>>> Stashed changes
		IF_ERROR_RETURN_ERROR(stream(
			bin::field("session_id", wire_session_id),
			bin::field("packet_sequence", wire_packet_sequence)
		));
		session_id = wire_session_id;
		packet_sequence = wire_packet_sequence;
		return {};
	}

	void resend_packets(net::Socket& socket, const net::Peer& peer, const vector<sent_packet>& sent_packets, const vector<PacketSequence>& missing, u32 max_resends) {
		u32 resent = 0;
		for (PacketSequence sequence : missing) {
			if (resent >= max_resends) {
				break;
			}
			for (const sent_packet& packet : sent_packets) {
				if (packet.sequence == sequence) {
					socket.send(peer, bytes_view(packet.bytes));
					++resent;
					break;
				}
			}
		}
	}

	vector<PacketSequence> take_heartbeat_requests(vector<PacketSequence>& pending) {
		vector<PacketSequence> requests = std::move(pending);
		pending.clear();
		return requests;
	}

	void append_unique(vector<PacketSequence>& out, PacketSequence sequence, u32 max_count) {
		if (out.size() >= max_count) {
			return;
		}
		if (std::find(out.begin(), out.end(), sequence) != out.end()) {
			return;
		}
		out.emplace_back(sequence);
	}

	void append_snapshot_range(transfer_block_requests& requests, u32 chunk_index, u32 max_ranges) {
		if (!requests.ranges.empty()) {
			transfer_block_requests::range& last = requests.ranges.back();
			if (last.first + last.count == chunk_index) {
				++last.count;
				return;
			}
		}
		if (requests.ranges.size() >= max_ranges) {
			return;
		}
		requests.ranges.emplace_back(transfer_block_requests::range{
			.first = chunk_index,
			.count = 1,
		});
	}

	template<bin::readable_byte_stream Stream>
	error process(Stream& stream, host_session& session, const net::Peer& peer) {
		packet_kind kind = {};
		IF_ERROR_RETURN_ERROR(stream(bin::field("kind", kind)));

		switch (kind) {
		case packet_kind::connect_request: {
			PacketSequence packet_sequence = 0;
			u32 version = 0;
			IF_ERROR_RETURN_ERROR(stream_packet_sequence(stream, "packet_sequence", packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("protocol_version", version)));
			if (version != protocol_version) {
				return {};
			}

			host_connection& connection = session.find_or_create_connection(peer);
			session.send_connect_accept(connection);
			return {};
		}
		case packet_kind::join_request: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			vector<byte> login_payload;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("login_payload", login_payload)));

			host_connection* connection = session.find_connection(peer, session_id);
			if (!connection) {
				return {};
			}
			if (connection->connection_state == state::joined) {
				session.push_event(SessionEvent{
					.kind = SessionEventKind::login_requested,
					.session_id = session_id,
					.bytes = std::move(login_payload),
				});
				return {};
			}
			if (connection->connection_state == state::downloading_snapshot) {
				return {};
			}
			u16 joined_clients = 0;
			for (const host_connection& existing : session.connections) {
				if (existing.connection_state != state::disconnected && existing.session_id != session_id) {
					++joined_clients;
				}
			}
			if (session.options.max_clients != 0 && joined_clients >= session.options.max_clients) {
				session.reject_login(session_id, reason);
				return {};
			}
			session.push_event(SessionEvent{
				.kind = SessionEventKind::login_requested,
				.session_id = session_id,
				.bytes = std::move(login_payload),
			});
			return {};
		}
		case packet_kind::client_heartbeat: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			client_to_server_heartbeat heartbeat;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("heartbeat", heartbeat)));

			host_connection* connection = session.find_connection(peer, session_id);
			if (!connection || connection->connection_state != state::joined) {
				return {};
			}
			// Mark the peer as actually playing — they wouldn't be sending
			// heartbeats yet if they were still receiving / decoding the
			// snapshot. The "Saving the world for X" overlay reads this.
			connection->client_heartbeat_seen = true;
			session.resend_heartbeats(*connection, heartbeat.requests.sequences);
			packet_record record = record_received_heartbeat(
				connection->received_heartbeats,
				connection->requested_heartbeats,
				packet_sequence,
				connection->next_heartbeat_sequence,
				session.options
			);
			session.request_heartbeats(*connection, std::move(record.missing));
			if (!record.fresh) {
				return {};
			}
			for (const auto id : heartbeat.rejection_acks) {
				std::erase_if(connection->rejections, [id](const auto& value) { return value.id == id; });
			}
			if (heartbeat.client_tick + session.options.max_buffered_ticks < session.current_tick) {
				session.reject_login(connection->session_id, "client fell behind retained history");
				return {};
			}
			for (const wire_payload& payload : heartbeat.commands) {
				if (payload.id.get() + 4096 < connection->highest_payload.get() || !remember_received_payload(*connection, payload.id)) {
					continue;
				}
<<<<<<< Updated upstream
				vector<stored_payload> scheduled;
				scheduled.emplace_back(from_wire_payload(payload, connection->session_id));
				session.schedule_commands(std::move(scheduled));
=======
				if (payload.id.get() > connection->highest_payload.get()) {
					connection->highest_payload = payload.id;
				}
				auto accepted = payload.bytes.size() > session.options.max_command_bytes
					? report<vector<byte>>{ unexpected(error{ generic_errc::input_error, "command exceeds size limit" }) }
					: session.command_admitter ? session.command_admitter(connection->session_id, payload.bytes) : report<vector<byte>>{ payload.bytes };
				if (!accepted) {
					if (connection->rejections.size() >= 128) {
						session.reject_login(connection->session_id, "too many unacknowledged commands");
						return {};
					}
					connection->rejections.push_back({ payload.id, accepted.error().message });
					continue;
				}
				auto command = from_wire_payload(payload, connection->session_id);
				command.bytes = std::move(*accepted);
				session.schedule_commands({ std::move(command) });
>>>>>>> Stashed changes
			}
			return {};
		}
		case packet_kind::snapshot_request: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			transfer_block_requests requests;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("requests", requests)));

			host_connection* connection = session.find_connection(peer, session_id);
			if (connection) {
				session.resend_snapshot_chunks(*connection, requests);
			}
			return {};
		}
		case packet_kind::disconnect: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));

			for (size_t index = 0; index < session.connections.size();) {
				if (session.connections[index].session_id == session_id && same_peer(session.connections[index].peer, peer)) {
					session.push_event(SessionEvent{
						.kind = SessionEventKind::peer_disconnected,
						.session_id = session_id,
					});
					session.connections.erase(session.connections.begin() + static_cast<i64>(index));
				} else {
					++index;
				}
			}
			return {};
		}
		case packet_kind::disconnect_ack: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));

			host_connection* connection = session.find_connection(peer, session_id);
			if (connection) {
				connection->disconnect_acknowledged = true;
			}
			return {};
		}
		default:
			return {};
		}
	}

	template<bin::readable_byte_stream Stream>
	error process(Stream& stream, client_session& session, const net::Peer& peer) {
		if (!same_peer(peer, session.host)) {
			return {};
		}

		packet_kind kind = {};
		IF_ERROR_RETURN_ERROR(stream(bin::field("kind", kind)));

		switch (kind) {
		case packet_kind::connect_accept: {
			PacketSequence packet_sequence = 0;
<<<<<<< Updated upstream
			SessionId accepted_session_id = 0;
			u32 wire_session_id = 0;
=======
			Session::ID accepted_session_id{};
			u64 wire_session_id = 0;
>>>>>>> Stashed changes
			u32 version = 0;
			IF_ERROR_RETURN_ERROR(stream_packet_sequence(stream, "packet_sequence", packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("protocol_version", version)));
			if (version != protocol_version) {
				session.socket.disconnect();
				session.session_state = state::disconnected;
				session.push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
				return {};
			}
			IF_ERROR_RETURN_ERROR(stream(bin::field("session_id", wire_session_id)));
			accepted_session_id = wire_session_id;

			session.session_id = accepted_session_id;
			session.local_session = accepted_session_id;
			if (session.session_state == state::connecting) {
				session.session_state = state::logging_in;
				session.next_join_request = session.options.clock();
			}
			return {};
		}
		case packet_kind::snapshot_begin: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			u64 snapshot_id = 0;
			Tick baseline_tick = 0;
			u64 total_bytes = 0;
			u32 chunk_count = 0;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("snapshot_id", snapshot_id)));
			IF_ERROR_RETURN_ERROR(stream_tick(stream, "baseline_tick", baseline_tick));
			IF_ERROR_RETURN_ERROR(stream(
				bin::field("total_bytes", total_bytes),
				bin::field("chunk_count", chunk_count)
			));

			if (!session.valid_session(session_id)) {
				return {};
			}
			if (session.session_state == state::catching_up || session.session_state == state::joined) {
				return {};
			}
			if (session.snapshot.active && session.snapshot.snapshot_id == snapshot_id) {
				return {};
			}
			session.session_state = state::downloading_snapshot;
			session.snapshot = {};
			session.snapshot.active = true;
			session.snapshot.snapshot_id = snapshot_id;
			session.snapshot.baseline_tick = baseline_tick;
			session.snapshot.chunk_size = session.options.snapshot_chunk_bytes == 0 ? 1000 : session.options.snapshot_chunk_bytes;
			session.snapshot.chunk_count = chunk_count;
			if (!total_bytes || total_bytes > session.options.max_snapshot_bytes || chunk_count != (total_bytes + session.snapshot.chunk_size - 1) / session.snapshot.chunk_size) {
				return { generic_errc::parse_error, "invalid snapshot size" };
			}
			session.snapshot.bytes.resize(static_cast<size_t>(total_bytes));
			session.snapshot.received_chunks.resize(chunk_count);
			session.snapshot.requested_chunks.resize(chunk_count);
			return {};
		}
		case packet_kind::snapshot_chunk: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			u64 snapshot_id = 0;
			u32 chunk_index = 0;
			vector<byte> bytes;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(
				bin::field("snapshot_id", snapshot_id),
				bin::field("chunk_index", chunk_index),
				bin::field("bytes", bytes)
			));

			if (!session.valid_session(session_id) || !session.snapshot.active || session.snapshot.snapshot_id != snapshot_id) {
				return {};
			}
			if (chunk_index >= session.snapshot.chunk_count || session.snapshot.received_chunks[chunk_index] != 0) {
				return {};
			}
			const size_t offset = static_cast<size_t>(chunk_index) * session.snapshot.chunk_size;
			if (offset > session.snapshot.bytes.size() || bytes.size() != std::min<usize>(session.snapshot.chunk_size, session.snapshot.bytes.size() - offset)) {
				return {};
			}
			std::copy(bytes.begin(), bytes.end(), session.snapshot.bytes.begin() + static_cast<i64>(offset));
			session.snapshot.received_chunks[chunk_index] = 1;
			++session.snapshot.received_count;
			session.snapshot.received_bytes += bytes.size();
			session.try_finish_snapshot();
			return {};
		}
		case packet_kind::snapshot_end: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			u64 snapshot_id = 0;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("snapshot_id", snapshot_id)));

			if (!session.valid_session(session_id) || !session.snapshot.active || session.snapshot.snapshot_id != snapshot_id) {
				return {};
			}
			session.snapshot.end_received = true;
			session.try_finish_snapshot();
			return {};
		}
		case packet_kind::server_heartbeat: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			server_to_client_heartbeat heartbeat;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("heartbeat", heartbeat)));

			if (!session.valid_session(session_id)) {
				return {};
			}
			session.resend_heartbeats(heartbeat.requests.sequences);
			packet_record record = record_received_heartbeat(
				session.received_heartbeats,
				session.requested_heartbeats,
				packet_sequence,
				session.next_heartbeat_sequence,
				session.options
			);
			session.request_heartbeats(std::move(record.missing));
			if (!record.fresh) {
				return {};
			}
			for (const auto& rejected : heartbeat.rejections) {
				const auto pending{ std::find_if(session.pending_payloads.begin(), session.pending_payloads.end(), [&](const auto& value) { return value.id == rejected.id; }) };
				if (pending != session.pending_payloads.end()) {
					session.pending_payloads.erase(pending);
					session.rebuild_pending_views();
					session.push_event(SessionEvent{ .kind = SessionEventKind::command_rejected, .command_id = rejected.id, .message = rejected.message });
				}
				if (std::find(session.rejection_acks.begin(), session.rejection_acks.end(), rejected.id) == session.rejection_acks.end()) {
					session.rejection_acks.push_back(rejected.id);
					if (session.rejection_acks.size() > 256) { session.rejection_acks.erase(session.rejection_acks.begin()); }
				}
			}
			for (const tick_closure& closure : heartbeat.tick_closures) {
				session.buffer_tick(closure);
			}
			if (session.session_state != state::connecting &&
				session.session_state != state::logging_in &&
				session.session_state != state::downloading_snapshot) {
				if (session.drain_buffered_ticks() && session.session_state == state::catching_up) {
					session.session_state = state::joined;
				}
			}
			if (session.session_state == state::joined) {
				session.resend_pending_payloads();
				session.send_client_heartbeat();
			}
			return {};
		}
		case packet_kind::disconnect: {
			SessionId session_id = 0;
			PacketSequence packet_sequence = 0;
			IF_ERROR_RETURN_ERROR(read_connected_header(stream, session_id, packet_sequence));
			string reason;
			if (stream.remaining()) { IF_ERROR_RETURN_ERROR(stream(field("reason", reason))); }
			if (session.valid_session(session_id)) {
				session.send_connected(packet_kind::disconnect_ack, [](bin::write_stream&) -> error {
					return {};
				});
				session.socket.disconnect();
				session.session_state = state::disconnected;
				session.push_event(SessionEvent{ .kind = SessionEventKind::disconnected, .message = reason });
			}
			return {};
		}
		default:
			return {};
		}
	}

	offline_session::offline_session(Options options)
		: Impl(mode::offline, state::joined, options) {
		local_session = host_session_id;
	}

	void offline_session::update() {
	}

	void offline_session::advance() {
		if (session_state == state::disconnected) {
			throw runtime_exception("lockstep session is disconnected");
		}
		++current_tick;
		sort_commands(staged_commands);
		erase_confirmed_pending(pending_payloads, host_session_id, staged_commands);
		rebuild_pending_views();
		push_ready_tick(current_tick, staged_commands);
		staged_commands.clear();
	}

	void offline_session::disconnect() {
		if (session_state == state::disconnected) {
			return;
		}
		session_state = state::disconnected;
		push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
	}

	PayloadId offline_session::submit(span<const byte> bytes) {
		if (session_state != state::joined) {
			throw runtime_exception("lockstep session is not joined");
		}
		stored_payload payload = make_payload(bytes, host_session_id, true);
<<<<<<< Updated upstream
		const PayloadId id = payload.id;
		staged_commands.emplace_back(std::move(payload));
=======
		const Command::ID id = payload.id;
		if (admit(payload)) {
			staged_commands.emplace_back(std::move(payload));
		}
>>>>>>> Stashed changes
		return id;
	}

	host_session::host_session(net::Socket socket, Options options)
		: Impl(mode::host, state::joined, options), socket(std::move(socket)) {
		local_session = host_session_id;
	}

	void host_session::update() {
		if (session_state == state::disconnected) {
			return;
		}
		continue_snapshots();
		poll_socket();
		flush_send_queue();
	}

	void host_session::advance() {
		if (session_state == state::disconnected) {
			throw runtime_exception("lockstep session is disconnected");
		}
		for (const host_connection& connection : connections) {
			// A snapshot must have a baseline before the simulation can move on.
			// Once begin_snapshot() has captured it, however, the client can buffer
			// every subsequent closure while the bulk bytes are still in flight.
			// Pausing for the whole download made one slow join freeze the server.
			if (connection.snapshot_requested) {
				return;
			}
		}
		++current_tick;
		for (size_t index = 0; index < scheduled_commands.size();) {
			if (scheduled_commands[index].tick > current_tick) {
				++index;
				continue;
			}
			staged_commands.emplace_back(std::move(scheduled_commands[index].payload));
			scheduled_commands.erase(scheduled_commands.begin() + static_cast<i64>(index));
		}
		sort_commands(staged_commands);
		erase_confirmed_pending(pending_payloads, host_session_id, staged_commands);
		rebuild_pending_views();

		tick_closure closure;
		closure.tick = current_tick;
		for (const stored_payload& command : staged_commands) {
			closure.commands.emplace_back(to_wire_payload(command));
		}
		for (host_connection& connection : connections) {
			if (connection.connection_state != state::joined &&
				connection.connection_state != state::downloading_snapshot) {
				continue;
			}
			const PacketSequence heartbeat_sequence = connection.next_heartbeat_sequence;
			++connection.next_heartbeat_sequence;
			send_connected(connection, packet_kind::server_heartbeat, heartbeat_sequence, [&](bin::write_stream& stream) -> error {
				server_to_client_heartbeat heartbeat;
				heartbeat.requests.sequences = take_heartbeat_requests(connection.pending_heartbeat_requests);
				heartbeat.tick_closures.emplace_back(closure);
<<<<<<< Updated upstream
				return stream(bin::field("heartbeat", heartbeat));
=======
				heartbeat.rejections = connection.rejections;
				return stream(field("heartbeat", heartbeat));
>>>>>>> Stashed changes
			});
			remember_sent_packet(connection.sent_heartbeats, heartbeat_sequence, send_queue.back().bytes);
		}
		push_ready_tick(current_tick, staged_commands);
		staged_commands.clear();
	}

	void host_session::disconnect() {
		if (session_state == state::disconnected) { return; }
		for (auto& connection : connections) {
			const auto bytes = write_connected_packet(packet_kind::disconnect, connection.session_id, connection.next_send_sequence++, [](bin::write_stream&) -> error { return {}; });
			socket.send(connection.peer, bytes);
		}
		socket.disconnect();
		session_state = state::disconnected;
		push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
	}

	PayloadId host_session::submit(span<const byte> bytes) {
		if (session_state != state::joined) {
			throw runtime_exception("lockstep session is not joined");
		}
		vector<stored_payload> collected;
		collected.emplace_back(make_payload(bytes, host_session_id, true));
<<<<<<< Updated upstream
		const PayloadId id = collected.back().id;
		schedule_commands(std::move(collected));
=======
		const Command::ID id = collected.back().id;
		if (admit(collected.back())) {
			schedule_commands(std::move(collected));
		}
>>>>>>> Stashed changes
		return id;
	}

	void host_session::poll_socket() {
		u32 received = 0;
		while (received < options.max_incoming_packets_per_update) {
			optional<net::Message> message = socket.recv(receive_buffer);
			if (!message) {
				break;
			}
			try {
				receive_message(*message);
			} catch (const exception&) {
			}
			++received;
		}
	}

	void host_session::receive_message(const net::Message& message) {
		bin::read_stream stream{ message.second, { .max_string_bytes = 128 * 1024, .max_vector_elements = 1024 * 1024 } };
		if (error err = process(stream, *this, message.first)) {
			throw runtime_exception(err.message);
		}
	}

	host_connection& host_session::find_or_create_connection(const net::Peer& peer) {
		for (host_connection& connection : connections) {
			if (same_peer(connection.peer, peer)) {
				return connection;
			}
		}
		const SessionId session_id = next_session_id;
		++next_session_id;
		connections.emplace_back(peer, session_id);
		return connections.back();
	}

	host_connection* host_session::find_connection(const net::Peer& peer, SessionId session_id) {
		for (host_connection& connection : connections) {
			if (connection.session_id == session_id && same_peer(connection.peer, peer)) {
				return &connection;
			}
		}
		return nullptr;
	}

	void host_session::send_connect_accept(host_connection& connection) {
		const PacketSequence packet_sequence = connection.next_send_sequence;
		++connection.next_send_sequence;
		vector<byte> bytes = write_packet(packet_kind::connect_accept, [&](bin::write_stream& stream) -> error {
<<<<<<< Updated upstream
			const u32 wire_session_id = static_cast<u32>(connection.session_id);
=======
			const u64 wire_session_id = static_cast<u32>(connection.session_id.get());
>>>>>>> Stashed changes
			IF_ERROR_RETURN_ERROR(stream_packet_sequence(stream, "packet_sequence", packet_sequence));
			IF_ERROR_RETURN_ERROR(stream(bin::field("protocol_version", protocol_version)));
			return stream(bin::field("session_id", wire_session_id));
		});
		socket.send(connection.peer, bytes_view(bytes));
		flush_send_queue();
		remember_sent_packet(connection.sent_packets, packet_sequence, bytes);
	}

<<<<<<< Updated upstream
	void host_session::accept_login(SessionId session_id, span<const byte> snapshot) {
=======
	void host_session::accept_login(Session::ID session_id, Tick baseline, span<const byte> snapshot) {
>>>>>>> Stashed changes
		for (host_connection& connection : connections) {
			if (connection.session_id != session_id) {
				continue;
			}
			connection.connection_state = state::downloading_snapshot;
			connection.snapshot_requested = true;
			connection.snapshot_bytes.assign(snapshot.begin(), snapshot.end());
<<<<<<< Updated upstream
=======
			connection.snapshot_baseline_tick = baseline;
>>>>>>> Stashed changes
			return;
		}
	}

<<<<<<< Updated upstream
	void host_session::reject_login(SessionId session_id) {
=======
	void host_session::reject_login(Session::ID session_id, string_view reason) {
		for (auto& connection : connections) {
			if (connection.session_id == session_id) {
				auto bytes = write_connected_packet(packet_kind::disconnect, session_id, connection.next_send_sequence++, [&](bin::write_stream& stream) {
					string message{ reason };
					return stream(field("reason", message));
				});
				socket.send(connection.peer, bytes);
				break;
			}
		}
>>>>>>> Stashed changes
		disconnect_peer(session_id);
	}

	void host_session::disconnect_peer(SessionId session_id) {
		for (host_connection& connection : connections) {
			if (connection.session_id != session_id) {
				continue;
			}
			send_connected(connection, packet_kind::disconnect, [](bin::write_stream&) -> error {
				return {};
			});
			connection.connection_state = state::disconnected;
			push_event(SessionEvent{
				.kind = SessionEventKind::peer_disconnected,
				.session_id = session_id,
			});
			return;
		}
	}

	void host_session::continue_snapshots() {
		for (host_connection& connection : connections) {
			if (connection.snapshot_sending) {
				continue_snapshot(connection);
				return;
			}
			if (!connection.snapshot_requested) {
				continue;
			}
			connection.snapshot_requested = false;
			try {
				begin_snapshot(connection);
				continue_snapshot(connection);
			} catch (...) {
				connection.connection_state = state::disconnected;
				connection.snapshot_sending = false;
				connection.snapshot_bytes.clear();
				throw;
			}
			return;
		}
	}

	void host_session::begin_snapshot(host_connection& connection) {
		const u16 chunk_size = options.snapshot_chunk_bytes == 0 ? 1000 : options.snapshot_chunk_bytes;
		const u32 chunk_count = static_cast<u32>((connection.snapshot_bytes.size() + chunk_size - 1u) / chunk_size);
		connection.snapshot_id = next_snapshot_id;
		++next_snapshot_id;
		connection.snapshot_baseline_tick = current_tick;
		connection.snapshot_chunk_size = chunk_size;
		connection.snapshot_chunk_count = chunk_count;
		connection.next_snapshot_chunk = 0;
		connection.snapshot_sending = true;

		send_connected(connection, packet_kind::snapshot_begin, [&](bin::write_stream& stream) -> error {
			const u64 total_bytes = static_cast<u64>(connection.snapshot_bytes.size());
			IF_ERROR_RETURN_ERROR(stream(bin::field("snapshot_id", connection.snapshot_id)));
			IF_ERROR_RETURN_ERROR(stream_tick(stream, "baseline_tick", connection.snapshot_baseline_tick));
			return stream(
				bin::field("total_bytes", total_bytes),
				bin::field("chunk_count", connection.snapshot_chunk_count)
			);
		});
	}

	void host_session::continue_snapshot(host_connection& connection) {
		u32 sent = 0;
		while (connection.next_snapshot_chunk < connection.snapshot_chunk_count && sent < options.max_snapshot_chunks_per_update) {
			const u32 chunk_index = connection.next_snapshot_chunk;
			++connection.next_snapshot_chunk;
			send_snapshot_chunk(connection, chunk_index);
			++sent;
		}

		if (connection.next_snapshot_chunk < connection.snapshot_chunk_count) {
			return;
		}

		send_snapshot_end(connection);
		connection.snapshot_sending = false;
		connection.connection_state = state::joined;
	}

	void host_session::send_snapshot_chunk(host_connection& connection, u32 chunk_index) {
		if (chunk_index >= connection.snapshot_chunk_count || connection.snapshot_bytes.empty()) {
			return;
		}
		const size_t offset = static_cast<size_t>(chunk_index) * connection.snapshot_chunk_size;
		if (offset >= connection.snapshot_bytes.size()) {
			return;
		}
		const size_t remaining = connection.snapshot_bytes.size() - offset;
		const size_t count = std::min(static_cast<size_t>(connection.snapshot_chunk_size), remaining);
		send_connected(connection, packet_kind::snapshot_chunk, [&](bin::write_stream& stream) -> error {
			vector<byte> bytes;
			bytes.assign(connection.snapshot_bytes.begin() + static_cast<i64>(offset), connection.snapshot_bytes.begin() + static_cast<i64>(offset + count));
			return stream(
				bin::field("snapshot_id", connection.snapshot_id),
				bin::field("chunk_index", chunk_index),
				bin::field("bytes", bytes)
			);
		});
	}

	void host_session::send_snapshot_end(host_connection& connection) {
		send_connected(connection, packet_kind::snapshot_end, [&](bin::write_stream& stream) -> error {
			return stream(bin::field("snapshot_id", connection.snapshot_id));
		});
	}

	void host_session::resend_snapshot_chunks(host_connection& connection, const transfer_block_requests& requests) {
		if (requests.snapshot_id != connection.snapshot_id || connection.snapshot_bytes.empty()) {
			return;
		}
		u32 sent = 0;
		for (const transfer_block_requests::range& range : requests.ranges) {
			for (u32 offset = 0; offset < range.count; ++offset) {
				if (sent >= options.max_snapshot_chunks_per_update) {
					return;
				}
				send_snapshot_chunk(connection, range.first + offset);
				++sent;
			}
		}
		if (requests.ranges.empty()) {
			send_snapshot_end(connection);
		}
	}

	void host_session::request_heartbeats(host_connection& connection, vector<PacketSequence> missing) {
		if (missing.empty()) {
			return;
		}
		for (PacketSequence sequence : missing) {
			append_unique(connection.pending_heartbeat_requests, sequence, options.max_missing_per_nack);
		}
	}

	void host_session::resend_heartbeats(host_connection& connection, const vector<PacketSequence>& requested) {
		resend_packets(socket, connection.peer, connection.sent_heartbeats, requested, options.max_resends_per_update);
	}

	void host_session::schedule_commands(vector<stored_payload> commands) {
		if (commands.empty()) {
			return;
		}
		const Tick target_tick = current_tick + std::max<u32>(1, options.command_latency_ticks);
		for (stored_payload& command : commands) {
			scheduled_commands.emplace_back(scheduled_payload{
				.tick = target_tick,
				.payload = std::move(command),
			});
		}
	}

	void host_session::flush_send_queue() {
		u32 sent = 0;
		while (!send_queue.empty() && sent < options.max_outgoing_packets_per_update) {
			queued_send outgoing = std::move(send_queue.front());
			send_queue.erase(send_queue.begin());
			socket.send(outgoing.peer, bytes_view(outgoing.bytes));
			++sent;
		}
	}

	template<typename Writer>
	void host_session::send_connected(host_connection& connection, packet_kind kind, Writer writer) {
		const PacketSequence packet_sequence = connection.next_send_sequence;
		++connection.next_send_sequence;
		send_connected(connection, kind, packet_sequence, writer);
	}

	template<typename Writer>
	void host_session::send_connected(host_connection& connection, packet_kind kind, PacketSequence sequence, Writer writer) {
		vector<byte> bytes = write_connected_packet(kind, connection.session_id, sequence, writer);
		send_queue.emplace_back(queued_send{
			.peer = connection.peer,
			.bytes = bytes,
		});
		remember_sent_packet(connection.sent_packets, sequence, bytes);
	}

	client_session::client_session(net::Socket socket, net::Peer host, Options options)
		: Impl(mode::client, state::connecting, options), socket(std::move(socket)), host(std::move(host)) {}

	void client_session::update() {
		if (session_state == state::disconnected) {
			return;
		}
		poll_socket();
		if (session_state == state::downloading_snapshot && snapshot.load_started) {
			try_finish_snapshot();
		}
		if (session_state == state::catching_up) {
			if (drain_buffered_ticks()) {
				session_state = state::joined;
			}
		}
		const instant current_time = options.clock();
		const duration time_since_activity = duration::from_quantum(current_time.quantum_count() - last_connect_activity.quantum_count());
		if ((session_state == state::connecting ||
			 session_state == state::logging_in ||
			 session_state == state::downloading_snapshot ||
			 session_state == state::catching_up) &&
			options.connect_timeout.quantum_count() > 0 &&
			time_since_activity >= options.connect_timeout) {
			socket.disconnect();
			session_state = state::disconnected;
			push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
			return;
		}
		if (session_state == state::joined &&
			options.connect_timeout.quantum_count() > 0 &&
			time_since_activity >= options.connect_timeout) {
			socket.disconnect();
			session_state = state::disconnected;
			push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
			return;
		}
		if (session_state == state::connecting && current_time >= next_connect_request) {
			send_connect_request();
			next_connect_request = current_time + options.handshake_interval;
		}
		if ((session_state == state::logging_in || session_state == state::downloading_snapshot) && current_time >= next_join_request) {
			send_join_request();
			session_state = state::downloading_snapshot;
			next_join_request = current_time + options.handshake_interval;
		}
		if (session_state == state::downloading_snapshot && snapshot.active && current_time >= next_snapshot_request) {
			send_snapshot_request();
			next_snapshot_request = current_time + options.nack_interval;
		}
	}

	void client_session::advance() {
		throw runtime_exception("client lockstep sessions advance only from server ticks");
	}

	void client_session::disconnect() {
		if (session_state == state::disconnected) {
			return;
		}
		if (session_id != 0) {
			send_connected(packet_kind::disconnect, [](bin::write_stream&) -> error {
				return {};
			});
		}
		socket.disconnect();
		session_state = state::disconnected;
		push_event(SessionEvent{ .kind = SessionEventKind::disconnected });
	}

	PayloadId client_session::submit(span<const byte> bytes) {
		if (session_state != state::joined || session_id == 0) {
			throw runtime_exception("lockstep session is not joined");
		}
		stored_payload payload = make_payload(bytes, session_id, true);
		const PayloadId id = payload.id;
		outgoing_commands.emplace_back(std::move(payload));
		return id;
	}

	void client_session::finish_snapshot_load() {
		if (!snapshot.active || !snapshot.load_started) {
			return;
		}
		current_tick = snapshot.baseline_tick;
		pending_payloads.clear();
		rebuild_pending_views();
		snapshot = {};
		session_state = state::catching_up;
		if (drain_buffered_ticks() && session_state == state::catching_up) {
			session_state = state::joined;
		}
	}

	void client_session::poll_socket() {
		u32 received = 0;
		while (received < options.max_incoming_packets_per_update) {
			optional<net::Message> message = socket.recv(receive_buffer);
			if (!message) {
				break;
			}
			try {
				receive_message(*message);
			} catch (const exception&) {
			}
			++received;
		}
	}

	void client_session::receive_message(const net::Message& message) {
		bin::read_stream stream{ message.second, { .max_string_bytes = 128 * 1024, .max_vector_elements = 1024 * 1024 } };
		last_connect_activity = options.clock();
		if (error err = process(stream, *this, message.first)) {
			throw runtime_exception(err.message);
		}
	}

	void client_session::buffer_tick(const tick_closure& closure) {
		if (closure.tick <= current_tick) {
			return;
		}
		for (stored_tick& existing : buffered_ticks) {
			if (existing.tick == closure.tick) {
				return;
			}
		}
		if (buffered_ticks.size() >= options.max_buffered_ticks || closure.tick > current_tick + options.max_buffered_ticks) {
			throw runtime_exception("client exceeded the catch-up window");
		}
		stored_tick tick;
		tick.tick = closure.tick;
		for (const wire_payload& payload : closure.commands) {
			tick.commands.emplace_back(from_wire_payload(payload, 0));
		}
		sort_commands(tick.commands);
		buffered_ticks.emplace_back(std::move(tick));
	}

	bool client_session::drain_buffered_ticks() {
		u32 stepped = 0;
		while (stepped < options.max_tick_steps_per_update) {
			const Tick next_tick = current_tick + 1;
			size_t found_index = buffered_ticks.size();
			for (size_t index = 0; index < buffered_ticks.size(); ++index) {
				if (buffered_ticks[index].tick == next_tick) {
					found_index = index;
					break;
				}
			}
			if (found_index == buffered_ticks.size()) {
				return true;
			}
			stored_tick tick = std::move(buffered_ticks[found_index]);
			buffered_ticks.erase(buffered_ticks.begin() + static_cast<i64>(found_index));
			erase_confirmed_pending(pending_payloads, session_id, tick.commands);
			rebuild_pending_views();
			current_tick = tick.tick;
			push_ready_tick(tick.tick, tick.commands);
			++stepped;
		}
		const Tick next_tick = current_tick + 1;
		for (const stored_tick& tick : buffered_ticks) {
			if (tick.tick == next_tick) {
				return false;
			}
		}
		return true;
	}

	void client_session::try_finish_snapshot() {
		if (!snapshot.active || !snapshot.end_received || snapshot.received_count != snapshot.chunk_count) {
			return;
		}
		if (!snapshot.load_started) {
			snapshot.load_started = true;
			push_event(SessionEvent{
				.kind = SessionEventKind::snapshot_received,
				.tick = snapshot.baseline_tick,
				.bytes = snapshot.bytes,
			});
			return;
		}
	}

	bool client_session::valid_session(SessionId packet_session_id) const {
		return session_id != 0 && packet_session_id == session_id;
	}

	void client_session::send_connect_request() {
		const PacketSequence packet_sequence = next_send_sequence;
		++next_send_sequence;
		vector<byte> bytes = write_packet(packet_kind::connect_request, [&](bin::write_stream& stream) -> error {
			IF_ERROR_RETURN_ERROR(stream_packet_sequence(stream, "packet_sequence", packet_sequence));
			return stream(bin::field("protocol_version", protocol_version));
		});
		socket.send(host, bytes_view(bytes));
		remember_sent_packet(sent_packets, packet_sequence, bytes);
	}

	void client_session::send_join_request() {
		if (session_id == 0) {
			return;
		}
		send_connected(packet_kind::join_request, [&](bin::write_stream& stream) -> error {
			return stream(bin::field("login_payload", login_payload));
		});
	}

	void client_session::send_snapshot_request() {
		if (session_id == 0 || !snapshot.active || snapshot.load_started) {
			return;
		}
		transfer_block_requests requests;
		requests.snapshot_id = snapshot.snapshot_id;
		if (snapshot.received_count < snapshot.chunk_count) {
			++snapshot.request_round;
			const u32 window = std::max<u32>(1, options.snapshot_request_window);
			const u32 retry_delay = std::max<u32>(1, options.snapshot_request_retry_delay);
			u32 in_flight = 0;
			for (u32 chunk_index = 0; chunk_index < snapshot.chunk_count; ++chunk_index) {
				if (snapshot.received_chunks[chunk_index] == 0 && snapshot.requested_chunks[chunk_index] != 0) {
					++in_flight;
				}
			}
			u32 scanned = 0;
			u32 chunk_index = snapshot.next_request_chunk;
			while (scanned < snapshot.chunk_count && requests.ranges.size() < options.max_missing_per_nack) {
				if (chunk_index >= snapshot.chunk_count) {
					chunk_index = 0;
				}
				const bool was_requested = snapshot.requested_chunks[chunk_index] != 0;
				const bool retry_ready = !was_requested ||
										 snapshot.request_round >= snapshot.requested_chunks[chunk_index] + retry_delay;
				const bool window_ready = was_requested || in_flight < window;
				if (snapshot.received_chunks[chunk_index] == 0 && retry_ready && window_ready) {
					append_snapshot_range(requests, chunk_index, options.max_missing_per_nack);
					snapshot.requested_chunks[chunk_index] = snapshot.request_round;
					if (!was_requested) {
						++in_flight;
					}
				}
				++chunk_index;
				++scanned;
				if (requests.ranges.size() >= options.max_missing_per_nack) {
					break;
				}
			}
			snapshot.next_request_chunk = chunk_index >= snapshot.chunk_count ? 0 : chunk_index;
			if (requests.ranges.empty()) {
				return;
			}
		} else if (snapshot.end_received) {
			return;
		}
		send_connected(packet_kind::snapshot_request, [&](bin::write_stream& stream) -> error {
			return stream(bin::field("requests", requests));
		});
	}

	void client_session::send_client_heartbeat() {
		const bool sent_commands = !outgoing_commands.empty();
		client_to_server_heartbeat heartbeat;
		heartbeat.client_tick = current_tick;
		heartbeat.rejection_acks = rejection_acks;
		heartbeat.requests.sequences = take_heartbeat_requests(pending_heartbeat_requests);
		heartbeat.commands.reserve(outgoing_commands.size());
		for (const stored_payload& command : outgoing_commands) {
			heartbeat.commands.emplace_back(to_wire_payload(command));
		}
		const PacketSequence heartbeat_sequence = next_heartbeat_sequence;
		++next_heartbeat_sequence;
		send_connected(packet_kind::client_heartbeat, heartbeat_sequence, [&](bin::write_stream& stream) -> error {
			return stream(bin::field("heartbeat", heartbeat));
		});
		remember_sent_packet(sent_heartbeats, heartbeat_sequence, sent_packets.back().bytes);
		outgoing_commands.clear();
		if (sent_commands) {
			next_pending_resend_tick = current_tick + 5;
		}
	}

	void client_session::resend_pending_payloads() {
		if (pending_payloads.empty() || current_tick < next_pending_resend_tick) {
			return;
		}
		if (!outgoing_commands.empty()) {
			return;
		}
		for (const pending_payload& pending : pending_payloads) {
			outgoing_commands.emplace_back(stored_payload{
				.id = pending.id,
				.source = session_id,
				.hash = pending.hash,
				.bytes = pending.bytes,
			});
		}
		next_pending_resend_tick = current_tick + 5;
	}

	void client_session::request_heartbeats(vector<PacketSequence> missing) {
		if (missing.empty() || session_id == 0) {
			return;
		}
		for (PacketSequence sequence : missing) {
			append_unique(pending_heartbeat_requests, sequence, options.max_missing_per_nack);
		}
	}

	void client_session::resend_heartbeats(const vector<PacketSequence>& requested) {
		resend_packets(socket, host, sent_heartbeats, requested, options.max_resends_per_update);
	}

	bool client_session::waiting_for_response() const {
		if (session_state != state::joined) {
			return false;
		}
		const i64 warning_quantums = std::min(
			options.connect_timeout.quantum_count(),
			std::max<i64>(1'000'000'000, options.handshake_interval.quantum_count() * 2)
		);
		const instant current_time = options.clock();
		const duration time_since_activity = duration::from_quantum(current_time.quantum_count() - last_connect_activity.quantum_count());
		return time_since_activity >= duration::from_quantum(warning_quantums);
	}

	template<typename Writer>
	void client_session::send_connected(packet_kind kind, Writer writer) {
		const PacketSequence packet_sequence = next_send_sequence;
		++next_send_sequence;
		send_connected(kind, packet_sequence, writer);
	}

	template<typename Writer>
	void client_session::send_connected(packet_kind kind, PacketSequence sequence, Writer writer) {
		vector<byte> bytes = write_connected_packet(kind, session_id, sequence, writer);
		socket.send(host, bytes_view(bytes));
		remember_sent_packet(sent_packets, sequence, bytes);
	}

	void Session::ImplDeleter::operator()(Impl* impl) const noexcept {
		if (impl) {
			try {
				impl->disconnect();
			} catch (const exception&) {
			}
		}
		delete impl;
	}

	Session::Session(unique_ptr<Impl> impl) : impl(impl.release()) {}
	Session::Session(Session&& other) noexcept = default;
	Session& Session::operator=(Session&& other) noexcept = default;

	Session::~Session() {
		impl.reset();
	}

	Session Session::Offline(Options options) {
		return Session(make_unique<offline_session>(options));
	}

	Session Session::Host(net::Socket socket, Options options) {
		return Session(make_unique<host_session>(std::move(socket), options));
	}

	Session Session::Client(net::Socket socket, net::Peer host, Options options) {
		return Session(make_unique<client_session>(std::move(socket), std::move(host), options));
	}

	Session::operator bool() const noexcept {
		return impl && impl->session_state != state::disconnected;
	}

	void Session::update() {
		if (!impl) {
			throw runtime_exception("lockstep session is not open");
		}
		impl->update();
	}

	void Session::advance() {
		if (!impl) {
			throw runtime_exception("lockstep session is not open");
		}
		impl->advance();
	}

	void Session::disconnect() {
		if (impl) {
			impl->disconnect();
		}
	}

	void Session::disconnect_peer(SessionId session_id) {
		if (!impl || impl->session_mode != mode::host) {
			throw runtime_exception("only host lockstep sessions can disconnect peers");
		}
		static_cast<host_session&>(*impl).disconnect_peer(session_id);
	}

	PayloadId Session::submit(span<const byte> bytes) {
		if (!impl) {
			throw runtime_exception("lockstep session is not open");
		}
		return impl->submit(bytes);
	}
<<<<<<< Updated upstream
=======
	bool Session::Impl::admit(stored_payload& payload) {
		auto accepted = payload.bytes.size() > options.max_command_bytes
			? report<vector<byte>>{ unexpected(error{ generic_errc::input_error, "command exceeds size limit" }) }
			: command_admitter ? command_admitter(payload.source, payload.bytes) : report<vector<byte>>{ payload.bytes };
		if (!accepted) {
			std::erase_if(pending_payloads, [&](const auto& value) { return value.id == payload.id; });
			rebuild_pending_views();
			push_event(SessionEvent{ .kind = SessionEventKind::command_rejected, .command_id = payload.id, .message = accepted.error().message });
			return false;
		}
		payload.bytes = std::move(*accepted);
		return true;
	}

	void Session::set_command_admitter(std::function<report<vector<byte>>(ID, span<const byte>)> admitter) {
		if (!impl || impl->session_mode == mode::client) {
			throw runtime_exception("command admission belongs to the authority");
		}
		impl->command_admitter = std::move(admitter);
	}

	Command::ID Session::submit_authoritative(span<const byte> bytes) {
		if (!impl || impl->session_mode == mode::client || bytes.size() > impl->options.max_command_bytes) {
			throw runtime_exception("invalid authoritative command submission");
		}
		auto payload = impl->make_payload(bytes, ID{}, false);
		const auto id{ payload.id };
		if (impl->session_mode == mode::offline) {
			static_cast<offline_session&>(*impl).staged_commands.push_back(std::move(payload));
		} else {
			static_cast<host_session&>(*impl).schedule_commands({ std::move(payload) });
		}
		return id;
	}
>>>>>>> Stashed changes

	vector<ReadyTick> Session::take_ready_ticks() {
		if (!impl) {
			return {};
		}
		vector<ReadyTick> ticks = std::move(impl->ready_ticks);
		impl->ready_ticks.clear();
		return ticks;
	}

	vector<SessionEvent> Session::take_events() {
		if (!impl) {
			return {};
		}
		vector<SessionEvent> events = std::move(impl->events);
		impl->events.clear();
		return events;
	}

	optional<SnapshotProgress> Session::incoming_snapshot() const {
		if (!impl || impl->session_mode != mode::client) {
			return nullopt;
		}
		const auto& client = static_cast<const client_session&>(*impl);
		if (!client.snapshot.active) {
			return nullopt;
		}
		SnapshotProgress p;
		p.session_id = client.session_id;
		p.chunks_done = client.snapshot.received_count;
		p.chunks_total = client.snapshot.chunk_count;
		p.bytes_done = client.snapshot.received_bytes;
		p.bytes_total = static_cast<u64>(client.snapshot.bytes.size());
		// Clamp because the final chunk is short and the multiplication above
		// can overrun the true byte total by chunk_size-1.
		if (p.bytes_done > p.bytes_total) {
			p.bytes_done = p.bytes_total;
		}
		return p;
	}

	vector<SnapshotProgress> Session::outgoing_snapshots() const {
		vector<SnapshotProgress> out;
		if (!impl || impl->session_mode != mode::host) {
			return out;
		}
		const auto& host = static_cast<const host_session&>(*impl);
		for (const host_connection& c : host.connections) {
			// Include a peer in the "Saving the world for X" overlay while
			// the lockstep machinery is still on the join path for them:
			//   - downloading_snapshot: the framework's own state, true
			//     during the bulk send.
			//   - joined but no heartbeat seen yet: bulk send finished, but
			//     the client hasn't completed snapshot decode + catch-up.
			//     The framework flips connection_state to joined the moment
			//     send_snapshot_end goes out the door, even though the peer
			//     can still be busy receiving missing chunks via NACK or
			//     decoding the world. The overlay stays up until the peer
			//     starts heartbeating, which is the real "I'm playing" signal.
			const bool in_join_handshake =
				c.connection_state == state::downloading_snapshot ||
				(c.connection_state == state::joined && !c.client_heartbeat_seen);
			if (!in_join_handshake) {
				continue;
			}
			SnapshotProgress p;
			p.session_id = c.session_id;
			p.chunks_done = c.next_snapshot_chunk;
			p.chunks_total = c.snapshot_chunk_count;
			p.bytes_done = static_cast<u64>(c.next_snapshot_chunk) * static_cast<u64>(c.snapshot_chunk_size);
			p.bytes_total = static_cast<u64>(c.snapshot_bytes.size());
			if (p.bytes_done > p.bytes_total) {
				p.bytes_done = p.bytes_total;
			}
			out.push_back(p);
		}
		return out;
	}

	void Session::set_login_payload(span<const byte> bytes) {
		if (!impl) {
			throw runtime_exception("lockstep session is not open");
		}
		impl->login_payload.assign(bytes.begin(), bytes.end());
	}

<<<<<<< Updated upstream
	void Session::accept_login(SessionId session_id, span<const byte> snapshot) {
=======
	void Session::accept_login(ID session_id, Tick baseline, span<const byte> snapshot) {
>>>>>>> Stashed changes
		if (!impl || impl->session_mode != mode::host) {
			throw runtime_exception("only host lockstep sessions can accept logins");
		}
		static_cast<host_session&>(*impl).accept_login(session_id, baseline, snapshot);
	}

<<<<<<< Updated upstream
	void Session::reject_login(SessionId session_id) {
=======
	void Session::reject_login(ID session_id, string_view reason) {
>>>>>>> Stashed changes
		if (!impl || impl->session_mode != mode::host) {
			throw runtime_exception("only host lockstep sessions can reject logins");
		}
		static_cast<host_session&>(*impl).reject_login(session_id, reason);
	}

	void Session::finish_snapshot_load() {
		if (!impl || impl->session_mode != mode::client) {
			throw runtime_exception("only client lockstep sessions load snapshots");
		}
		static_cast<client_session&>(*impl).finish_snapshot_load();
	}

	mode Session::mode() const {
		return impl ? impl->session_mode : mode::offline;
	}

	state Session::state() const {
		return impl ? impl->session_state : state::disconnected;
	}

	SessionId Session::local_session_id() const {
		return impl ? impl->local_session : 0;
	}

	Tick Session::tick() const {
		return impl ? impl->current_tick : 0;
	}

	bool Session::joined() const {
		return impl && impl->session_state == state::joined;
	}

	bool Session::waiting_for_response() const {
		return impl && impl->waiting_for_response();
	}

	span<const PendingPayload> Session::pending() const {
		if (!impl) {
			return {};
		}
		return span<const PendingPayload>(impl->pending_views.data(), impl->pending_views.size());
	}
} // namespace lf::lockstep
