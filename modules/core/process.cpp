#include "leaf/core/process.hpp"

namespace lf {
	Process::Process(string_view label, Work work) { start(label, std::move(work)); }
	Process::~Process() { finish(); }
	void Process::start(string_view label, Work work) {
		if (worker.joinable()) {
			if (!ready()) { throw invalid_argument_exception("A process is already running"); }
			worker.join();
		}
		progress_ = Progress(label);
		progress_.add(label);
		std::promise<error> promise;
		completion = promise.get_future().share();
		worker = std::jthread([work, progress = progress_(), promise = std::move(promise)](std::stop_token stop) mutable {
			progress.stop = stop;
			try {
				promise.set_value(work(std::move(progress)));
			} catch (...) {
				promise.set_value(current_exception_error());
			}
		});
	}
	bool Process::ready() const {
		return completion.valid() && completion.wait_for(std::chrono::seconds{}) == std::future_status::ready;
	}
	void Process::result() {
		if (!completion.valid()) { throw invalid_argument_exception("No process has been started"); }
		if (const error status = completion.get()) { throw runtime_exception(status.message); }
	}
	void Process::cancel() { worker.request_stop(); }
	void Process::finish() {
		cancel();
		if (worker.joinable()) { worker.join(); }
	}
}

