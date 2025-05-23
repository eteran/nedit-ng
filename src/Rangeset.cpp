
#include "Rangeset.h"
#include "TextBuffer.h"
#include "Util/algorithm.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

RangesetUpdateFn RangesetInsDelMaintain;
RangesetUpdateFn RangesetInclMaintain;
RangesetUpdateFn RangesetDelInsMaintain;
RangesetUpdateFn RangesetExclMaintain;
RangesetUpdateFn RangesetBreakMaintain;

auto DefaultUpdateFuncName = QLatin1String("maintain");

struct {
	QLatin1String name;
	RangesetUpdateFn *updateFunc;
} RangesetUpdateMap[] = {
	{DefaultUpdateFuncName, RangesetInsDelMaintain},
	{QLatin1String("ins_del"), RangesetInsDelMaintain},
	{QLatin1String("include"), RangesetInclMaintain},
	{QLatin1String("del_ins"), RangesetDelInsMaintain},
	{QLatin1String("exclude"), RangesetExclMaintain},
	{QLatin1String("break"), RangesetBreakMaintain}};

/**
 * @brief Refresh the given range on the screen. If the range indicated is invalid, we
 * refresh the screen for the whole file.
 *
 * @param buffer The text buffer to refresh.
 * @param start The start position of the range to refresh.
 * @param end The end position of the range to refresh.
 */
void RefreshRange(TextBuffer *buffer, TextCursor start, TextCursor end) {
	if (buffer) {
		buffer->BufCheckDisplay(start, end);
	}
}

/**
 * @brief Check if the given index is a start of a range.
 *
 * @param i The index to check.
 * @return `true` if the index is a start of a range, `false` otherwise.
 */
bool IsStart(int64_t i) {
	return !(i & 1);
}

/**
 * @brief Check if the given index is an end of a range.
 *
 * @param i The index to check.
 * @return `true` if the index is an end of a range, `false` otherwise.
 */
bool IsEnd(int64_t i) {
	return (i & 1);
}

/**
 * @brief Refresh all ranges in the rangeset.
 *
 * @param buffer The text buffer to refresh.
 * @param rangeset The rangeset containing the ranges to refresh.
 */
void RefreshAllRanges(TextBuffer *buffer, Rangeset *rangeset) {
	for (const TextRange &range : rangeset->ranges_) {
		RefreshRange(buffer, range.start, range.end);
	}
}

// --------------------------------------------------------------------------

/**
 * @brief Searches for the first element in the range which is not ordered before val.
 *
 * @param table the sorted array to search.
 * @param base the base index to start searching from.
 * @param len the length of the array.
 * @param val the value to search for in the array.
 * @return the index of the first element in the array which is not ordered before val.
 *         If val is greater than all elements, returns len.
 *
 * @note This function is effectively the same as `std::lower_bound`.
 */
template <class T>
int64_t AtOrBefore(const T *table, int64_t base, int64_t len, T val) {

	int64_t mid = 0;

	if (base >= len) {
		return len; // not sure what this means!
	}

	int64_t lo = base;    // first valid index
	int64_t hi = len - 1; // last valid index

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

	// if we get here, we didn't find val itself
	if (val > table[mid]) {
		mid++;
	}

	return mid;
}

/**
 * @brief Searches for the first element in the range which is not ordered before val.
 *
 * @param table the sorted array to search.
 * @param base the base index to start searching from.
 * @param len the length of the array.
 * @param val the value to search for in the array.
 * @return the index of the first element in the array which is not ordered before val.
 *         If val is greater than all elements, returns len.
 */
template <class T>
int64_t WeightedAtOrBefore(const T *table, int64_t base, int64_t len, T val) {

	int64_t mid = 0;

	if (base >= len) {
		return len; // not sure what this means!
	}

	int64_t lo = base;    // first valid index
	int64_t hi = len - 1; // last valid index

	TextCursor min = table[lo]; // establish initial min/max
	TextCursor max = table[hi];

	if (val <= min) { // initial range checks
		return lo;    // needed to avoid out-of-range mid values
	}

	if (val > max) {
		return len;
	}

	if (val == max) {
		return hi;
	}

	while (lo <= hi) {
		// Beware of integer overflow when multiplying large numbers!
		mid = lo + static_cast<int64_t>((hi - lo) * static_cast<long double>(val - min) / (max - min));
		// we won't worry about min == max - values should be unique

		if (val == table[mid]) {
			return mid;
		}

		if (val < table[mid]) {
			hi  = mid - 1;
			max = table[mid];
		} else { // val > table[mid]
			lo  = mid + 1;
			min = table[mid];
		}
	}

	// if we get here, we didn't find val itself
	if (val > table[mid]) {
		return mid + 1;
	}

	return mid;
}

