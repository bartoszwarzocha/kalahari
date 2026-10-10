/// @file wrapping_label.cpp
/// @brief A label in one line where it has room, which wraps its text where it has less

#include "kalahari/gui/widgets/wrapping_label.h"

#include <QFontMetrics>
#include <QRect>
#include <QTextDocument>

namespace kalahari {
namespace gui {

namespace {

/// @brief Room for the text in QLabel's measuring (as in QLabelPrivate::sizeForWidth)
constexpr int MEASURE_ROOM = 2000;

} // namespace

WrappingLabel::WrappingLabel(const QString& text, QWidget* parent) : QLabel(text, parent) {
    setWordWrap(true);
}

QSize WrappingLabel::sizeHint() const {
    const bool richText = textFormat() == Qt::RichText ||
                          (textFormat() == Qt::AutoText && Qt::mightBeRichText(text()));
    if (richText || text().isEmpty()) {
        return QLabel::sizeHint();
    }
    // The text's lines as QLabel measures them, without wrapping
    int flags = Qt::AlignLeft | Qt::AlignTop;
    if (buddy() != nullptr) {
        flags |= Qt::TextShowMnemonic;
    }
    const QRect lines = fontMetrics().boundingRect(0, 0, MEASURE_ROOM, MEASURE_ROOM, flags, text());
    const QMargins margins = contentsMargins();
    const int width = lines.width() + 2 * margin() + margins.left() + margins.right();
    return QSize(width, heightForWidth(width)).expandedTo(minimumSize());
}

} // namespace gui
} // namespace kalahari
