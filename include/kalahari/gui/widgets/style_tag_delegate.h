/// @file style_tag_delegate.h
/// @brief The names of paragraph styles in a list, with a tag after a name: "own", "changed",
///        "not in this book"

#pragma once

#include "kalahari/editor/paragraph_styles.h"

#include <QCoreApplication>
#include <QStyledItemDelegate>

class QListWidgetItem;

namespace kalahari::gui {

/// @brief Draws the items of a list of paragraph styles with a tag at the right end
///
/// A style of the writer's own gets "own" in the color of links, a style of the book's type
/// changed in the book "changed" in the color of hints; a style of the type as the type has
/// it gets none. A style of paragraphs from another book, which this book does not have, gets
/// "not in this book" in the color of hints. The name is cut short before the tag, never under
/// it.
class StyleTagDelegate : public QStyledItemDelegate {
    Q_DECLARE_TR_FUNCTIONS(StyleTagDelegate)

public:
    /// @brief The role of the text of an item's tag, and of its kind (editor::StyleOrigin)
    static constexpr int TAG_ROLE = Qt::UserRole + 20;
    static constexpr int ORIGIN_ROLE = Qt::UserRole + 21;
    /// @brief The role that marks a style the book does not have (true)
    static constexpr int MISSING_ROLE = Qt::UserRole + 22;

    explicit StyleTagDelegate(QObject* parent = nullptr);

    /// @brief Give an item the tag of a style of its origin (none for a style of the type)
    static void tagItem(QListWidgetItem* item, editor::StyleOrigin origin);

    /// @brief Give an item the tag of a style the book does not have
    static void tagMissing(QListWidgetItem* item);

    /// @brief The tag of a style of an origin: "own", "changed"; empty for a style of the type
    [[nodiscard]] static QString tagText(editor::StyleOrigin origin);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    /// @brief The font of a tag: the list's, a little smaller
    [[nodiscard]] static QFont tagFont(const QStyleOptionViewItem& option);

    /// @brief The width a tag takes, with its padding and the gap before it
    [[nodiscard]] static int tagWidth(const QStyleOptionViewItem& option, const QString& tag);
};

}  // namespace kalahari::gui