// --------------------------------------------------------------------------

TextCursor *FlattenRanges(std::vector<TextRange> &ranges) {
	// NOTE(eteran): ranges_ contains TextRange objects which are POD structs
	// with two TextCursors in them. So by casting to TextCursor *, we can
	// iterate through the individual elements of the pairs.
	// so { {s1, e1}, {s2, e2}, {s3, e3}, ... }
	// becomes
	// { s1, e1, s2, e2, s3, e3, ... }
	return reinterpret_cast<TextCursor *>(ranges.data());
}

/*
** Find the index of the first entry in the range set's ranges table (viewed as
** an int array) whose value is equal to or greater than pos. As a side effect,
** update the last_index value of the range set. Return's the index value. This
** will be twice p->n_ranges if pos is beyond the end.
*/
int64_t RangesetWeightedAtOrBefore(Rangeset *rangeset, TextCursor pos) {

	int64_t i;

	const TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);

	auto n = static_cast<int64_t>(rangeset->ranges_.size());
	if (n == 0) {
		return 0;
	}

	int64_t last = rangeset->last_index_;

	if (last >= n || last < 0) {
		last = 0;
	}

	n *= 2;
	last *= 2;

	if (pos >= rangeTable[last]) {                        // ranges[last_index].start
		i = WeightedAtOrBefore(rangeTable, last, n, pos); // search end only
	} else {
		i = WeightedAtOrBefore(rangeTable, 0, last, pos); // search front only
	}

	rangeset->last_index_ = i / 2;
	return i;
}

/*
** Adjusts values in table[] by an amount delta, perhaps moving them meanwhile.
*/
template <class T>
int64_t RangesetShuffleToFrom(T *table, int64_t to, int64_t from, int64_t n, int64_t delta) {
	int64_t end;
	const int64_t diff = from - to;

	if (n <= 0) {
		return 0;
	}

	if (delta != 0) {
		if (diff > 0) { // shuffle entries down
			for (end = to + n; to < end; to++) {
				table[to] = table[to + diff] + delta;
			}
		} else if (diff < 0) { // shuffle entries up
			for (end = to, to += n; --to >= end;) {
				table[to] = table[to + diff] + delta;
			}
		} else { // diff == 0: just run through
			for (end = n; end--;) {
				table[to++] += delta;
			}
		}
	} else {
		if (diff > 0) { // shuffle entries down
			for (end = to + n; to < end; to++) {
				table[to] = table[to + diff];
			}
		} else if (diff < 0) { // shuffle entries up
			for (end = to, to += n; --to >= end;) {
				table[to] = table[to + diff];
			}
		}
		// else diff == 0: nothing to do
	}

	return n;
}

/*
** Functions to adjust a rangeset to include new text or remove old.
** *** NOTE: No redisplay: that's outside the responsibility of these routines.
*/

/* "Insert/Delete": if the start point is in or at the end of a range
** (start < pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
*/
Rangeset *RangesetInsDelMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);
	auto n                 = 2 * static_cast<int64_t>(rangeset->ranges_.size());

	int64_t i = RangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; // all beyond the end
	}

	const TextCursor end_del = pos + del;
	const int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
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

	if (IsStart(i) != IsStart(j)) {
		i++;
	}

	RangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Inclusive": if the start point is in, at the start of, or at the end of a
