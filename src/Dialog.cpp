
#include "Dialog.h"
#include "Preferences.h"

#include <QTimer>

/**
 * @brief Dialog::Dialog
 * @param parent passed to QDialog's constructor
 * @param f      passed to QDialog's constructor
 */
Dialog::Dialog(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
}

/**
 * @brief Dialog::showEvent
 * @param event
 *
 * reposition the dialog to be centered on the cursor if the setting is enabled
 */
void Dialog::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	if (Preferences::GetPrefRepositionDialogs()) {
		const QPoint pos = QCursor::pos();

		const int x = pos.x() - (width() / 2);
		const int y = pos.y() - (height() / 2);

		// no effort is made to fixup the location to prevent it from being
		// partially off screen. At least on KDE Plasma, this happens
		// automagically for us. We can revisit this if need be
		move(x, y);
	}
}

/**
 * @brief
 *
 * @param dialog
 *
 * Shrinks the size of the dialog to the minimum possible given its layout.
 * Does so in a timer with a 0 timeout so the event loop has a chance to actually
 * do the resize
 */
void Dialog::shrinkToFit(Dialog *dialog) {
	QTimer::singleShot(0, dialog, [dialog]() {
		dialog->resize(0, 0);
	});
}
