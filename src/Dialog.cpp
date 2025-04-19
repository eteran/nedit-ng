
#include "Dialog.h"
#include "Preferences.h"

#include <QTimer>

/**
 * @brief Constructor for Dialog class
 *
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
Dialog::Dialog(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
}

/**
 * @brief Handles the show event for the dialog.
 * Repositions the dialog to be centered on the cursor if the setting is enabled.
 *
 * @param event The show event that triggered this method.
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
 * @brief Shrink the dialog to fit its contents.
 * Shrinks the size of the dialog to the minimum possible given its layout.
 * Does so in a timer with a 0 timeout so the event loop has a chance to actually
 * do the resize
 *
 * @param dialog The dialog to shrink
 */
void Dialog::shrinkToFit(Dialog *dialog) {
	QTimer::singleShot(0, dialog, [dialog]() {
		dialog->resize(0, 0);
	});
}
