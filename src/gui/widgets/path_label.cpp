/// @file path_label.cpp
/// @brief A label with a file path, which wraps where its line ends, whatever the path's names

#include "kalahari/gui/widgets/path_label.h"

#include <QDir>
#include <QPainter>
#include <QStyle>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief The path with the system's separators, which a line may end after
QString breakablePath(const QString& path) {
    // A zero-width space after each separator: a line ends there rather than in a name
    const QChar zeroWidthSpace(0x200B);
    QString result = QDir::toNativeSeparators(path);
    result.replace(QLatin1Char('\\'), QStringLiteral("\\") + zeroWidthSpace);
    result.replace(QLatin1Char('/'), QStringLiteral("/") + zeroWidthSpace);
    return result;
}

} // namespace

PathLabel::PathLabel(const QString& path, QWidget* parent) : QLabel(breakablePath(path), parent) {
    setTextFormat(Qt::PlainText);
    // Its height follows its width (QLabel gives the size policy height for width)
    setWordWrap(true);
}

QSize PathLabel::sizeHint() const {
    QTextLayout layout(text(), font(), this);
    const QSizeF lines = layOut(layout, QWIDGETSIZE_MAX);
    return QSize(qCeil(lines.width()), qCeil(lines.height())) + room();
}

QSize PathLabel::minimumSizeHint() const {
    const QFontMetrics metrics = fontMetrics();
    return QSize(metrics.horizontalAdvance(QLatin1Char('W')), metrics.height()) + room();
}

int PathLabel::heightForWidth(int width) const {
    QTextLayout layout(text(), font(), this);
    const QSize around = room();
    const qreal lineWidth = std::max(width - around.width(), 1);
    return qCeil(layOut(layout, lineWidth).height()) + around.height();
}

void PathLabel::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    drawFrame(&painter);
    const QRect area = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    QTextLayout layout(text(), font(), this);
    const QSizeF lines = layOut(layout, area.width());
    QPointF origin = area.topLeft();
    if (alignment() & Qt::AlignVCenter) {
        origin.ry() += (area.height() - lines.height()) / 2;
    } else if (alignment() & Qt::AlignBottom) {
        origin.ry() += area.height() - lines.height();
    }
    // The color of the label's palette, which its style sheet sets
    painter.setPen(palette().color(foregroundRole()));
    layout.draw(&painter, origin);
}

QSizeF PathLabel::layOut(QTextLayout& layout, qreal width) const {
    QTextOption option(QStyle::visualAlignment(layoutDirection(), alignment()) &
                       Qt::AlignHorizontal_Mask);
    // After a separator where it can, inside a name where the name is longer than the line
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    qreal height = 0;
    qreal longest = 0;
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        line.setPosition(QPointF(0, height));
        height += line.height();
        longest = std::max(longest, line.naturalTextWidth());
    }
    layout.endLayout();
    return {longest, height};
}

QSize PathLabel::room() const {
    const QMargins margins = contentsMargins();
    return {2 * margin() + margins.left() + margins.right(),
            2 * margin() + margins.top() + margins.bottom()};
}

} // namespace gui
} // namespace kalahari
