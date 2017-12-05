
#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <algorithm>

template <class Cont>
void moveItem(Cont &cont, int from, int to) {
	
    Q_ASSERT(from >= 0 && from < cont.size());
    Q_ASSERT(to >= 0 && to < cont.size());
	
	if (from == to) // don't detach when no-op
    	return;
	
	auto b = cont.begin();
	if (from < to)
    	std::rotate(b + from, b + from + 1, b + to + 1);
	else
    	std::rotate(b + to, b + from, b + from + 1);
}

#endif