** range (start <= pos && pos <= end), any text inserted will extend that range.
** Insertions appear to occur before deletions. This will never add new ranges.
** (Almost identical to RangesetInsDelMaintain().)
*/
Rangeset *RangesetInclMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);
	auto n                 = 2 * static_cast<int64_t>(rangeset->ranges_.size());

	int64_t i = RangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; // all beyond the end
	}

	/* if the insert occurs at the start of a range, the following lines will
	   extend the range, leaving the start of the range at pos. */

	if (IsStart(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	const TextCursor end_del = pos + del;
	const int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
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

	if (IsStart(i) != IsStart(j)) {
		i++;
	}

	RangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

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
Rangeset *RangesetDelInsMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);
	auto n                 = 2 * static_cast<int64_t>(rangeset->ranges_.size());

	int64_t i = RangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; // all beyond the end
	}

	const TextCursor end_del = pos + del;
	const int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i) {
		rangeTable[i] = (IsEnd(j)) ? pos + ins : pos;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */

	if (IsStart(i) != IsStart(j)) {
		i++;
	}

	RangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Exclusive": if the start point is in, but not at the end of, a range
** (start < pos && pos < end), and the end of deletion is also in a range
** (start <= pos + del && pos + del < end) any text inserted will extend that
** range. Deletions appear to occur before insertions. This will never add new
** ranges. (Almost identical to RangesetDelInsMaintain().)
*/
Rangeset *RangesetExclMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);
	int64_t n              = 2 * static_cast<int64_t>(rangeset->ranges_.size());

	int64_t i = RangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; // all beyond the end
	}

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (IsEnd(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	const TextCursor end_del = pos + del;
	const int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
		j++;
	}

	/* if j moved forward, we have deleted over rangeTable[i] - reduce it accordingly,
	   accounting for inserts. (Note: if rangeTable[j] is an end position, inserted
	   text will belong to the range that rangeTable[j] closes; otherwise inserted
	   text does not belong to a range.) */
	if (j > i) {
		rangeTable[i] = (IsEnd(j)) ? pos + ins : pos;
	}

	/* If i and j both index starts or ends, just copy all the rangeTable[] values down
	   by j - i spaces, adjusting on the way. Otherwise, move beyond rangeTable[i]
	   before doing this. */
	if (IsStart(i) != IsStart(j)) {
		i++;
	}

	RangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	n -= (j - i);

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

/* "Break": if the modification point pos is strictly inside a range, that range
** may be broken in two if the deletion point pos+del does not extend beyond the
** end. Inserted text is never included in the range.
*/
Rangeset *RangesetBreakMaintain(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del) {

	TextCursor *rangeTable = FlattenRanges(rangeset->ranges_);

	auto n = 2 * static_cast<int64_t>(rangeset->ranges_.size());

	int64_t i = RangesetWeightedAtOrBefore(rangeset, pos);

	if (i == n) {
		return rangeset; // all beyond the end
	}

	/* if the insert occurs at the end of a range, the following lines will
	   skip the range, leaving the end of the range at pos. */

	if (IsEnd(i) && rangeTable[i] == pos && ins > 0) {
		i++;
	}

	const TextCursor end_del = pos + del;
	const int64_t movement   = ins - del;

	/* the idea now is to determine the first range not concerned with the
	   movement: its index will be j. For indices j to n-1, we will adjust
	   position by movement only. (They may need shuffling up or down, depending
	   on whether ranges have been deleted or created by the change.) */
	int64_t j = i;
	while (j < n && rangeTable[j] <= end_del) { // skip j to first ind beyond changes
		j++;
	}

	if (j > i) {
		rangeTable[i] = pos;
	}

	// do we need to insert a gap? yes if pos is in a range and ins > 0

	/* The logic for the next statement: if i and j are both range ends, range
	   boundaries indicated by index values between i and j (if any) have been
	   "skipped". This means that rangeTable[i-1],rangeTable[j] is the current range. We will
	   be inserting in that range, splitting it. */

	const bool need_gap = (IsEnd(i) && IsEnd(j) && ins > 0);

	// if we've got start-end or end-start, skip rangeTable[i]
	if (IsStart(i) != IsStart(j)) { // one is start, other is end
		if (IsStart(i)) {
			if (rangeTable[i] == pos) {
				rangeTable[i] = pos + ins; // move the range start
			}
		}
		i++; // skip to next index
	}

	/* values rangeTable[j] to rangeTable[n-1] must be adjusted by movement and placed in
	   position. */

	if (need_gap) {
		i += 2; // make space for the break
	}

	// adjust other position values: shuffle them up or down if necessary
	RangesetShuffleToFrom(rangeTable, i, j, n - j, movement);

	if (need_gap) { // add the gap informations
		rangeTable[i - 2] = pos;
		rangeTable[i - 1] = pos + ins;
	}

	n -= j - i;

	rangeset->ranges_.resize(n / 2);
	return rangeset;
}

// --------------------------------------------------------------------------

}

/**
 * @brief Get the name of the rangeset.
 *
 * @return The name of the rangeset, or a QString() if no name is set.
 */
QString Rangeset::name() const {
	return name_;
}

