
#ifndef TEXT_BUFFER_H_
#define TEXT_BUFFER_H_

#include "gap_buffer.h"
#include "TextBufferFwd.h"
#include "TextCursor.h"
#include "TextRange.h"
#include "Util/string_view.h"

#include <gsl/gsl_util>

#include <deque>
#include <string>
#include <cstdint>

#include <boost/optional.hpp>

template <class Ch, class Tr>
class BasicTextBuffer {
public:
	using string_type = std::basic_string<Ch, Tr>;
	using view_type   = view::basic_string_view<Ch, Tr>;

public:
	using modify_callback_type     = void (*)(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view_type deletedText, void *user);
	using pre_delete_callback_type = void (*)(TextCursor pos, int64_t nDeleted, void *user);

private:
	/* Initial size for the buffer gap (empty space in the buffer where text
	 * might be inserted if the user is typing sequential chars)
	 */
	static constexpr int PreferredGapSize = 80;

public:
	/* Maximum length in characters of a tab or control character expansion
	 * of a single buffer character
	 */
	static constexpr int MAX_EXP_CHAR_LEN = 20;

	static constexpr int DefaultTabWidth = 8;

public:
	class Selection {
		template <class CharT, class Traits>
		friend class BasicTextBuffer;

	public:
		bool getSelectionPos(TextCursor *start, TextCursor *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const;
		bool inSelection(TextCursor pos, TextCursor lineStartPos, int64_t dispIndex) const;
		bool rangeTouchesRectSel(TextCursor rangeStart, TextCursor rangeEnd) const;
		void setRectSelect(TextCursor newStart, TextCursor newEnd, int64_t newRectStart, int64_t newRectEnd);
		void setSelection(TextCursor newStart, TextCursor newEnd);
		void updateSelection(TextCursor pos, int64_t nDeleted, int64_t nInserted);

	public:
		bool hasSelection() const      { return selected_; }
		bool isRectangular() const     { return rectangular_; }
		bool isZeroWidth() const       { return zeroWidth_; }
		TextCursor start() const       { return start_; }
		TextCursor end() const         { return end_; }
		int64_t rectStart() const      { return rectStart_; }
		int64_t rectEnd() const        { return rectEnd_; }

	public:
		bool selected_     = false; // true if the selection is active
		bool rectangular_  = false; // true if the selection is rectangular
		bool zeroWidth_    = false; // Width 0 selections aren't "real" selections, but they can be useful when creating rectangular selections from the keyboard.
		TextCursor start_  = {};    // Pos. of start of selection, or if rectangular start of line containing it.
		TextCursor end_    = {};    // Pos. of end of selection, or if rectangular end of line containing it.
		int64_t rectStart_ = 0;     // Indent of left edge of rect. selection
		int64_t rectEnd_   = 0;     // Indent of right edge of rect. selection
	};

public:
	BasicTextBuffer();
	explicit BasicTextBuffer(int64_t size);
	BasicTextBuffer(const BasicTextBuffer &)            = delete;
	BasicTextBuffer &operator=(const BasicTextBuffer &) = delete;
	~BasicTextBuffer() noexcept                         = default;

public:
	static int BufCharWidth(Ch ch, int64_t indent, int tabDist) noexcept;
	static int BufExpandCharacter(Ch ch, int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;
	static int BufExpandTab(int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN], int tabDist) noexcept;

public:
	Ch front() const noexcept;
	Ch back() const noexcept;

public:
	bool BufGetEmptySelectionPos(TextCursor *start, TextCursor *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const noexcept;
	bool BufGetSelectionPos(TextCursor *start, TextCursor *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const noexcept;
	bool BufGetSyncXSelection() const;
	bool BufGetUseTabs() const noexcept;
	bool BufIsEmpty() const noexcept;
	bool BufSetSyncXSelection(bool sync);
	boost::optional<TextCursor> searchBackward(TextCursor startPos, view_type searchChars) const noexcept;
	boost::optional<TextCursor> searchForward(TextCursor startPos, view_type searchChars) const noexcept;
	Ch BufGetCharacter(TextCursor pos) const noexcept;
	int64_t BufCountDispChars(TextCursor lineStartPos, TextCursor targetPos) const noexcept;
	int64_t BufCountLines(TextCursor startPos, TextCursor endPos) const noexcept;
	int64_t length() const noexcept;
	int compare(TextCursor pos, Ch ch) const noexcept;
	int compare(TextCursor pos, Ch *cmpText, int64_t size) const noexcept;
	int compare(TextCursor pos, view_type cmpText) const noexcept;
	int BufGetExpandedChar(TextCursor pos, int64_t indent, Ch outStr[MAX_EXP_CHAR_LEN]) const noexcept;
	int BufGetTabDistance() const noexcept;
	string_type BufGetAllEx() const;
	string_type BufGetRangeEx(TextCursor start, TextCursor end) const;
	string_type BufGetRangeEx(TextRange range) const;
	string_type BufGetSecSelectTextEx() const;
	string_type BufGetSelectionTextEx() const;
	string_type BufGetTextInRectEx(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) const;
	TextCursor BufCountBackwardNLines(TextCursor startPos, int64_t nLines) const noexcept;
	TextCursor BufCountForwardDispChars(TextCursor lineStartPos, int64_t nChars) const noexcept;
	TextCursor BufCountForwardNLines(TextCursor startPos, int64_t nLines) const noexcept;
	TextCursor BufCursorPosHint() const noexcept;
	TextCursor BufEndOfLine(TextCursor pos) const noexcept;
	TextCursor BufStartOfLine(TextCursor pos) const noexcept;
	TextCursor BufEndOfBuffer() const noexcept;
	constexpr TextCursor BufStartOfBuffer() const noexcept { return {}; }
	view_type BufAsStringEx() noexcept;
	void BufAddHighPriorityModifyCB(modify_callback_type bufModifiedCB, void *user);
	void BufAddModifyCB(modify_callback_type bufModifiedCB, void *user);
	void BufAddPreDeleteCB(pre_delete_callback_type bufPreDeleteCB, void *user);
	void BufAppendEx(Ch ch) noexcept;
	void BufAppendEx(view_type text) noexcept;
	void BufCheckDisplay(TextCursor start, TextCursor end) const noexcept;
	void BufClearRect(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) noexcept;
	void BufCopyFromBuf(BasicTextBuffer *fromBuf, TextCursor fromStart, TextCursor fromEnd, TextCursor toPos) noexcept;
	void BufHighlight(TextCursor start, TextCursor end) noexcept;
	void BufInsertColEx(int64_t column, TextCursor startPos, view_type text, int64_t *charsInserted, int64_t *charsDeleted) noexcept;
	void BufInsertEx(TextCursor pos, Ch ch) noexcept;
	void BufInsertEx(TextCursor pos, view_type text) noexcept;
	void BufOverlayRectEx(TextCursor startPos, int64_t rectStart, int64_t rectEnd, view_type text, int64_t *charsInserted, int64_t *charsDeleted) noexcept;
	void BufRectHighlight(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) noexcept;
	void BufRectSelect(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) noexcept;
	void BufRemoveModifyCB(modify_callback_type bufModifiedCB, void *user) noexcept;
	void BufRemovePreDeleteCB(pre_delete_callback_type bufPreDeleteCB, void *user) noexcept;
	void BufRemoveRect(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) noexcept;
	void BufRemoveSecSelect() noexcept;
	void BufRemoveSelected() noexcept;
	void BufRemove(TextCursor start, TextCursor end) noexcept;
	void BufReplaceEx(TextCursor start, TextCursor end, Ch ch) noexcept;
	void BufReplaceEx(TextCursor start, TextCursor end, view_type text) noexcept;
	void BufReplaceEx(TextRange range, view_type text) noexcept;
	void BufReplaceEx(TextRange range, Ch ch) noexcept;
	void BufReplaceRectEx(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd, view_type text);
	void BufReplaceSecSelectEx(view_type text) noexcept;
	void BufReplaceSelectedEx(view_type text) noexcept;
	void BufSecondarySelect(TextCursor start, TextCursor end) noexcept;
	void BufSecondaryUnselect() noexcept;
	void BufSecRectSelect(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd) noexcept;
	void BufSelectAll() noexcept;
	void BufSelect(TextCursor start, TextCursor end) noexcept;	
	void BufSelect(std::pair<TextCursor, TextCursor> range) noexcept;
	void BufSetAll(view_type text);
	void BufSetTabDistance(int distance, bool notify) noexcept;
	void BufSetUseTabs(bool useTabs) noexcept;
	void BufUnhighlight() noexcept;
	void BufUnselect() noexcept;

public:
	bool GetSimpleSelection(TextRange *range) const noexcept;

private:
	boost::optional<TextCursor> searchBackward(TextCursor startPos, Ch searchChar) const noexcept;
	boost::optional<TextCursor> searchForward(TextCursor startPos, Ch searchChar) const noexcept;
	int64_t insertEx(TextCursor pos, view_type text) noexcept;
	int64_t insertEx(TextCursor pos, Ch ch) noexcept;
	string_type getSelectionTextEx(const Selection *sel) const;
	void callModifyCBs(TextCursor pos, int64_t nDeleted, int64_t nInserted, int64_t nRestyled, view_type deletedText) const noexcept;
	void callPreDeleteCBs(TextCursor pos, int64_t nDeleted) const noexcept;
	void deleteRange(TextCursor start, TextCursor end) noexcept;
	void deleteRect(TextCursor start, TextCursor end, int64_t rectStart, int64_t rectEnd, int64_t *replaceLen, TextCursor *endPos);
	void findRectSelBoundariesForCopy(TextCursor lineStartPos, int64_t rectStart, int64_t rectEnd, TextCursor *selStart, TextCursor *selEnd) const noexcept;
	void insertColEx(int64_t column, TextCursor startPos, view_type insText, int64_t *nDeleted, int64_t *nInserted, TextCursor *endPos);
	void overlayRectEx(TextCursor startPos, int64_t rectStart, int64_t rectEnd, view_type insText, int64_t *nDeleted, int64_t *nInserted, TextCursor *endPos);
	void redisplaySelection(const Selection &oldSelection, Selection *newSelection) const noexcept;
	void removeSelected(const Selection *sel) noexcept;
	void replaceSelectedEx(Selection *sel, view_type text) noexcept;
	void updateSelections(TextCursor pos, int64_t nDeleted, int64_t nInserted) noexcept;
	void sanitizeRange(TextCursor &start, TextCursor &end) const noexcept;
	void updatePrimarySelection() noexcept;

private:
	static string_type unexpandTabs(view_type text, int64_t startIndent, int tabDist);
	static string_type expandTabs(view_type text, int64_t startIndent, int tabDist);
	static string_type realignTabs(view_type text, int64_t origIndent, int64_t newIndent, int tabDist, bool useTabs) noexcept;
	static void insertColInLine(view_type line, view_type insLine, int64_t column, int insWidth, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
	static void deleteRectFromLine(view_type line, int64_t rectStart, int64_t rectEnd, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
	static int textWidth(view_type text, int tabDist) noexcept;
	static int64_t countLines(view_type string) noexcept;
	static void overlayRectInLine(view_type line, view_type insLine, int64_t rectStart, int64_t rectEnd, int tabDist, bool useTabs, string_type *outStr, int64_t *endOffset) noexcept;
	static const Ch *controlCharacter(size_t index) noexcept;

private:
	template <class Out>
	static int addPadding(Out out, int64_t startIndent, int64_t toIndent, int tabDist, bool useTabs) noexcept;

	template <class Ran>
	static string_type copyLine(Ran first, Ran last);

private:
	TextCursor cursorPosHint_ = {};               // hint for reasonable cursor position after a buffer modification operation
	int tabDist_              = DefaultTabWidth;  // equiv. number of characters in a tab
	bool useTabs_             = true;             // true if buffer routines are allowed to use tabs for padding in rectangular operations
	bool syncXSelection_      = true;

private:
	gap_buffer<Ch> buffer_;

private:
	std::deque<std::pair<pre_delete_callback_type, void *>> preDeleteProcs_; // procedures to call before text is deleted from the buffer; at most one is supported.
	std::deque<std::pair<modify_callback_type, void *>> modifyProcs_;        // procedures to call when buffer is modified to redisplay contents

public:
	Selection primary;   // highlighted areas
	Selection secondary;
	Selection highlight;
};

#include "TextBuffer.tcc"

extern template class BasicTextBuffer<char>;
extern template class gap_buffer<char>;

#endif
