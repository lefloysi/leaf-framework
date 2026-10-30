#include "leaf/store/progression.hpp"

#include <leaf/progression/progression.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace lf::store {
	ProgressionBackend::~ProgressionBackend() = default;

	StoreProgression::StoreProgression() : backend_(&store_progression_backend()) {
	}

	StoreProgression::StoreProgression(ProgressionBackend& backend) : backend_(&backend) {
	}

	bool StoreProgression::pending() const noexcept {
		return submitted_;
	}

	error StoreProgression::synchronize(Progression& progression) {
		if (!backend_->supported()) {
			return error::no_error;
		}
		if (owner_ && owner_ != &progression) {
			return error(generic_errc::invalid_state, "A store synchronizer belongs to one progression instance.");
		}
		owner_ = &progression;
		if (submitted_) {
			if (backend_->status() == progression_commit_state::pending) {
				return error::no_error;
			}
			if (backend_->status() != progression_commit_state::succeeded) {
				auto failure = backend_->last_error();
				// Retry the same store-cache transaction; never apply average-rate samples twice.
				if (auto err = backend_->commit(); err) {
					return err;
				}
				return failure;
			}
			for (const auto& write : stat_writes_) {
				progression.acknowledge_stat(write.id, write.revision, write.rate_samples);
			}
			for (const auto& write : achievement_writes_) {
				progression.acknowledge_achievement(write.id, write.revision);
			}
			stat_writes_.clear();
			achievement_writes_.clear();
			submitted_ = false;
		}

		error first_error;
		for (const auto& definition : vector<Stat>(progression.stats())) {
			Stat stat = *progression.find_stat(definition.id);
			if (stat.prototype.store_id.empty()) {
				continue;
			}
			if (std::find(imported_stats_.begin(), imported_stats_.end(), stat.id) == imported_stats_.end()) {
				error err;
				if (std::holds_alternative<i64>(stat.value)) {
					auto remote = backend_->read_integer(stat.prototype.store_id);
					err = remote ? ((stat.prototype.increment_only || stat.revision <= stat.synced_revision) ? progression.merge_stat(stat.id, i64(*remote)) : error::no_error) : remote.error();
				}
				else {
					auto remote = backend_->read_float(stat.prototype.store_id);
					err = remote ? ((stat.prototype.increment_only || stat.revision <= stat.synced_revision) ? progression.merge_stat(stat.id, f64(*remote)) : error::no_error) : remote.error();
				}
				if (err) {
					if (!first_error) {
						first_error = err;
					}
					continue;
				}
				imported_stats_.push_back(stat.id);
				stat = *progression.find_stat(definition.id);
			}
			if (stat.revision <= stat.synced_revision) {
				continue;
			}
			error err;
			usize rate_samples = 0;
			if (auto value = std::get_if<i64>(&stat.value)) {
				if (*value < std::numeric_limits<i32>::min() || *value > std::numeric_limits<i32>::max()) {
					err = error(generic_errc::out_of_range, "A Steam integer stat must fit in 32 bits.");
				}
				else {
					err = backend_->set_integer(stat.prototype.store_id, i32(*value));
				}
			}
			else if (auto value = std::get_if<f64>(&stat.value)) {
				if (!std::isfinite(*value) || std::abs(*value) > std::numeric_limits<f32>::max()) {
					err = error(generic_errc::out_of_range, "A Steam float stat must fit in 32 bits.");
				}
				else {
					err = backend_->set_float(stat.prototype.store_id, f32(*value));
				}
			}
			else {
				f64 amount = 0;
				f64 seconds = 0;
				for (const auto& sample : stat.pending_rates) {
					amount += sample.amount;
					seconds += sample.seconds;
				}
				if (stat.pending_rates.empty()) {
					progression.acknowledge_stat(stat.id, stat.revision);
					continue;
				}
				if (!std::isfinite(amount) || !std::isfinite(seconds) || seconds <= 0 || std::abs(amount) > std::numeric_limits<f32>::max()) {
					err = error(generic_errc::out_of_range, "Average-rate samples exceed the store's numeric range.");
				}
				else {
					err = backend_->update_average(stat.prototype.store_id, f32(amount), seconds);
					rate_samples = stat.pending_rates.size();
				}
			}
			if (err) {
				if (!first_error) {
					first_error = err;
				}
				continue;
			}
			stat_writes_.push_back({ stat.id, stat.revision, rate_samples });
		}

		for (const auto& definition : vector<Achievement>(progression.achievements())) {
			Achievement achievement = *progression.find_achievement(definition.id);
			if (achievement.prototype.store_id.empty()) {
				continue;
			}
			if (std::find(imported_achievements_.begin(), imported_achievements_.end(), achievement.id) == imported_achievements_.end()) {
				auto remote = backend_->read_achievement(achievement.prototype.store_id);
				if (!remote) {
					if (!first_error) {
						first_error = remote.error();
					}
					continue;
				}
				if (*remote) {
					if (auto err = progression.complete_achievement(achievement.id); err) {
						if (!first_error) {
							first_error = err;
						}
						continue;
					}
				}
				imported_achievements_.push_back(achievement.id);
				achievement = *progression.find_achievement(definition.id);
			}
			if (!achievement.completed || achievement.revision <= achievement.synced_revision) {
				continue;
			}
			if (auto err = backend_->unlock(achievement.prototype.store_id); err) {
				if (!first_error) {
					first_error = err;
				}
				continue;
			}
			achievement_writes_.push_back({ achievement.id, achievement.revision, 0 });
		}
		if (!stat_writes_.empty() || !achievement_writes_.empty()) {
			submitted_ = true;
			if (auto err = backend_->commit(); err) {
				return err;
			}
		}
		return first_error;
	}
} // namespace lf::store



