/// @file flow_layout.h
/// @brief A layout that puts its items side by side and goes on in the next row when a row is
/// full

#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>

class QLayoutItem;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief Items side by side, in more rows when the layout is narrow
///
/// A row takes as many items as fit in it at their preferred widths; the next item starts
/// the next row. In a wide panel the items stand in one row, in a narrow one (a small screen)
/// in two or three, so the panel can be narrower than all its items side by side. An item
/// that is to expand (a combo box with an Expanding policy) takes the room left in its row,
/// as far as its maximum width allows. Items are centered in the height of their row.
///
/// The layout's height depends on its width (hasHeightForWidth()): a layout above it gives it
/// the rows it needs.
class FlowLayout : public QLayout {
    Q_OBJECT

public:
    /// @brief Create the layout
    /// @param parent Widget the layout is for; nullptr: a layout in another layout
    explicit FlowLayout(QWidget* parent = nullptr);

    /// @brief Delete the items
    ~FlowLayout() override;

    FlowLayout(const FlowLayout&) = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    /// @brief No direction: the rows take the room their items need
    Qt::Orientations expandingDirections() const override;

    /// @brief Yes: in a narrower layout the items take more rows
    bool hasHeightForWidth() const override;

    /// @brief The height of the rows the items take in @p width
    int heightForWidth(int width) const override;

    /// @brief All the items in one row
    QSize sizeHint() const override;

    /// @brief As wide as the widest item and as high as one row; the rows the items take at a
    /// given width come from heightForWidth()
    QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief Put the items in rows in @p rect (only count, with @p place false)
    /// @return The height the rows take, with the margins
    int arrange(const QRect& rect, bool place) const;

    /// @brief The space between the items (and between the rows)
    int gap() const;

    QList<QLayoutItem*> m_items;
};

} // namespace gui
} // namespace kalahari
