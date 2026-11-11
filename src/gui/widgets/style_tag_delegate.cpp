/// @file style_tag_delegate.cpp
/// @brief The names of paragraph styles in a list, with a tag after a name

#include "kalahari/gui/widgets/style_tag_delegate.h"

#include <QApplication>
#include <QListWidgetItem>
#include <QPainter>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The tag: its letters as a share of the list's, its padding, the gap before it and the
/// rounding of its corners, in pixels
constexpr qreal TAG_FONT_SHARE = 0.85;
constexpr int TAG_PADDING = 4;
constexpr int TAG_GAP = 8;
constexpr qreal TAG_RADIUS = 3.0;

}  // namespace

StyleTagDelegate::StyleTagDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QString StyleTagDelegate::tagText(editor::StyleOrigin origin) {
    switch (origin) {
        case editor::StyleOrigin::Own:
            //: The tag of a style of the writer's own in a list of styles. In Polish: własny
            return tr("own");
        case editor::StyleOrigin::Changed:
            //: The tag of a style of the book's type changed in the book. In Polish: zmieniony
            return tr("changed");
        case editor::StyleOrigin::Type:
            break;
    }
    return QString();
}

void StyleTagDelegate::tagItem(QListWidgetItem* item, editor::StyleOrigin origin) {
    item->setData(TAG_ROLE, tagText(origin));
    item->setData(ORIGIN_ROLE, static_cast<int>(origin));
    item->setData(MISSING_ROLE, false);
}

void StyleTagDelegate::tagMissing(QListWidgetItem* item) {
    //: The tag of a style of paragraphs pasted from another book, which this book does not
    //: have, in a list of styles. In Polish: brak w tej książce
    item->setData(TAG_ROLE, tr("not in this book"));
    item->setData(ORIGIN_ROLE, static_cast<int>(editor::StyleOrigin::Type));
    item->setData(MISSING_ROLE, true);
}

QFont StyleTagDelegate::tagFont(const QStyleOptionViewItem& option) {
    QFont font = option.widget != nullptr ? option.widget->font() : QApplication::font();
    font.setPointSizeF(font.pointSizeF() > 0 ? font.pointSizeF() * TAG_FONT_SHARE
                                             : QFontInfo(font).pointSizeF() * TAG_FONT_SHARE);
    return font;
}

int StyleTagDelegate::tagWidth(const QStyleOptionViewItem& option, const QString& tag) {
    return QFontMetrics(tagFont(option)).horizontalAdvance(tag) + 2 * TAG_PADDING + TAG_GAP;
}

void StyleTagDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
    const QString tag = index.data(TAG_ROLE).toString();
    if (tag.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QStyleOptionViewItem item(option);
    initStyleOption(&item, index);
    const int width = tagWidth(option, tag);

    // The name, cut short before the tag
    const QStyle* style = item.widget != nullptr ? item.widget->style() : QApplication::style();
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &item, item.widget);
    const int room = std::max(textRect.width() - width, 0);
    item.text = item.fontMetrics.elidedText(item.text, item.textElideMode, room);
    style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);

    // The tag at the right end, in a frame
    const QFont font = tagFont(option);
    const QFontMetrics metrics(font);
    const int tagHeight = metrics.height() + 2;
    const QRect frame(textRect.right() - width + TAG_GAP + 1,
                      textRect.top() + (textRect.height() - tagHeight) / 2, width - TAG_GAP,
                      tagHeight);
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const auto origin = static_cast<editor::StyleOrigin>(index.data(ORIGIN_ROLE).toInt());
    const QColor color =
        selected ? option.palette.color(QPalette::HighlightedText)
        : origin == editor::StyleOrigin::Own ? option.palette.color(QPalette::Link)
                                             : option.palette.color(QPalette::PlaceholderText);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(font);
    painter->setPen(color);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(QRectF(frame).adjusted(0.5, 0.5, -0.5, -0.5), TAG_RADIUS, TAG_RADIUS);
    painter->drawText(frame, Qt::AlignCenter, tag);
    painter->restore();
}

QSize StyleTagDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    const QString tag = index.data(TAG_ROLE).toString();
    if (!tag.isEmpty()) {
        size.rwidth() += tagWidth(option, tag);
        size.setHeight(std::max(size.height(), QFontMetrics(tagFont(option)).height() + 4));
    }
    return size;
}

}  // namespace kalahari::gui
