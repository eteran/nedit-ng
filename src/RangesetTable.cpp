
#include "RangesetTable.h"
#include "TextBuffer.h"

#include <algorithm>
#include <array>
#include <iterator>

#include <gsl/gsl_util>

namespace {

// --------------------------------------------------------------------------

constexpr std::array<uint8_t, N_RANGESETS> RangesetLabels = {
	{58, 10, 15, 1, 27, 52, 14, 3, 61, 13, 31, 30, 45, 28, 41, 55,
	 33, 20, 62, 34, 42, 18, 57, 47, 24, 49, 19, 50, 25, 38, 40, 2,
	 21, 39, 59, 22, 60, 4, 6, 16, 29, 37, 48, 46, 54, 43, 32, 56,
	 51, 7, 9, 63, 5, 8, 36, 44, 26, 11, 23, 17, 53, 35, 12}};

// --------------------------------------------------------------------------

/**
 * @brief
 *
 * @param pos
 * @param nInserted
 * @param nDeleted
 * @param nRestyled
 * @param deletedText
 * @param user
 */
void RangesetBufModifiedCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, std::string_view deletedText, void *user) {
	Q_UNUSED(nRestyled)

	if (auto *table = static_cast<RangesetTable *>(user)) {
		if ((nInserted != nDeleted) || table->buffer_->compare(pos, deletedText) != 0) {
			table->updatePos(pos, nInserted, nDeleted);
		}
	}
}

}

/**
 * @brief Constructor for RangesetTable
 *
 * @param buffer
 */
RangesetTable::RangesetTable(TextBuffer *buffer)
	: buffer_(buffer) {
	/* Range sets must be updated before the text display callbacks are
	   called to avoid highlighted ranges getting out of sync. */
	buffer->BufAddHighPriorityModifyCB(RangesetBufModifiedCB, this);
}

/**
 * @brief
 */
RangesetTable::~RangesetTable() {
	buffer_->BufRemoveModifyCB(RangesetBufModifiedCB, this);
}

/**
 * @brief Fetch the rangeset identified by label.
 *
 * @param label
 * @return
 */
Rangeset *RangesetTable::fetch(int label) {
	auto it = std::find_if(sets_.begin(), sets_.end(), [label](const Rangeset &set) {
		return set.label_ == label;
	});

	if (it == sets_.end()) {
		return nullptr;
	}

	return &*it;
}

/**
 * @brief Forget the rangeset identified by label.
 *
 * @param label
 */
void RangesetTable::forgetLabel(int label) {
	auto it = std::find_if(sets_.begin(), sets_.end(), [label](const Rangeset &set) {
		return set.label_ == label;
	});

	if (it != sets_.end()) {
		sets_.erase(it);
	}
}

/**
 * @brief Return the color name, if any.
 *
 * @param index
 * @return
 */
QString RangesetTable::getColorName(size_t index) const {
	Q_ASSERT(index <= sets_.size());

	const Rangeset &set = sets_[index];
	return set.color_name_;
}

/**
 * @brief Find the index of the first range which includes the position.
 * Returns the index of the rangeset in the rangeset table + 1 if an including
 * rangeset was found, 0 otherwise. If needs_color is `true`, "colorless" ranges
 * will be skipped.
 *
 * @param pos
 * @param needs_color
 * @return
 */
size_t RangesetTable::index1ofPos(TextCursor pos, bool needs_color) {

	for (size_t i = 0; i < sets_.size(); ++i) {
		Rangeset &set = sets_[i];
		if (set.RangesetCheckRangeOfPos(pos) >= 0) {
			if (needs_color && set.color_set_ >= 0 && !set.color_name_.isNull()) {
				return i + 1;
			}
		}
	}

	return 0;
}

/**
 * @brief Assign a color pixel value to a rangeset via the rangeset table.
 * If the color is invalid, the color_set flag is set to an invalid (negative) value.
 *
 * @param index
 * @param color
 */
void RangesetTable::assignColor(size_t index, const QColor &color) {
	Q_ASSERT(index <= sets_.size());

	Rangeset &set = sets_[index];

	set.color_set_ = color.isValid() ? 1 : -1;
	set.color_     = color;
}

/**
 * @brief Return the color color validity, if any, and the value in *color.
 *
 * @param index
 * @param color
 * @return
 */
int RangesetTable::getColorValid(size_t index, QColor *color) const {
	Q_ASSERT(index <= sets_.size());

	const Rangeset &set = sets_[index];

	*color = set.color_;
	return set.color_set_;
}

/**
 * @brief Return the number of rangesets that are available to create.
 *
 * @return
 */
int RangesetTable::rangesetsAvailable() const {
	return static_cast<int>(N_RANGESETS - sets_.size());
}

/**
 * @brief
 *
 * @return
 */
std::vector<uint8_t> RangesetTable::labels() const {
	std::vector<uint8_t> list;
	list.reserve(sets_.size());

	std::transform(sets_.begin(), sets_.end(), std::back_inserter(list), [](const Rangeset &set) {
		return set.label_;
	});

	return list;
}

/**
 * @brief
 *
 * @param pos
 * @param ins
 * @param del
 */
void RangesetTable::updatePos(TextCursor pos, int64_t ins, int64_t del) {

	if (ins == 0 && del == 0) {
		return;
	}

	for (Rangeset &set : sets_) {
		set.update_(&set, pos, ins, del);
	}
}

/**
 * @brief Create a new empty rangeset.
 *
 * @return
 */
int RangesetTable::create() {

	const std::vector<uint8_t> list = labels();

	// find the first label not used
	const auto it = std::find_if(RangesetLabels.begin(), RangesetLabels.end(), [&list](char ch) {
		return std::find(list.begin(), list.end(), ch) == list.end();
	});

	if (it == RangesetLabels.end()) {
		return 0;
	}

	const size_t labelIndex = (it - RangesetLabels.begin());
	const uint8_t label     = RangesetLabels[labelIndex];

	sets_.insert(sets_.begin(), Rangeset(buffer_, label));
	return label;
}

/**
 * @brief Return `true` if label is a valid identifier for a range set.
 *
 * @param label
 * @return
 */
bool RangesetTable::labelOK(int label) {
	return std::find(RangesetLabels.begin(), RangesetLabels.end(), label) != RangesetLabels.end();
}
