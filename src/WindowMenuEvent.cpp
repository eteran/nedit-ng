
#include "WindowMenuEvent.h"
#include "CommandRecorder.h"

/**
 * @brief WindowMenuEvent::WindowMenuEvent
 * @param macroString
 * @param arguments
 */
WindowMenuEvent::WindowMenuEvent(QString macroString, QStringList arguments)
	: QEvent(eventType), macroString_(std::move(macroString)), arguments_(std::move(arguments)) {
}

/**
 * @brief WindowMenuEvent::argumentString
 * @return
 */
QString WindowMenuEvent::argumentString() const {
	QStringList args;

	std::transform(arguments_.begin(), arguments_.end(), std::back_inserter(args), [](const QString &arg) {
		return CommandRecorder::quoteString(CommandRecorder::escapeString(arg));
	});

	return args.join(QLatin1Char(','));
}

/**
 * @brief WindowMenuEvent::toString
 * @return
 */
QString WindowMenuEvent::toString() const {
	return QString(QLatin1String("%1(%2)")).arg(macroString_, argumentString());
}

/**
 * @brief WindowMenuEvent::actionString
 * @return
 */
QString WindowMenuEvent::actionString() const {
	return macroString_;
}
