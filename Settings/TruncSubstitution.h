
#ifndef TRUNC_SUBSTITUTION_H_
#define TRUNC_SUBSTITUTION_H_

#include "FromInteger.h"

#include <QtDebug>

enum class TruncSubstitution {
	Silent,
	Fail,
	Warn,
	Ignore,
};

template <>
inline TruncSubstitution FromInteger(int value) {
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