/**
 * @brief Get the span of the rangeset, which is the range from the start of the first
 * range to the end of the last range.
 *
 * @return A TextRange containing the span of the rangeset if it has ranges,
 * or an empty optional if the rangeset is empty.
 */
std::optional<TextRange> Rangeset::RangesetSpan() const {
	if (ranges_.empty()) {
		return {};
	}

	TextRange r;
	r.start = ranges_.front().start;
	r.end   = ranges_.back().end;
	return r;
}

/**
 * @brief Find the range at the given index.
 *
 * @param index The index of the range to find.
 * @return The TextRange containing the range if found, or an empty optional
 * if the index is out of bounds.
 */
std::optional<TextRange> Rangeset::RangesetFindRangeNo(int index) const {

	const auto n = static_cast<size_t>(index);
	if (index < 0 || ranges_.size() <= n) {
		return {};
	}

	return ranges_[n];
}

/**
 * @brief Find the range that contains the position `pos`.
 *
 * @param pos The position to check.
 * @param incl_end If `true`, the end of the range is included in the check.
 * @return The containing range's index if found, -1 otherwise.
 */
int64_t Rangeset::RangesetFindRangeOfPos(TextCursor pos, bool incl_end) const {

	if (ranges_.empty()) {
		return -1;
	}

	auto ranges       = reinterpret_cast<const TextCursor *>(ranges_.data()); // { s1,e1, s2,e2, s3,e3,... }
	const auto len    = ssize(ranges_) * 2;
	const int64_t ind = AtOrBefore(ranges, 0, len, pos);

	if (ind == len) {
		return -1; // beyond end
	}

	if (IsEnd(ind)) {
		if (pos < ranges[ind] || (incl_end && pos == ranges[ind])) {
			return ind / 2; // return the range index
		}
	} else { // ind even: references start marker
		if (pos == ranges[ind]) {
			return ind / 2; // return the range index
		}
	}

	return -1; // not in any range
}

/**
 * @brief Get the number of ranges in the rangeset.
 *
 * @return The number of ranges in the rangeset.
 */
int64_t Rangeset::size() const {
	return ssize(ranges_);
}

/**
 * @brief Invert the rangeset (replace it with its complement in the range 0-maxpos).
 * Never adds more than one range.
 *
 * @return The new number of ranges.
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
			// existing ranges don't extend to the beginning, so add an element for it
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

	RefreshRange(buffer_, first, last);
	return ssize(ranges_);
}

/**
 * @brief Assign a color name to a rangeset via the rangeset table.
 *
 * @param buffer The text buffer to refresh.
 * @param color_name The name of the color to set for the rangeset. If `color_name` is
 * empty, the color is cleared.
 * @return `true` if the color was set, else `false`.
 */
bool Rangeset::setColor(TextBuffer *buffer, const QString &color_name) {

	// store new color name value
	color_name_ = color_name.isEmpty() ? QString() : color_name; // "" invalid
	color_set_  = 0;

	RefreshAllRanges(buffer, this);
	return true;
}

/**
 * @brief Change a range set's name.
 *
 * @param name The name to set for the rangeset. If `name` is empty, the name is
 * cleared.
 * @return `true` if the name was set, else `false`.
 */
bool Rangeset::setName(const QString &name) {
	name_ = name.isEmpty() ? QString() : name;
	return true;
}

/**
 * @brief Change a range set's modification behaviour.
 *
 * @param mode The mode to set for the rangeset. If `mode` is null, the default
 * mode is set.
 * @return `true` if the update function name was found, else `false`.
 */
bool Rangeset::setMode(const QString &mode) {

	if (mode.isNull()) {
		return setMode(DefaultUpdateFuncName);
	}

	for (const auto &entry : RangesetUpdateMap) {
		if (entry.name == mode) {
			update_      = entry.updateFunc;
			update_name_ = entry.name;
			return true;
		}
	}

	return false;
}

/**
 * @brief Find out whether the position `pos` is included in one of the ranges of
 * rangeset.
 * Essentially the same as the `RangesetFindRangeOfPos()` function, but uses the
 * last_index member of the rangeset and WeightedAtOrBefore() for speedy
 * lookup in refresh tasks. The rangeset is assumed to be valid, as is the
 * position. We also don't allow checking of the endpoint.
 *
 * @param pos The position to check.
 * @return The containing range's index if `true`, -1 otherwise.
 */
