
#ifndef UTILS_H_
#define UTILS_H_

template <int (&F)(int) noexcept, class Ch>
constexpr int safe_ctype(Ch c) {
	return F(static_cast<unsigned char>(c));
}

#endif
