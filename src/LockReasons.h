
#ifndef LOCK_REASONS_H_
#define LOCK_REASONS_H_

#include <cstdint>

class LockReasons {
private:
	enum Reason : uint32_t {
		USER_LOCKED_BIT = 1,
		PERM_LOCKED_BIT = 2,
	};

public:
	bool isUserLocked() const {
		return (reasons_ & USER_LOCKED_BIT) != 0;
	}

	bool isPermLocked() const {
		return (reasons_ & PERM_LOCKED_BIT) != 0;
	}

	bool isAnyLockedIgnoringUser() const {
		return (reasons_ & ~USER_LOCKED_BIT) != 0;
	}

	bool isAnyLockedIgnoringPerm() const {
		return (reasons_ & ~PERM_LOCKED_BIT) != 0;
	}

	bool isAnyLocked() const {
		return reasons_ != 0;
	}

	void clear() {
		reasons_ = 0;
	}

	void setUserLocked(bool enabled) {
		setLockedByReason(enabled, USER_LOCKED_BIT);
	}

	void setPermLocked(bool enabled) {
		setLockedByReason(enabled, PERM_LOCKED_BIT);
	}

private:
	void setLockedByReason(bool enabled, Reason reason) {
		if (enabled) {
			reasons_ |= reason;
		} else {
			reasons_ &= ~reason;
		}
	}

private:
	uint32_t reasons_ = 0;
};

#endif