int64_t Rangeset::RangesetCheckRangeOfPos(TextCursor pos) {

	int64_t index;

	auto len = ssize(ranges_);
	if (ranges_.empty()) {
		return -1; // no ranges
	}

	TextCursor *ranges = FlattenRanges(ranges_); // { s1,e1, s2,e2, s3,e3,... }
	int64_t last       = last_index_;

	// try to profit from the last lookup by using its index
	if (last >= len || last < 0) {
		last        = (len > 0) ? len - 1 : 0; // make sure last is in range
		last_index_ = last;
	}

	len *= 2;
	last *= 2;

	if (pos >= ranges[last]) {        // last even: this is a start
		if (pos < ranges[last + 1]) { // checking an end here
			return last / 2;          // no need to change rangeset->last_index
		}

		last += 2; // not in this range: move on

		if (last == len) {
			return -1; // moved on too far
		}

		// find the entry in the upper portion of ranges
		index = WeightedAtOrBefore(ranges, last, len, pos); // search end only
	} else if (last > 0) {
		index = WeightedAtOrBefore(ranges, 0, last, pos); // search front only
	} else {
		index = 0;
	}

	last_index_ = index / 2;

	if (index == len) {
		return -1; // beyond end
	}

	if (IsEnd(index)) {
		if (pos < ranges[index]) {
			return index / 2; // return the range index
		}
	} else { // index even: references start marker
		if (pos == ranges[index]) {
			return index / 2; // return the range index
		}
	}

	return -1; // not in any range
}

/**
 * @brief Merges the ranges in the specified Rangeset into this Rangeset.
 *
 * @param other The Rangeset containing ranges to be added to this Rangeset.
 * @return The new number of ranges in this Rangeset after addition.
 */
