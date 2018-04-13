
#include "TextEditEvent.h"
#include "CommandRecorder.h"

/**
 * @brief TextEditEvent::TextEditEvent
 * @param macroString
 * @param argument
 * @param flags
 */
TextEditEvent::TextEditEvent(QString macroString, QString argument, TextArea::EventFlags flags) : QEvent(eventType), macroString_(std::move(macroString)), argument_(std::move(argument)), flags_(flags) {
}

/**
 * @brief TextEditEvent::TextEditEvent
 * @param macroString
 * @param flags
 */
TextEditEvent::TextEditEvent(QString macroString, TextArea::EventFlags flags) : QEvent(eventType), macroString_(std::move(macroString)), flags_(flags) {
}

/**
 * @brief TextEditEvent::argumentString
 * @return
 */
QString TextEditEvent::argumentString() const {
    QStringList args;

    if(!argument_.isEmpty()) {
        args << CommandRecorder::quoteString(CommandRecorder::escapeString(argument_));
    }

    if(flags_ & TextArea::AbsoluteFlag)  args << QLatin1String("\"absolute\"");
    if(flags_ & TextArea::ColumnFlag)    args << QLatin1String("\"column\"");
    if(flags_ & TextArea::CopyFlag)      args << QLatin1String("\"copy\"");
    if(flags_ & TextArea::DownFlag)      args << QLatin1String("\"down\"");
    if(flags_ & TextArea::ExtendFlag)    args << QLatin1String("\"extend\"");
    if(flags_ & TextArea::LeftFlag)      args << QLatin1String("\"left\"");
    if(flags_ & TextArea::OverlayFlag)   args << QLatin1String("\"overlay\"");
    if(flags_ & TextArea::RectFlag)      args << QLatin1String("\"rect\"");
    if(flags_ & TextArea::RightFlag)     args << QLatin1String("\"right\"");
    if(flags_ & TextArea::UpFlag)        args << QLatin1String("\"up\"");
    if(flags_ & TextArea::WrapFlag)      args << QLatin1String("\"wrap\"");
    if(flags_ & TextArea::TailFlag)      args << QLatin1String("\"tail\"");
    if(flags_ & TextArea::StutterFlag)   args << QLatin1String("\"stutter\"");
    if(flags_ & TextArea::ScrollbarFlag) args << QLatin1String("\"scrollbar\"");
    if(flags_ & TextArea::NoBellFlag)    args << QLatin1String("\"nobell\"");

    return args.join(QLatin1String(","));
}

/**
 * @brief TextEditEvent::toString
 * @return
 */
QString TextEditEvent::toString() const {
    return QString(QLatin1String("%1(%2)")).arg(macroString_, argumentString());
}

/**
 * @brief TextEditEvent::actionString
 * @return
 */
QString TextEditEvent::actionString() const {
    return macroString_;
}
