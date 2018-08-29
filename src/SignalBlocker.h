
#ifndef SIGNAL_BLOCKER_H_
#define SIGNAL_BLOCKER_H_

#include <utility>

template <class T>
class SignalBlocker {
public:
	explicit SignalBlocker(T *blocked_) : blocked_(blocked_), previous_(blocked_->blockSignals(true)) {
	}

	~SignalBlocker() {
		if(blocked_) {
			blocked_->blockSignals(previous_);
		}
	}

	SignalBlocker(const SignalBlocker &)            = delete;
	SignalBlocker& operator=(const SignalBlocker &) = delete;

	SignalBlocker(SignalBlocker &&other) noexcept : blocked_(std::exchange(other.blocked_, nullptr)), previous_(std::exchange(other.previous_, false)) {
	}

	SignalBlocker& operator=(SignalBlocker &&rhs) noexcept {
		blocked_  = std::exchange(rhs.blocked_, nullptr);
		previous_ = std::exchange(rhs.previous_, false);
		return *this;
	}

	T *operator->() {
		return blocked_;
	}

	explicit operator bool() const { return blocked_ != nullptr; }

private:
	T   *blocked_;
	bool previous_;
};

template <class T>
SignalBlocker<T> no_signals(T *obj) {
	return SignalBlocker<T>(obj);
}

#endif
