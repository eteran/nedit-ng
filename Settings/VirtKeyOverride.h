
#ifndef VIRT_KEY_OVERRIDE_H_
#define VIRT_KEY_OVERRIDE_H_

#include <QtDebug>

enum class VirtKeyOverride {
    VIRT_KEY_OVERRIDE_NEVER,
    VIRT_KEY_OVERRIDE_AUTO,
    VIRT_KEY_OVERRIDE_ALWAYS
};

template <class T>
inline T from_integer(int value);

template <>
inline VirtKeyOverride from_integer(int value) {
    switch(value) {
    case static_cast<int>(VirtKeyOverride::VIRT_KEY_OVERRIDE_NEVER):
    case static_cast<int>(VirtKeyOverride::VIRT_KEY_OVERRIDE_AUTO):
    case static_cast<int>(VirtKeyOverride::VIRT_KEY_OVERRIDE_ALWAYS):
        return static_cast<VirtKeyOverride>(value);
    default:
        qWarning("NEdit: Invalid value for VirtKeyOverride");
        return VirtKeyOverride::VIRT_KEY_OVERRIDE_NEVER;
    }
}

#endif