int64_t Rangeset::RangesetAdd(const Rangeset &other) {

	if (other.ranges_.empty()) {
		// no ranges in plusSet - nothing to do
		return ssize(ranges_);
	}

	if (ranges_.empty()) {
		// no ranges in destination: just copy the ranges from the other set
		ranges_ = other.ranges_;

		for (const TextRange &range : ranges_) {
			RefreshRange(buffer_, range.start, range.end);
		}

		return ssize(ranges_);
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

	bool isOld = true; // `true` if origRanges points to a range in oldRanges[]

	while (nOrigRanges > 0 || nPlusRanges > 0) {

		// make the range with the lowest start value the origRanges range
		if (nOrigRanges == 0 || (nPlusRanges > 0 && origRanges->start > plusRanges->start)) {
			std::swap(origRanges, plusRanges);
			std::swap(nOrigRanges, nPlusRanges);
			isOld = !isOld;
		}

		auto newRange = newRanges.insert(newRanges.end(), *origRanges++);

		--nOrigRanges;

		if (!isOld) {
			RefreshRange(buffer_, newRange->start, newRange->end);
		}

		// now we must cycle over plusRanges, merging in the overlapped ranges
		while (nPlusRanges > 0 && newRange->end >= plusRanges->start) {
			do {
				if (newRange->end < plusRanges->end) {
					if (isOld) {
						RefreshRange(buffer_, newRange->end, plusRanges->end);
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

	// finally, forget the old rangeset values, and reallocate the new ones
	ranges_ = std::move(newRanges);
	return ssize(ranges_);
}

/**
 * @brief Removes the ranges in the specified Rangeset from this Rangeset.
 *
 * @param other The Rangeset containing ranges to be removed from this Rangeset.
 * @return The new number of ranges in this Rangeset after removal.
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
					*newRangeOut++ = *origRanges++; // *minusRanges beyond *origRanges: save *origRanges in *newRangeOut
					--nOrigRanges;
				}
			} else {
				// no more minusRanges ranges to remove - save the rest of origRanges
				while (nOrigRanges > 0) {
					*newRangeOut++ = *origRanges++;
					--nOrigRanges;
				}
			}
		} while (nMinusRanges > 0 && minusRanges->end <= origRanges->start); // any more non-overlaps

		// when we get here either we're done, or we have overlap
		if (nOrigRanges > 0) {
			if (minusRanges->start <= origRanges->start) {
				// origRanges->start inside *minusRanges
				if (minusRanges->end < origRanges->end) {
					RefreshRange(buffer_, origRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; // cut off front of original *origRanges
					minusRanges++;                        // dealt with this *minusRanges: move on
					nMinusRanges--;
				} else {
					// all *origRanges inside *minusRanges
					RefreshRange(buffer_, origRanges->start, origRanges->end);
					origRanges++; // all of *origRanges can be skipped
					nOrigRanges--;
				}
			} else {
				// minusRanges->start inside *origRanges: save front, adjust or skip rest
				*newRangeOut++ = {origRanges->start, minusRanges->start}; // save front of *origRanges in *newRanges

				if (minusRanges->end < origRanges->end) {
					// all *minusRanges inside *origRanges
					RefreshRange(buffer_, minusRanges->start, minusRanges->end);
					origRanges->start = minusRanges->end; // cut front of *origRanges upto end *minusRanges
					minusRanges++;                        // dealt with this *minusRanges: move on
					nMinusRanges--;
				} else {
					// minusRanges->end beyond *origRanges
					RefreshRange(buffer_, minusRanges->start, origRanges->end);
					origRanges++; // skip rest of *origRanges
					nOrigRanges--;
				}
			}
		}
	}

	// finally, forget the old rangeset values, and reallocate the new ones
	ranges_ = std::move(newRanges);
	return ssize(ranges_);
}

/**
 * @brief Adds a specified range to the rangeset.
 *
 * @param r The TextRange to add to the rangeset.
 * @return The new number of ranges in the set after addition.
 */
int64_t Rangeset::RangesetAdd(TextRange r) {

	if (r.start > r.end) {
		// quietly sort the positions
		std::swap(r.start, r.end);
	} else if (r.start == r.end) {
		// no-op - empty range == no range
		return ssize(ranges_);
	}

	// if it's the first range, just insert it
	if (ranges_.empty()) {
		ranges_.push_back(r);
		RefreshRange(buffer_, r.start, r.end);
		return ssize(ranges_);
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

	RefreshRange(buffer_, r.start, r.end);
	return ssize(ranges_);
}

/**
 * @brief Removes a specified range from the rangeset.
 *
 * @param r The TextRange to remove from the rangeset.
 * @return The new number of ranges in the set after removal.
 */
int64_t Rangeset::RangesetRemove(TextRange r) {

	if (r.start > r.end) {
		// quietly sort the positions
		std::swap(r.start, r.end);
	} else if (r.start == r.end) {
		// no-op - empty range == no range
		return ssize(ranges_);
	}

	if (ranges_.empty()) {
		return ssize(ranges_);
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

	RefreshRange(buffer_, r.start, r.end);
	return ssize(ranges_);
}

/**
 * @brief Adds the ranges of another Rangeset to this one.
 *
 * @param rhs The Rangeset to add to this one.
 * @return A reference to this Rangeset after the addition.
 */
Rangeset &Rangeset::operator+=(const Rangeset &rhs) {
	RangesetAdd(rhs);
	return *this;
}

/**
 * @brief Subtracts the ranges of another Rangeset from this one.
 *
 * @param rhs The Rangeset to subtract from this one.
 * @return A reference to this Rangeset after the subtraction.
 */
Rangeset &Rangeset::operator-=(const Rangeset &rhs) {
	RangesetRemove(rhs);
	return *this;
}

/**
 * @brief Inverts the rangeset, creating a new one that represents the complement of the current rangeset.
 *
 * @return A new Rangeset object that is the inverse of the current rangeset.
 */
Rangeset Rangeset::operator~() const {
	Rangeset ret(*this);
	ret.RangesetInverse();
	return ret;
}

/**
 * @brief Get information about the rangeset.
 *
 * @return A RangesetInfo structure containing details about the rangeset.
 */
RangesetInfo Rangeset::RangesetGetInfo() const {
	RangesetInfo info;
	info.defined = true;
	info.label   = static_cast<int>(label_);
	info.count   = ssize(ranges_);
	info.color   = color_name_;
	info.name    = name_;
	info.mode    = update_name_;
	return info;
}

/**
 * @brief Constructor for Rangeset.
 *
 * @param buffer The text buffer this rangeset is associated with.
 * @param label A label for the rangeset, used to identify it.
 */
Rangeset::Rangeset(TextBuffer *buffer, uint8_t label)
	: buffer_(buffer), label_(label) {
	setMode(DefaultUpdateFuncName);
}

/**
 * @brief Destructor for Rangeset.
 */
Rangeset::~Rangeset() {
	for (const TextRange &range : ranges_) {
		RefreshRange(buffer_, range.start, range.end);
	}
}
