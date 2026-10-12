/// @file segmented_layout.h
/// @brief Joined buttons of a choice: in a row, or one under another on a narrow panel

#pragma once

#include <QLayout>
#include <QList>

class QLayoutItem;
class QRect;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief Joined buttons in a row where it has the room, else one under another, all as
///        wide as the widest
///
/// Each widget may grow sideways (its horizontal size policy becomes Preferred) and gets the
/// property "stacked" ("false" in the row, "true" one under another), so
/// that a style sheet can join them on the right sides; it is polished again when that
/// changes. On a small screen (1366x768 at 150%) the choice then keeps all its buttons in
/// sight and joined instead of making the whole panel wider.
class SegmentedLayout : public QLayout {
public:
    /// @brief Constructor
    /// @param parent Widget the layout arranges
    explicit SegmentedLayout(QWidget* parent = nullptr);
    ~SegmentedLayout() override;

    SegmentedLayout(const SegmentedLayout&) = delete;
    SegmentedLayout& operator=(const SegmentedLayout&) = delete;

    /// @brief Whether the buttons are one under another (after the last arrangement)
    [[nodiscard]] bool isStacked() const { return m_stacked; }

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief The buttons in a row
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief As narrow as the buttons one under another, one button high (the height one
    ///        under another comes from heightForWidth())
    [[nodiscard]] QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief The width of the buttons in a row
    [[nodiscard]] int rowWidth() const;

    /// @brief The width of the widest button
    [[nodiscard]] int columnWidth() const;

    /// @brief The height of the tallest button
    [[nodiscard]] int buttonHeight() const;

    /// @brief Tell the widgets how they are arranged
    void setStacked(bool stacked);

    QList<QLayoutItem*> m_items;
    bool m_stacked = false;
};

} // namespace gui
} // namespace kalahari
