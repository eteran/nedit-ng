
#include "TextEditEvent.h"
#include "CommandRecorder.h"

/**
 * @brief Constructor for TextEditEvent.
 *
 * @param action The action associated with the event.
 * @param argument The argument associated with the event.
 * @param flags The flags associated with the event.
 */
TextEditEvent::TextEditEvent(QString action, TextArea::EventFlags flags, QString argument)
	: QEvent(eventType), action_(std::move(action)), argument_(std::move(argument)), flags_(flags) {
}

/**
 * @brief Constructor for TextEditEvent without an argument.
 *
 * @param action The action associated with the event.
 * @param flags The flags associated with the event.
 */
TextEditEvent::TextEditEvent(QString action, TextArea::EventFlags flags)
	: QEvent(eventType), action_(std::move(action)), flags_(flags) {
}

/**
 * @brief Generates a string representation of the event's arguments.
 *
 * @return A string representation of the event's arguments.
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
 * @brief Generates a string representation of the event.
 *
 * @return A string representation of the event.
 */
QString TextEditEvent::toString() const {
	return QStringLiteral("%1(%2)").arg(action_, argumentString());
}

/**
 * @brief Returns the action associated with the event.
 *
 * @return The action associated with the event.
 */
QString TextEditEvent::actionString() const {
	return action_;
}
