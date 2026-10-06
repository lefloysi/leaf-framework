#include "leaf/network/socket.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/store/network.hpp"
#include "leaf/system/socket.hpp"

namespace lf::net {
	struct Socket::Impl {
		virtual ~Impl() = default;

		virtual void send(string_view address, u16 channel, span<const byte> data) = 0;
		virtual optional<Message> recv(span<byte> buffer) = 0;
	};

	struct socket_udp final : Socket::Impl {
		explicit socket_udp(u16 port) : socket(lf::sys::open_socket_udp(port)) {}

		~socket_udp() override {
			lf::sys::close_socket_udp(socket);
		}

		void send(string_view address, u16 channel, span<const byte> data) override {
			lf::sys::send_socket_udp(socket, address, channel, data);
		}

		optional<Message> recv(span<byte> buffer) override {
			const optional<lf::sys::message_udp> message = lf::sys::recv_socket_udp(socket, buffer);
			if (!message) {
				return std::nullopt;
			}

			return Message{ Peer::Address(message->address, message->port), message->data };
		}

		lf::sys::socket_udp socket;
	};

	struct socket_store final : Socket::Impl {
		explicit socket_store(u16 channel) : local_channel(channel) {
			store_provider::open_channel(local_channel);
		}

		~socket_store() override {
			store_provider::close_channel(local_channel);
		}

		void send(string_view address, u16 channel, span<const byte> data) override {
			store_provider::send(local_channel, address, channel, data);
		}

		optional<Message> recv(span<byte> buffer) override {
			return store_provider::recv(local_channel, buffer);
		}

		u16 local_channel;
	};

	Socket::Socket(unique_ptr<Impl> impl) : impl(std::move(impl)) {}

	Socket::Socket(Socket&& other) noexcept = default;
	Socket& Socket::operator=(Socket&& other) noexcept = default;
	Socket::~Socket() = default;

	Socket::operator bool() const noexcept {
		return impl != nullptr;
	}

	void Socket::disconnect() {
		impl.reset();
	}

	Socket Socket::Port(u16 port) {
		return Socket(make_unique<socket_udp>(port));
	}

	Socket Socket::Channel(u16 channel) {
		return Socket(make_unique<socket_store>(channel));
	}

	void Socket::send(const Peer& peer, span<const byte> data) {
		if (!impl) {
			throw runtime_exception("network socket is not open");
		}
		impl->send(peer.m_id, peer.m_channel, data);
	}

	optional<Message> Socket::recv(span<byte> buffer) {
		if (!impl) {
			throw runtime_exception("network socket is not open");
		}
		return impl->recv(buffer);
	}
} // namespace lf::net
