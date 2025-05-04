
#include "TextEditEvent.h"
#include "CommandRecorder.h"

/**
 * @brief
 *
 * @param macroString
 * @param argument
 * @param flags
 */
TextEditEvent::TextEditEvent(QString macroString, TextArea::EventFlags flags, QString argument)
	: QEvent(eventType), macroString_(std::move(macroString)), argument_(std::move(argument)), flags_(flags) {
}

/**
 * @brief
 *
 * @param macroString
 * @param flags
 */
TextEditEvent::TextEditEvent(QString macroString, TextArea::EventFlags flags)
	: QEvent(eventType), macroString_(std::move(macroString)), flags_(flags) {
}

/**
 * @brief
 *
 * @return
 */
QString TextEditEvent::argumentString() const {
	QStringList args;

	if (!argument_.isEmpty()) {
		args << CommandRecorder::quoteString(CommandRecorder::escapeString(argument_));
	}

	if (flags_ & TextArea::AbsoluteFlag) {
		args << QStringLiteral("\"absolute\"");
	}
	if (flags_ & TextArea::ColumnFlag) {
		args << QStringLiteral("\"column\"");
	}
	if (flags_ & TextArea::CopyFlag) {
		args << QStringLiteral("\"copy\"");
	}
	if (flags_ & TextArea::DownFlag) {
		args << QStringLiteral("\"down\"");
	}
	if (flags_ & TextArea::ExtendFlag) {
		args << QStringLiteral("\"extend\"");
	}
	if (flags_ & TextArea::LeftFlag) {
		args << QStringLiteral("\"left\"");
	}
	if (flags_ & TextArea::OverlayFlag) {
		args << QStringLiteral("\"overlay\"");
	}
	if (flags_ & TextArea::RectFlag) {
		args << QStringLiteral("\"rect\"");
	}
	if (flags_ & TextArea::RightFlag) {
		args << QStringLiteral("\"right\"");
	}
	if (flags_ & TextArea::UpFlag) {
		args << QStringLiteral("\"up\"");
	}
	if (flags_ & TextArea::WrapFlag) {
		args << QStringLiteral("\"wrap\"");
	}
	if (flags_ & TextArea::TailFlag) {
		args << QStringLiteral("\"tail\"");
	}
	if (flags_ & TextArea::StutterFlag) {
		args << QStringLiteral("\"stutter\"");
	}
	if (flags_ & TextArea::ScrollbarFlag) {
		args << QStringLiteral("\"scrollbar\"");
	}
	if (flags_ & TextArea::NoBellFlag) {
		args << QStringLiteral("\"nobell\"");
	}

	return args.join(QStringLiteral(","));
}

/**
 * @brief
 *
 * @return
 */
QString TextEditEvent::toString() const {
	return QStringLiteral("%1(%2)").arg(macroString_, argumentString());
}

/**
 * @brief
 *
 * @return
 */
QString TextEditEvent::actionString() const {
	return macroString_;
}
