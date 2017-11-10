
#ifndef TRUNC_SUBSTITUTION_H_
#define TRUNC_SUBSTITUTION_H_

#include <QMetaType>

enum class TruncSubstitution {
    Silent,
    Fail,
    Warn,
    Ignore,
};

Q_DECLARE_METATYPE(TruncSubstitution)

#endif
