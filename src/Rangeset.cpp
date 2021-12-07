
#include "Rangeset.h"
#include "TextBuffer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

RangesetUpdateFn rangesetInsDelMaintain;
RangesetUpdateFn rangesetInclMaintain;
RangesetUpdateFn rangesetDelInsMaintain;
RangesetUpdateFn rangesetExclMaintain;
RangesetUpdateFn rangesetBreakMaintain;

auto DEFAULT_UPDATE_FN_NAME = QLatin1String("maintain");

struct {
	QLatin1String name;
	RangesetUpdateFn *update_fn;
} RangesetUpdateMap[] = {
	{DEFAULT_UPDATE_FN_NAME, rangesetInsDelMaintain},
	{QLatin1String("ins_del"), rangesetInsDelMaintain},
	{QLatin1String("include"), rangesetInclMaintain},
	{QLatin1String("del_ins"), rangesetDelInsMaintain},
	{QLatin1String("exclude"), rangesetExclMaintain},
	{QLatin1String("break"), rangesetBreakMaintain}};

/*
** Refresh the given range on the screen. If the range indicated is null, we
** refresh the screen for the whole file.
*/
void RangesetRefreshRange(TextBuffer *buffer, TextCursor start, TextCursor end) {
	if (buffer) {
		buffer->BufCheckDisplay(start, end);
	}
}

// --------------------------------------------------------------------------

bool is_start(int64_t i) {
	return !(i & 1);
}

bool is_end(int64_t i) {
	return (i & 1);
}

void rangesetRefreshAllRanges(TextBuffer *buffer, Rangeset *rangeset) {

	for (TextRange &range : rangeset->ranges_) {
		RangesetRefreshRange(buffer, range.start, range.end);
	}
}

// --------------------------------------------------------------------------

/*
** Find the index of the first integer in table greater than or equal to pos.
** Fails with len (the total number of entries). The start index base can be
** chosen appropriately.
*/

// NOTE(eteran): this just an implementation of std::lower_bound.
//               though it is probably better to replace the fundamental
//               data-structures instead of replacing a few random
//               functions
template <class T>
int64_t at_or_before(const T *table, int64_t base, int64_t len, T val) {

	int64_t mid = 0;

	if (base >= len) {
		return len; /* not sure what this means! */
	}

	int64_t lo = base;    /* first valid index */
	int64_t hi = len - 1; /* last valid index */

	while (lo <= hi) {
		mid = (lo + hi) / 2;

		if (val == table[mid]) {
			return mid;
		}

		if (val < table[mid]) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}

	/* if we get here, we didn't find val itself */
	if (val > table[mid]) {
		mid++;
	}

	return mid;
}

template <class T>
int64_t weighted_at_or_before(const T *table, int64_t base, int64_t len, T val) {

	int64_t mid = 0;

	if (base >= len) {
		return len; /* not sure what this means! */
	}

	int64_t lo = base;    /* first valid index */
	int64_t hi = len - 1; /* last valid index */

	TextCursor min = table[lo]; /* establish initial min/max */
	TextCursor max = table[hi];

	if (val <= min) { /* initial range checks */
		return lo;    /* needed to avoid out-of-range mid values */
	} else if (val > max) {
		return len;
	} else if (val == max) {
		return hi;
	}

	while (lo <= hi) {
		/* Beware of integer overflow when multiplying large numbers! */
		mid = lo + static_cast<int>((hi - lo) * static_cast<double>(val - min) / (max - min));
		/* we won't worry about min == max - values should be unique */

		if (val == table[mid]) {
			return mid;
		}

		if (val < table[mid]) {
			hi  = mid - 1;
			max = table[mid];
		} else { /* val > table[mid] */
			lo  = mid + 1;
			min = table[mid];
		}
	}

	/* if we get here, we didn't find val itself */
	if (val > table[mid]) {
		return mid + 1;
	}

	return mid;
}

// --------------------------------------------------------------------------

/*
** Find the index of the first entry in the range set's ranges table (viewed as
** an int array) whose value is equal to or greater than pos. As a side effect,
** update the last_index value of the range set. Return's the index value. This
** will be twice p->n_ranges if pos is beyond the end.
*/
int64_t rangesetWeightedAtOrBefore(Rangeset *rangeset, TextCursor pos) {

	int64_t i;
	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());

	int64_t n = rangeset->ranges_.size();
	if (n == 0) {
		return 0;
	}

	int64_t last = rangeset->last_index_;

	if (last >= n || last < 0) {
		last = 0;
	}

	n *= 2;
	last *= 2;

	if (pos >= rangeTable[last]) {                           // ranges[last_index].start
		i = weighted_at_or_before(rangeTable, last, n, pos); // search end only
	} else {
		i = weighted_at_or_before(rangeTable, 0, last, pos); // search front only
	}

	rangeset->last_index_ = i / 2;
	return i;
}

