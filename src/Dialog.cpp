
#include "Dialog.h"
#include "Preferences.h"

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
		QPoint pos = QCursor::pos();

		int x = pos.x() - (width() / 2);
		int y = pos.y() - (height() / 2);

		// no effort is made to fixup the location to prevent it from being
		// partially off screen. At least on KDE Plasma, this happens
		// automagically for us. We can revisit this if need be
		move(x, y);
	}
}
