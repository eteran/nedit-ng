
#include "WindowMenuEvent.h"
#include "CommandRecorder.h"

/**
 * @brief Constructor for WindowMenuEvent.
 *
 * @param action The action triggering the event.
 * @param arguments The arguments for the action.
 */
WindowMenuEvent::WindowMenuEvent(QString action, QStringList arguments)
	: QEvent(eventType), action_(std::move(action)), arguments_(std::move(arguments)) {
}

/**
 * @brief Returns the argument string for the event.
 *
 * @return A string representation of the arguments.
 */
QString WindowMenuEvent::argumentString() const {
	QStringList args;

	std::transform(arguments_.begin(), arguments_.end(), std::back_inserter(args), [](const QString &arg) {
		return CommandRecorder::quoteString(CommandRecorder::escapeString(arg));
	});

	return args.join(QLatin1Char(','));
}

/**
 * @brief Returns a string representation of the event.
 *
 * @return A string representation of the event.
 * @note The format is "action(arguments)".
 */
QString WindowMenuEvent::toString() const {
	return QStringLiteral("%1(%2)").arg(action_, argumentString());
}

/**
 * @brief Returns the action string for the event.
 *
 * @return The action string associated with the event.
 */
QString WindowMenuEvent::actionString() const {
	return action_;
}
