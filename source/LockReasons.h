
#ifndef LOCK_REASONS_H_
#define LOCK_REASONS_H_

#include <cstdint>

class LockReasons {
private:
	// This handles all the different reasons files can be locked
	constexpr static const uint32_t USER_LOCKED_BIT                 = 1;
	constexpr static const uint32_t PERM_LOCKED_BIT                 = 2;
	constexpr static const uint32_t TOO_MUCH_BINARY_DATA_LOCKED_BIT = 4;
	
public:
	LockReasons() : reasons_(0) {
	}
	
	LockReasons(const LockReasons &) = default;
	LockReasons(LockReasons &&) = default;
	LockReasons& operator=(const LockReasons &) = default;
	LockReasons& operator=(LockReasons &&) = default;

public:
	bool isUserLocked() const {
		return (reasons_ & USER_LOCKED_BIT) != 0;
	}
	
	bool isPermLocked() const {
		return (reasons_ & PERM_LOCKED_BIT) != 0;
	}
	
	bool isTMBDLocked() const {
		return (reasons_ & TOO_MUCH_BINARY_DATA_LOCKED_BIT) != 0;
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
	
	void setTMBDLocked(bool enabled) {
		setLockedByReason(enabled, TOO_MUCH_BINARY_DATA_LOCKED_BIT);
	}
	
private:
	void setLockedByReason(bool enabled, uint32_t reasonBit) {
		if(enabled) {
			reasons_ |= reasonBit;
		} else {
			reasons_ &= ~reasonBit;
		}
	}

private:
	uint32_t reasons_;
};

#endif