/*
** Adjusts values in tab[] by an amount delta, perhaps moving them meanwhile.
*/
template <class T>
int64_t rangesetShuffleToFrom(T *table, int64_t to, int64_t from, int64_t n, int64_t delta) {
	int64_t end;
	int64_t diff = from - to;

	if (n <= 0) {
		return 0;
	}

	if (delta != 0) {
		if (diff > 0) { /* shuffle entries down */
			for (end = to + n; to < end; to++) {
				table[to] = table[to + diff] + delta;
			}
		} else if (diff < 0) { /* shuffle entries up */
			for (end = to, to += n; --to >= end;) {
				table[to] = table[to + diff] + delta;
			}
		} else { /* diff == 0: just run through */
			for (end = n; end--;) {
				table[to++] += delta;
			}
		}
	} else {
		if (diff > 0) { /* shuffle entries down */
			for (end = to + n; to < end; to++) {
				table[to] = table[to + diff];
			}
		} else if (diff < 0) { /* shuffle entries up */
			for (end = to, to += n; --to >= end;) {
				table[to] = table[to + diff];
			}
		}
		/* else diff == 0: nothing to do */
	}

	return n;
}

/*
** Functions to adjust a rangeset to include new text or remove old.
** *** NOTE: No redisplay: that's outside the responsability of these routines.
*/

