/// @file flow_layout.h
/// @brief A row of items that goes on in the next line where the panel is too narrow

#pragma once

#include <QLayout>
#include <QList>

class QLayoutItem;
class QRect;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief Items left to right, in as many lines as the width needs
///
/// Each item is as wide as it likes, or as the line where that is less (not less than its
/// minimum). An item that expands sideways (a combo box set so) takes the room its line
/// leaves. On a small screen (1366x768 at 150%, the side panels open) a row of buttons then
/// wraps instead of making the whole panel wider.
class FlowLayout : public QLayout {
public:
    /// @brief Constructor
    /// @param parent Widget the layout arranges
    /// @param spacing Between the items in a line
    /// @param lineSpacing Between the lines
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = 6, int lineSpacing = 6);
    ~FlowLayout() override;

    FlowLayout(const FlowLayout&) = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;

    [[nodiscard]] int lineSpacing() const { return m_lineSpacing; }

    /// @brief Add a layout as one item: its widgets stay together in a line
    void addLayout(QLayout* layout);

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief All the items in one line
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief As narrow as the widest item can be, one line high (the lines a width needs
    ///        come from heightForWidth())
    [[nodiscard]] QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief Place the items in @p rect, or only measure them
    /// @return The height they take, with the margins
    int arrange(const QRect& rect, bool place) const;

    QList<QLayoutItem*> m_items;
    int m_lineSpacing;
};

} // namespace gui
} // namespace kalahari
