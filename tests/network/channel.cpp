#include <catch2/catch_test_macros.hpp>
#include <leaf/network/channel.hpp>
#include <leaf/lockstep/session.hpp>
#include <leaf/core/scope.hpp>
#include <leaf/system/socket.hpp>
#include <array>

TEST_CASE("Channel preserves messages through loss, duplication and reordering", "[network]") {
	using namespace lf;
	REQUIRE_FALSE(sys::init_udp_sockets());
	scope_exit cleanup{ sys::exit_udp_sockets };
	auto left_socket = net::Socket::Port(41480);
	auto right_socket = net::Socket::Port(41481);
	auto relay = net::Socket::Port(41482);
	const auto left_peer = net::Peer::Address("127.0.0.1", 41480);
	const auto right_peer = net::Peer::Address("127.0.0.1", 41481);
	const auto relay_peer = net::Peer::Address("127.0.0.1", 41482);
	net::Channel left{ relay_peer, 17 }, right{ relay_peer, 17 };
	vector<byte> large(700'013);
	for (usize index = 0; index < large.size(); ++index) { large[index] = byte(index % 251); }
	const vector<byte> small{ byte{ 3 }, byte{ 4 }, byte{ 9 } };
	left.send(large);
	left.send({});
	left.send(small);
	right.send(small);
	vector<vector<byte>> received_left, received_right;
	std::array<byte, 65507> buffer;
	usize packet_count = 0;
	bool appended = false;
	optional<net::Peer> held_peer;
	vector<byte> held;
	const auto deadline = now() + duration::from_quantum(10'000'000'000);
	while (now() < deadline && (received_left.size() != 2 || received_right.size() != 3)) {
		if (!appended && packet_count > 100) { right.send(large); appended = true; }
		left.update(left_socket);
		right.update(right_socket);
		while (auto message = relay.recv(buffer)) {
			const auto& destination = message->first.channel() == left_peer.channel() ? right_peer : left_peer;
			++packet_count;
			if (packet_count % 7 == 0) { continue; }
			if (packet_count % 11 == 0 && !held_peer) {
				held_peer = destination;
				held.assign(message->second.begin(), message->second.end());
				continue;
			}
			relay.send(destination, message->second);
			if (packet_count % 13 == 0) { relay.send(destination, message->second); }
			if (held_peer) {
				relay.send(*held_peer, held);
				held_peer.reset();
			}
		}
		const auto collect = [&](net::Socket& socket, net::Channel& channel, auto& received) {
			while (auto message = socket.recv(buffer)) {
				auto packet = net::Channel::read(message->second);
				REQUIRE(packet);
				REQUIRE_FALSE(channel.receive(*packet));
			}
			for (auto& bytes : channel.take_messages()) { received.push_back(std::move(bytes)); }
		};
		collect(left_socket, left, received_left);
		collect(right_socket, right, received_right);
		sleep_for(duration::from_quantum(1'000'000));
	}
	REQUIRE(received_left.size() == 2);
	REQUIRE(received_right.size() == 3);
	CHECK(received_left[0] == small);
	CHECK(received_left[1] == large);
	CHECK(received_right[0] == large);
	CHECK(received_right[1].empty());
	CHECK(received_right[2] == small);
}

TEST_CASE("Remote connection timeout exposes elapsed progress", "[network]") {
	using namespace lf;
	REQUIRE_FALSE(sys::init_udp_sockets());
	scope_exit cleanup{ sys::exit_udp_sockets };
	lockstep::RemoteSession remote{ net::Socket::Port(0), net::Peer::Address("127.0.0.1", 41483),
		{ .connect_timeout = duration::from_quantum(50'000'000) } };
	remote.set_login_payload({});
	const auto deadline = now() + duration::from_quantum(500'000'000);
	f64 previous = 0;
	while (remote.state() != lockstep::Session::State::disconnected && now() < deadline) {
		remote.update();
		const auto progress = remote.connection_wait();
		CHECK(progress >= previous);
		previous = progress;
		sleep_for(duration::from_quantum(2'000'000));
	}
	REQUIRE(remote.state() == lockstep::Session::State::disconnected);
	CHECK(remote.connection_wait() == 1);
	const auto events = remote.take_events();
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == lockstep::Session::Event::Type::disconnected);
}

TEST_CASE("Join acceptance precedes slow snapshot preparation and loading", "[network]") {
	using namespace lf;
	REQUIRE_FALSE(sys::init_udp_sockets());
	scope_exit cleanup{ sys::exit_udp_sockets };
	lockstep::HostSession host{ net::Socket::Port(41484), { .connect_timeout = duration::from_quantum(50'000'000) } };
	lockstep::RemoteSession remote{ net::Socket::Port(0), net::Peer::Address("127.0.0.1", 41484),
		{ .connect_timeout = duration::from_quantum(50'000'000) } };
	remote.set_login_payload({});
	lockstep::Session::ID player;
	bool prepare = false;
	const auto deadline = now() + duration::from_quantum(1'000'000'000);
	while (!prepare && now() < deadline) {
		remote.update();
		host.update();
		for (const auto& event : host.take_events()) {
			if (event.type == lockstep::Session::Event::Type::login_requested) { host.accept_login(event.session_id); }
			if (event.type == lockstep::Session::Event::Type::snapshot_requested) { player = event.session_id; prepare = true; }
		}
		sleep_for(duration::from_quantum(1'000'000));
	}
	REQUIRE(prepare);
	REQUIRE(remote.state() == lockstep::Session::State::preparing_snapshot);
	const auto prepared = now() + duration::from_quantum(100'000'000);
	while (now() < prepared) { remote.update(); sleep_for(duration::from_quantum(1'000'000)); }
	REQUIRE(remote.state() == lockstep::Session::State::preparing_snapshot);
	const vector<byte> snapshot{ byte{ 1 }, byte{ 2 } };
	host.send_snapshot(player, snapshot);
	bool received = false;
	while (!received && now() < deadline) {
		host.update(); remote.update();
		for (const auto& event : remote.take_events()) {
			if (event.type == lockstep::Session::Event::Type::snapshot_received) { REQUIRE(event.bytes == snapshot); received = true; }
		}
		sleep_for(duration::from_quantum(1'000'000));
	}
	REQUIRE(received);
	const auto loaded = now() + duration::from_quantum(100'000'000);
	while (now() < loaded) { host.update(); sleep_for(duration::from_quantum(1'000'000)); }
	CHECK(host.take_events().empty());
	remote.finish_snapshot_load();
	host.advance();
	while (!remote.joined() && now() < deadline) { host.update(); remote.update(); sleep_for(duration::from_quantum(1'000'000)); }
	REQUIRE(remote.joined());
	CHECK(remote.tick() == host.tick());
}