/* "Insert/Delete": if the start point is in or at the end of a range
** (start < pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
*/
Rangeset *rangesetInsDelMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());
	int64_t n       = 2 * rangeset->ranges_.size();

	int64_t i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; /* all beyond the end */
	}

	TextCursor end_del = pos + del;
	int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { /* skip j to first ind beyond changes */
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. */
	if (j > i) {
		rangeTable[i] = pos + ins;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j)) {
		i++;
	}

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Inclusive": if the start point is in, at the start of, or at the end of a
** range (start <= pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
** (Almost identical to rangesetInsDelMaintain().)
*/
Rangeset *rangesetInclMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());
	int64_t n       = 2 * rangeset->ranges_.size();

	int64_t i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; /* all beyond the end */
	}

	/* if the insert occurs at the start of a range, the following lines will
	   extend the range, leaving the start of the range at pos. */

	if (is_start(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	TextCursor end_del = pos + del;
	int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { /* skip j to first ind beyond changes */
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. */
	if (j > i) {
		rangeTable[i] = pos + ins;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j)) {
		i++;
	}

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Delete/Insert": if the start point is in a range (start < pos &&
** pos <= end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges.
*/
Rangeset *rangesetDelInsMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());
	int64_t n       = 2 * rangeset->ranges_.size();

	int64_t i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; /* all beyond the end */
	}

	TextCursor end_del = pos + del;
	int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { /* skip j to first ind beyond changes */
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i) {
		rangeTable[i] = (is_end(j)) ? pos + ins : pos;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (is_start(i) != is_start(j)) {
		i++;
	}

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Exclusive": if the start point is in, but not at the end of, a range
** (start < pos && pos < end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges. (Almost identical to rangesetDelInsMaintain().)
*/
Rangeset *rangesetExclMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());
	int64_t n       = 2 * rangeset->ranges_.size();

	int64_t i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; /* all beyond the end */
	}

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (is_end(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	TextCursor end_del = pos + del;
	int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { /* skip j to first ind beyond changes */
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i) {
		rangeTable[i] = (is_end(j)) ? pos + ins : pos;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */
	if (is_start(i) != is_start(j)) {
		i++;
	}

	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Break": if the modification point pos is strictly inside a range, that range
** may be broken in two if the deletion point pos+del does not extend beyond the
** end. Inserted text is never included in the range.
*/
Rangeset *rangesetBreakMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	auto rangeTable = reinterpret_cast<TextCursor *>(rangeset->ranges_.data());

	int64_t n = 2 * rangeset->ranges_.size();

	int64_t i = rangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; /* all beyond the end */
	}

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (is_end(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	TextCursor end_del = pos + del;
	int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { /* skip j to first ind beyond changes */
		j++;
	}

	if (j > i) {
		rangeTable[i] = pos;
	}

	/* do we need to insert a gap? yes if pos is in a range and ins > 0 */

	/* The logic for the next statement: if i and j are both range ends, range
	   boundaries indicated by index values between i and j (if any) have been
	   "skipped". This means that rangeTable[i-1],rangeTable[j] is the current range. We will
	   be inserting in that range, splitting it. */

	bool need_gap = (is_end(i) && is_end(j) && ins > 0);

	/* if we've got start-end or end-start, skip rangeTable[i] */
	if (is_start(i) != is_start(j)) { /* one is start, other is end */
		if (is_start(i)) {
			if (rangeTable[i] == pos) {
				rangeTable[i] = pos + ins; /* move the range start */
			}
		}
		i++; /* skip to next index */
	}

	/* values rangeTable[j] to rangeTable[n-1] must be adjusted by movement and placed in
	   position. */

	if (need_gap) {
		i += 2; /* make space for the break */
	}

	/* adjust other position values: shuffle them up or down if necessary */
	rangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	if (need_gap) { /* add the gap informations */
		rangeTable[i - 2] = pos;
		rangeTable[i - 1] = pos + ins;
	}

	n -= j - i;

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

// --------------------------------------------------------------------------

}

/*
** Return the name, if any.
*/
QString Rangeset::name() const {
	return name_;
}

/**
 * @brief Rangeset::RangesetSpan
 * @return
 */
ext::optional<TextRange> Rangeset::RangesetSpan() const {
	if (ranges_.empty()) {
		return {};
	}

	TextRange r;
	r.start = ranges_.front().start;
	r.end   = ranges_.back().end;
	return r;
}

/**
 * @brief Rangeset::RangesetFindRangeNo
 * @param index
 * @return
 */
ext::optional<TextRange> Rangeset::RangesetFindRangeNo(int index) const {

	const auto n = static_cast<size_t>(index);
	if (index < 0 || ranges_.size() <= n) {
		return {};
	}

	return ranges_[n];
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Note: ranges are indexed from zero.
*/
int64_t Rangeset::RangesetFindRangeOfPos(TextCursor pos, bool incl_end) const {

	if (ranges_.empty()) {
		return -1;
	}

	auto ranges       = reinterpret_cast<const TextCursor *>(ranges_.data()); /* { s1,e1, s2,e2, s3,e3,... } */
	const int64_t len = ranges_.size() * 2;
	const int64_t ind = at_or_before(ranges, 0, len, pos);

	if (ind == len) {
		return -1; /* beyond end */
	}

	if (is_end(ind)) {
		if (pos < ranges[ind] || (incl_end && pos == ranges[ind])) {
			return ind / 2; /* return the range index */
		}
	} else { /* ind even: references start marker */
		if (pos == ranges[ind]) {
			return ind / 2; /* return the range index */
		}
	}

	return -1; /* not in any range */
}

/*
** Get number of ranges in rangeset.
*/
int64_t Rangeset::size() const {
	return ranges_.size();
}

/*
** Invert the rangeset (replace it with its complement in the range 0-maxpos).
** Returns the new number of ranges. Never adds more than one range.
*/
int64_t Rangeset::RangesetInverse() {

	const TextCursor first = buffer_->BufStartOfBuffer();
	const TextCursor last  = buffer_->BufEndOfBuffer();

	if (ranges_.empty()) {
		ranges_.push_back({first, last});
	} else {

		// find out what we have
		const bool has_zero = (ranges_.front().start == first);
		const bool has_end  = (ranges_.back().end == last);

		std::vector<TextRange> newRanges;
		newRanges.reserve(ranges_.size() + 1);

		if (!has_zero) {
			// existing ranges don't extend to the begining, so add an element for it
			newRanges.push_back({first, ranges_.front().start});
		}

		// create an entry for all of the between current ranges
		for (auto curr = ranges_.begin(); curr != ranges_.end(); ++curr) {
			auto next = std::next(curr);
			if (next != ranges_.end()) {
				newRanges.push_back({curr->end, next->start});
			}
		}

		if (!has_end) {
			// existing ranges don't extend to the end, so add an element for it
			newRanges.push_back({ranges_.back().end, last});
		}

		ranges_ = std::move(newRanges);
	}

	RangesetRefreshRange(buffer_, first, last);
	return ranges_.size();
}

/*
** Assign a color name to a rangeset via the rangeset table.
*/
bool Rangeset::setColor(TextBuffer *buffer, const QString &color_name) {

	/* store new color name value */
	color_name_ = color_name.isEmpty() ? QString() : color_name; /* "" invalid */
	color_set_  = 0;

	rangesetRefreshAllRanges(buffer, this);
	return true;
}

/*
** Assign a name to a rangeset via the rangeset table.
*/
bool Rangeset::setName(const QString &name) {
	name_ = name.isEmpty() ? QString() : name;
	return true;
}

/*
** Change a range set's modification behaviour. Returns true (non-zero)
** if the update function name was found, else false.
*/
bool Rangeset::setMode(const QString &mode) {

	if (mode.isNull()) {
		return setMode(DEFAULT_UPDATE_FN_NAME);
	}

	for (auto &entry : RangesetUpdateMap) {
		if (entry.name == mode) {
			update_      = entry.update_fn;
			update_name_ = entry.name;
			return true;
		}
	}

	return false;
}

/*
** Find out whether the position pos is included in one of the ranges of
** rangeset. Returns the containing range's index if true, -1 otherwise.
** Essentially the same as the RangesetFindRangeOfPos() function, but uses the
** last_index member of the rangeset and weighted_at_or_before() for speedy
** lookup in refresh tasks. The rangeset is assumed to be valid, as is the
** position. We also don't allow checking of the endpoint.
** Returns the including range index, or -1 if not found.
*/
int64_t Rangeset::RangesetCheckRangeOfPos(TextCursor pos) {

	int64_t index;

	int64_t len = ranges_.size();
	if (ranges_.empty()) {
		return -1; /* no ranges */
	}

	auto ranges  = reinterpret_cast<TextCursor *>(ranges_.data()); /* { s1,e1, s2,e2, s3,e3,... } */
	int64_t last = last_index_;

	/* try to profit from the last lookup by using its index */
	if (last >= len || last < 0) {
		last        = (len > 0) ? len - 1 : 0; /* make sure last is in range */
		last_index_ = last;
	}

	len *= 2;
	last *= 2;

	if (pos >= ranges[last]) {        /* last even: this is a start */
		if (pos < ranges[last + 1]) { /* checking an end here */
			return last / 2;          /* no need to change rangeset->last_index */
		} else {
			last += 2; /* not in this range: move on */
		}

		if (last == len) {
			return -1; /* moved on too far */
		}

		/* find the entry in the upper portion of ranges */
		index = weighted_at_or_before(ranges, last, len, pos); /* search end only */
	} else if (last > 0) {
		index = weighted_at_or_before(ranges, 0, last, pos); /* search front only */
	} else {
		index = 0;
	}

	last_index_ = index / 2;

	if (index == len) {
		return -1; /* beyond end */
	}

	if (is_end(index)) {
		if (pos < ranges[index]) {
			return index / 2; /* return the range index */
		}
	} else { /* index even: references start marker */
		if (pos == ranges[index]) {
			return index / 2; /* return the range index */
		}
	}

	return -1; /* not in any range */
}

/*
** Merge the ranges in rangeset other into this rangeset.
*/
int64_t Rangeset::RangesetAdd(const Rangeset &other) {

	if (other.ranges_.empty()) {
		// no ranges in plusSet - nothing to do
		return ranges_.size();
	}

	if (ranges_.empty()) {
		// no ranges in destination: just copy the ranges from the other set
		ranges_ = other.ranges_;

		for (TextRange &range : ranges_) {
			RangesetRefreshRange(buffer_, range.start, range.end);
		}

		return ranges_.size();
	}

	auto origRanges    = ranges_.cbegin();
	size_t nOrigRanges = ranges_.size();

	auto plusRanges    = other.ranges_.cbegin();
	size_t nPlusRanges = other.ranges_.size();

	std::vector<TextRange> newRanges;
	newRanges.reserve(nOrigRanges + nPlusRanges);

	/* in the following we merrily swap the pointers/counters of the two input
	   ranges (from origSet and plusSet) - don't worry, they're both considered
	   read-only - building the merged set in newRanges */

	bool isOld = true; /* true if origRanges points to a range in oldRanges[] */

	while (nOrigRanges > 0 || nPlusRanges > 0) {

		/* make the range with the lowest start value the origRanges range */
		if (nOrigRanges == 0 || (nPlusRanges > 0 && origRanges->start > plusRanges->start)) {
			std::swap(origRanges, plusRanges);
			std::swap(nOrigRanges, nPlusRanges);
			isOld = !isOld;
		}

		auto newRange = newRanges.insert(newRanges.end(), *origRanges++);

		--nOrigRanges;

		if (!isOld) {
			RangesetRefreshRange(buffer_, newRange->start, newRange->end);
		}

		/* now we must cycle over plusRanges, merging in the overlapped ranges */
		while (nPlusRanges > 0 && newRange->end >= plusRanges->start) {
			do {
				if (newRange->end < plusRanges->end) {
					if (isOld) {
						RangesetRefreshRange(buffer_, newRange->end, plusRanges->end);
					}
					newRange->end = plusRanges->end;
				}

				++plusRanges;
				--nPlusRanges;
			} while (nPlusRanges > 0 && newRange->end >= plusRanges->start);

			/* by now, newRangeIt->end may have extended to overlap more ranges
			 * in origRanges, so swap and start again */
			std::swap(origRanges, plusRanges);
			std::swap(nOrigRanges, nPlusRanges);
			isOld = !isOld;
		}
	}

	/* finally, forget the old rangeset values, and reallocate the new ones */
	ranges_ = std::move(newRanges);
	return ranges_.size();
}

/*
** Subtract the ranges of other from this rangeset.
*/
int64_t Rangeset::RangesetRemove(const Rangeset &other) {

	if (ranges_.empty() || other.ranges_.empty()) {
		// no ranges in origSet or minusSet - nothing to do
		return 0;
	}

	auto origRanges    = ranges_.begin();
	size_t nOrigRanges = ranges_.size();

	auto minusRanges    = other.ranges_.cbegin();
	size_t nMinusRanges = other.ranges_.size();

	// we must provide more space: each range in minusSet might split a range in origSet
	std::vector<TextRange> newRanges;
	newRanges.reserve(ranges_.size() + other.ranges_.size());

	auto newRangeOut = std::back_inserter(newRanges);

	/* consider each range in origRanges - we do not change any of
	 * minusRanges's data, but we may change origRanges's - it will be
	 * discarded at the end */
	while (nOrigRanges > 0) {
		do {
			// skip all minusRanges ranges strictly in front of *origRanges
			while (nMinusRanges > 0 && minusRanges->end <= origRanges->start) {

				// *minusRanges in front of *origRanges: move onto next *minusRanges
				++minusRanges;
				--nMinusRanges;
			}

			if (nMinusRanges > 0) {
				// keep all origRanges ranges strictly in front of *minusRanges
				while (nOrigRanges > 0 && origRanges->end <= minusRanges->start) {
					*newRangeOut++ = *origRanges++; /* *minusRanges beyond *origRanges: save *origRanges in *newRangeOut */
					--nOrigRanges;
				}
			} else {
				// no more minusRanges ranges to remove - save the rest of origRanges
				while (nOrigRanges > 0) {
					*newRangeOut++ = *origRanges++;
					--nOrigRanges;
				}
			}
		} while (nMinusRanges > 0 && minusRanges->end <= origRanges->start); /* any more non-overlaps */

		// when we get here either we're done, or we have overlap
		if (nOrigRanges > 0) {
			if (minusRanges->start <= origRanges->start) {
				// origRanges->start inside *minusRanges
				if (minusRanges->end < origRanges->end) {
					RangesetRefreshRange(buffer_, origRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; // cut off front of original *origRanges
					minusRanges++;                        // dealt with this *minusRanges: move on
					nMinusRanges--;
				} else {
					// all *origRanges inside *minusRanges
					RangesetRefreshRange(buffer_, origRanges->start, origRanges->end);
					origRanges++; /* all of *origRanges can be skipped */
					nOrigRanges--;
				}
			} else {
				/* minusRanges->start inside *origRanges: save front, adjust or skip rest */
				*newRangeOut++ = {origRanges->start, minusRanges->start}; /* save front of *origRanges in *newRanges */

				if (minusRanges->end < origRanges->end) {
					/* all *minusRanges inside *origRanges */
					RangesetRefreshRange(buffer_, minusRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; /* cut front of *origRanges upto end *minusRanges */
					minusRanges++;                        /* dealt with this *minusRanges: move on */
					nMinusRanges--;
				} else {
					/* minusRanges->end beyond *origRanges */
					RangesetRefreshRange(buffer_, minusRanges->start, origRanges->end);
					origRanges++; /* skip rest of *origRanges */
					nOrigRanges--;
				}
			}
		}
	}

	// finally, forget the old rangeset values, and reallocate the new ones
	ranges_ = std::move(newRanges);
	return ranges_.size();
}

/*
** Add the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/
int64_t Rangeset::RangesetAdd(TextRange r) {

	if (r.start > r.end) {
		// quietly sort the positions
		std::swap(r.start, r.end);
	} else if (r.start == r.end) {
		// no-op - empty range == no range
		return ranges_.size();
	}

	// if it's the first range, just insert it
	if (ranges_.empty()) {
		ranges_.push_back(r);
		RangesetRefreshRange(buffer_, r.start, r.end);
		return ranges_.size();
	}

	auto next = std::lower_bound(ranges_.begin(), ranges_.end(), r);
	if (next != ranges_.end()) {
		auto prev = (next != ranges_.begin()) ? std::prev(next) : next;

		if (r.start <= next->end && next->start <= r.end) {
			// new range overlaps with one to its right
			next->start = r.start;

			if (prev->start <= next->end && next->start <= prev->end) {
				// adjusted range now overlaps with one to its left
				prev->end = next->end;
				ranges_.erase(next);
			}
		} else if (prev->start <= r.end && r.start <= prev->end) {
			// new range overlaps with the one to its left
			prev->end = r.end;
		} else {
			// no overlap, just insert it in the right place
			ranges_.insert(next, r);
		}
	} else {
		auto prev = std::prev(ranges_.end());

		if (prev->start <= r.end && r.start <= prev->end) {
			// new range overlaps with the one to its left (last element)
			prev->end = r.end;
		} else {
			// no overlap, just insert it at the end
			ranges_.push_back(r);
		}
	}

	RangesetRefreshRange(buffer_, r.start, r.end);
	return ranges_.size();
}

/*
** Remove the range indicated by the positions start and end. Returns the
** new number of ranges in the set.
*/
int64_t Rangeset::RangesetRemove(TextRange r) {

	if (r.start > r.end) {
		// quietly sort the positions
		std::swap(r.start, r.end);
	} else if (r.start == r.end) {
		// no-op - empty range == no range
		return ranges_.size();
	}

	if (ranges_.empty()) {
		return ranges_.size();
	}

	auto next = std::lower_bound(ranges_.begin(), ranges_.end(), r);
	if (next != ranges_.end()) {
		auto prev = (next != ranges_.begin()) ? std::prev(next) : next;

		if (*next == r) {
			// exact match
			ranges_.erase(next);
		} else if (r.start <= next->end && next->start <= r.end) {
			// range overlaps with one to its right
			next->start = r.end;
		} else if (prev->start <= r.end && r.start <= prev->end) {
			// range overlaps with the one to its left
			prev->end = r.start;
		}
	} else {
		auto prev = std::prev(ranges_.end());

		if (prev->start <= r.end && r.start <= prev->end) {
			// range overlaps with the one to its left (last element)
			prev->end = r.start;
		}
	}

	RangesetRefreshRange(buffer_, r.start, r.end);
	return ranges_.size();
}

/**
 * @brief Rangeset::operator +=
 * @param rhs
 * @return
 */
Rangeset &Rangeset::operator+=(const Rangeset &rhs) {
	RangesetAdd(rhs);
	return *this;
}

/**
 * @brief Rangeset::operator -=
 * @param rhs
 * @return
 */
Rangeset &Rangeset::operator-=(const Rangeset &rhs) {
	RangesetRemove(rhs);
	return *this;
}

/**
 * @brief Rangeset::operator ~
 * @return
 */
Rangeset Rangeset::operator~() const {
	Rangeset ret(*this);
	ret.RangesetInverse();
	return ret;
}

/**
 * @brief Rangeset::RangesetGetInfo
 * @return
 */
RangesetInfo Rangeset::RangesetGetInfo() const {
	RangesetInfo info;
	info.defined = true;
	info.label   = static_cast<int>(label_);
	info.count   = ranges_.size();
	info.color   = color_name_;
	info.name    = name_;
	info.mode    = update_name_;
	return info;
}

/**
 * @brief Rangeset::Rangeset
 * @param label
 */
Rangeset::Rangeset(TextBuffer *buffer, uint8_t label)
	: buffer_(buffer), label_(label) {
	setMode(DEFAULT_UPDATE_FN_NAME);
}

/**
 * @brief Rangeset::~Rangeset
 */
Rangeset::~Rangeset() {
	for (const TextRange &range : ranges_) {
		RangesetRefreshRange(buffer_, range.start, range.end);
	}
}
