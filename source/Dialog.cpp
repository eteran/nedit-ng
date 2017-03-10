
#include "Dialog.h"
#include "preferences.h"

Dialog::Dialog(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
}

void Dialog::showEvent(QShowEvent *event) {
    Q_UNUSED(event);

    if(GetPrefRepositionDialogs()) {
        QPoint pos = QCursor::pos();

        int x = pos.x() - (width() / 2);
        int y = pos.y() - (height() / 2);

        // NOTE(eteran): no effort is made to fixup the location to prevent it
        // from being partially off screen. At least on KDE Plasma, this happens
        // automagically for us. We can revisit this if need be

        move(x, y);
    }
}
