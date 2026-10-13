/// @file height_for_width_scroll_area.cpp
/// @brief A scroll area as high as its content needs at its width, which scrolls only in a
///        panel too low for it

#include "kalahari/gui/widgets/height_for_width_scroll_area.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QLayout>
#include <QScrollBar>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {

HeightForWidthScrollArea::HeightForWidthScrollArea(QWidget* content, int minimumLines,
                                                   QWidget* parent)
    : QScrollArea(parent), m_minimumLines(std::max(minimumLines, 1)) {
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::NoFocus);
    setWidgetResizable(true);
    setWidget(content);
    content->installEventFilter(this);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}

bool HeightForWidthScrollArea::hasHeightForWidth() const {
    return true;
}

int HeightForWidthScrollArea::heightForWidth(int width) const {
    const QWidget* content = widget();
    const int borders = 2 * frameWidth();
    if (const QLayout* layout = content->layout()) {
        return layout->totalHeightForWidth(width - borders) + borders;
    }
    return content->sizeHint().height() + borders;
}

QSize HeightForWidthScrollArea::sizeHint() const {
    const int borders = 2 * frameWidth();
    const int width = widget()->sizeHint().width() + borders;
    return {width, heightForWidth(this->width() > 0 ? this->width() : width)};
}

QSize HeightForWidthScrollArea::minimumSizeHint() const {
    const int borders = 2 * frameWidth();
    const int lines = fontMetrics().lineSpacing() * m_minimumLines + borders;
    return {widget()->minimumSizeHint().width() + verticalScrollBar()->sizeHint().width() +
                borders,
            std::min(widget()->minimumSizeHint().height() + borders, lines)};
}

bool HeightForWidthScrollArea::eventFilter(QObject* watched, QEvent* event) {
    if (watched == widget() && event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    return QScrollArea::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
