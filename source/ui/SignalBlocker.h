
#ifndef SIGNAL_BLOCKER_H_
#define SIGNAL_BLOCKER_H_

template <class T>
class SignalBlocker {
public:
	explicit SignalBlocker(T *blocked_) : blocked_(blocked_), previous_(blocked_->blockSignals(true)) {
	}
	
	~SignalBlocker() {
		blocked_->blockSignals(previous_);
	}
	
	SignalBlocker(const SignalBlocker &)            = default;
	SignalBlocker(SignalBlocker &&)                 = default;
	SignalBlocker& operator=(const SignalBlocker &) = default;
	SignalBlocker& operator=(SignalBlocker &&)      = default;
	
	T *operator->() {
		return blocked_;
	}

private:
	T   *blocked_;
	bool previous_;
};

template <class T>
SignalBlocker<T> no_signals(T *blocked_) {
	return SignalBlocker<T>(blocked_);
}

#endif
