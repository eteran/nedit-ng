
#include "Util/System.h"

/**
 * @brief Gets the operating system that NEdit was built on.
 *
 * @return The operating system name.
 */
QLatin1String BuildOperatingSystem() {
#if defined(Q_OS_AIX)
	return QLatin1String("AIX");
#elif defined(Q_OS_BSD4)
	return QLatin1String("BSD4");
#elif defined(Q_OS_BSDI)
	return QLatin1String("BSDI");
#elif defined(Q_OS_CYGWIN)
	return QLatin1String("Cygwin");
#elif defined(Q_OS_DARWIN)
	return QLatin1String("Darwin");
#elif defined(Q_OS_DGUX)
	return QLatin1String("DGUX");
#elif defined(Q_OS_DYNIX)
	return QLatin1String("Dynix");
#elif defined(Q_OS_FREEBSD)
	return QLatin1String("FreeBSD");
#elif defined(Q_OS_HPUX)
	return QLatin1String("HPUX");
#elif defined(Q_OS_HURD)
	return QLatin1String("HURD");
#elif defined(Q_OS_IRIX)
	return QLatin1String("IRIX");
#elif defined(Q_OS_LINUX)
	return QLatin1String("Linux");
#elif defined(Q_OS_LYNX)
	return QLatin1String("Lynx");
#elif defined(Q_OS_MAC)
	return QLatin1String("OSX");
#elif defined(Q_OS_MSDOS)
	return QLatin1String("MS-DOS");
#elif defined(Q_OS_NETBSD)
	return QLatin1String("NetBSD");
#elif defined(Q_OS_OS2)
	return QLatin1String("OS/2");
#elif defined(Q_OS_OPENBSD)
	return QLatin1String("OpenBSD");
#elif defined(Q_OS_OS2EMX)
	return QLatin1String("OS/2 EMX");
#elif defined(Q_OS_OSF)
	return QLatin1String("OSF");
#elif defined(Q_OS_QNX)
	return QLatin1String("QNX");
#elif defined(Q_OS_RELIANT)
	return QLatin1String("Reliant");
#elif defined(Q_OS_SCO)
	return QLatin1String("SCO");
#elif defined(Q_OS_SOLARIS)
	return QLatin1String("Solaris");
#elif defined(Q_OS_SYMBIAN)
	return QLatin1String("Symbian");
#elif defined(Q_OS_ULTRIX)
	return QLatin1String("Ultrix");
#elif defined(Q_OS_UNIX)
	return QLatin1String("Unix");
#elif defined(Q_OS_UNIXWARE)
	return QLatin1String("Unixware");
#elif defined(Q_OS_WIN32)
	return QLatin1String("Windows");
#elif defined(Q_OS_WINCE)
	return QLatin1String("WinCE");
#else
	return QLatin1String("<Unknown OS>");
#endif
}

/**
 * @brief Gets the architecture that NEdit was built for.
 *
 * @return The architecture name.
 */
QLatin1String BuildArchitecture() {
#if defined(Q_PROCESSOR_ALPHA)
	return QLatin1String("alpha");
#elif defined(Q_PROCESSOR_ARM_32)
	return QLatin1String("arm");
#elif defined(Q_PROCESSOR_ARM_64)
	return QLatin1String("arm64");
#elif defined(Q_PROCESSOR_AVR32)
	return QLatin1String("avr32");
#elif defined(Q_PROCESSOR_BLACKFIN)
	return QLatin1String("bfin");
#elif defined(Q_PROCESSOR_X86_32)
	return QLatin1String("x86");
#elif defined(Q_PROCESSOR_X86_64)
	return QLatin1String("x86_64");
#elif defined(Q_PROCESSOR_IA64)
	return QLatin1String("ia64");
#elif defined(Q_PROCESSOR_MIPS_64)
	return QLatin1String("mips64");
#elif defined(Q_PROCESSOR_MIPS)
	return QLatin1String("mips");
#elif defined(Q_PROCESSOR_POWER_32)
	return QLatin1String("power");
#elif defined(Q_PROCESSOR_POWER_64)
	return QLatin1String("power64");
#elif defined(Q_PROCESSOR_S390_X)
	return QLatin1String("s390x");
#elif defined(Q_PROCESSOR_S390)
	return QLatin1String("s390");
#elif defined(Q_PROCESSOR_SH)
	return QLatin1String("sh");
#elif defined(Q_PROCESSORS_SPARC_64)
	return QLatin1String("sparc64");
#elif defined(Q_PROCESSOR_SPARC_V9)
	return QLatin1String("sparcv9");
#elif defined(Q_PROCESSOR_SPARC)
	return QLatin1String("sparc");
#else
	return QLatin1String("unknown");
#endif
}

/**
 * @brief Gets the compiler that NEdit was built with.
 *
 * @return A QString containing the compiler name and version.
 *
 * @note adapted from QtCreator src/plugins/coreplugin/icore.cpp: compilerString()
 */
QString BuildCompiler() {
#if defined(Q_CC_CLANG) // must be before GNU, because clang claims to be GNU too
	QString isAppleString;
#if defined(__apple_build_version__) // Apple clang has other version numbers
	isAppleString = QStringLiteral(" (Apple)");
#endif
	return QStringLiteral("Clang ") +
		   QString::number(__clang_major__) +
		   QLatin1Char('.') +
		   QString::number(__clang_minor__) +
		   isAppleString;
#elif defined(Q_CC_GNU)
	return QStringLiteral("GCC ") + QStringLiteral(__VERSION__);
#elif defined(Q_CC_MSVC)
	// clang-format off
	if constexpr (_MSC_VER > 1999) return QLatin1String("MSVC <unknown>");
	else if constexpr (_MSC_VER >= 1930) return QLatin1String("MSVC 2022");
	else if constexpr (_MSC_VER >= 1920) return QLatin1String("MSVC 2019");
	else if constexpr (_MSC_VER >= 1910) return QLatin1String("MSVC 2017");
	else if constexpr (_MSC_VER >= 1900) return QLatin1String("MSVC 2015");
	// clang-format on
#else
	return QStringLiteral("<unknown compiler>");
#endif
}
