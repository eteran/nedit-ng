
#ifndef TRUNC_SUBSTITUTION_H_
#define TRUNC_SUBSTITUTION_H_

#include "from_integer.h"
#include <QtDebug>

enum class TruncSubstitution {
	Silent,
	Fail,
	Warn,
	Ignore,
};

template <>
inline TruncSubstitution from_integer(int value) {
	switch (value) {
	case static_cast<int>(TruncSubstitution::Silent):
	case static_cast<int>(TruncSubstitution::Fail):
	case static_cast<int>(TruncSubstitution::Warn):
	case static_cast<int>(TruncSubstitution::Ignore):
		return static_cast<TruncSubstitution>(value);
	default:
		qWarning("NEdit: Invalid value for TruncSubstitution");
		return TruncSubstitution::Silent;
	}
}

#endif
