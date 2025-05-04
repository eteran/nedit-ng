
#include "WindowMenuEvent.h"
#include "CommandRecorder.h"

/**
 * @brief
 *
 * @param macroString
 * @param arguments
 */
WindowMenuEvent::WindowMenuEvent(QString macroString, QStringList arguments)
	: QEvent(eventType), macroString_(std::move(macroString)), arguments_(std::move(arguments)) {
}

/**
 * @brief
 *
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
 * @brief
 *
 * @return
 */
QString WindowMenuEvent::toString() const {
	return QLatin1String("%1(%2)").arg(macroString_, argumentString());
}

/**
 * @brief
 *
 * @return
 */
QString WindowMenuEvent::actionString() const {
	return macroString_;
}
