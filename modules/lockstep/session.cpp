#include "leaf/lockstep/session.hpp"
#include "leaf/core/exception.hpp"

namespace lf::lockstep {
	Session::Session(ID owner, State state) : owner(owner), status(state) {}
	Session::~Session() = default;

	Session::ID Session::local_session_id() const { return owner; }
	u64 Session::tick() const { return current_tick; }
	Session::State Session::state() const { return status; }
	bool Session::joined() const { return status == State::joined; }
	vector<Session::Frame> Session::take_ready_ticks() { return std::exchange(ready, {}); }
	vector<Session::Event> Session::take_events() { return std::exchange(events, {}); }

	Session::Input Session::make_input(span<const byte> bytes) {
		if (!joined()) { throw runtime_exception("Session is not ready for input"); }

		Input input{ next_input, owner, { bytes.begin(), bytes.end() } };
		next_input = Input::ID::from_raw(next_input.get() + 1);
		return input;
	}

	LocalSession::LocalSession() : Session(ID{ 0 }, State::joined) {}
	void LocalSession::update() {}
	void LocalSession::disconnect() { status = State::disconnected; }

	Session::Input::ID LocalSession::submit(span<const byte> bytes) {
		inputs.push_back(make_input(bytes));
		return inputs.back().id;
	}

	void LocalSession::advance() {
		if (!joined()) { return; }
		ready.push_back(Frame{ ++current_tick, std::exchange(inputs, {}) });
	}
}
