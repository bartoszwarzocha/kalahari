/// @file dashboard_layouts.h
/// @brief The layouts of the Dashboard, which fit it to a narrow or a low panel
///
/// On a small screen (1366x768 at 150%, the side panels open) the Dashboard is a narrow
/// column: its parts go one under another and its texts wrap, so that nothing is cut off.

#pragma once

#include <QLayout>
#include <QList>

class QLayoutItem;
class QRect;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief The Dashboard's content in the middle of the panel: three quarters of its width,
///        or as wide as the content likes where that is more, with a margin on each side
///
/// The content is narrower than it likes only on a narrow panel, where its texts wrap. A
/// panel narrower than the content can be at all scrolls sideways (its scroll area).
class DashboardContentLayout : public QLayout {
public:
    static constexpr int SIDE_MARGIN = 16;         ///< Least room left and right of the content
    static constexpr double CONTENT_SHARE = 0.75;  ///< The content's share of a wide panel

    /// @brief Constructor
    /// @param parent Widget the layout arranges
    explicit DashboardContentLayout(QWidget* parent = nullptr);
    ~DashboardContentLayout() override;

    DashboardContentLayout(const DashboardContentLayout&) = delete;
    DashboardContentLayout& operator=(const DashboardContentLayout&) = delete;

    /// @brief The content (one item; a second one is not shown)
    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;

    /// @brief The width of the content in a panel @p width wide
    [[nodiscard]] int contentWidth(int width) const;

private:
    QList<QLayoutItem*> m_items;
};

/// @brief The Dashboard's logo beside its title and tagline, or above them
///
/// Beside the texts, each in its line, the logo takes the room they leave, up to its largest
/// size. Where they do not fit beside the smallest logo, the logo goes above them, as small
/// as that, so that the texts stay in sight on a small screen; under it they are centered
/// and wrap where even their own line is too narrow for them.
class DashboardHeaderLayout : public QLayout {
public:
    static constexpr int LOGO_SIZE = 256;           ///< The logo where it has room
    static constexpr int LOGO_MIN_SIZE = 96;        ///< Beside the texts at least, and above them
    static constexpr int LOGO_SPACING = 24;         ///< Between the logo and the texts beside it
    static constexpr int LOGO_SPACING_ABOVE = 12;   ///< Between the logo and the texts under it
    static constexpr int TITLE_SPACING = 8;         ///< Between the title and the tagline

    /// @brief Constructor
    /// @param parent Widget the layout arranges
    explicit DashboardHeaderLayout(QWidget* parent = nullptr);
    ~DashboardHeaderLayout() override;

    DashboardHeaderLayout(const DashboardHeaderLayout&) = delete;
    DashboardHeaderLayout& operator=(const DashboardHeaderLayout&) = delete;

    /// @brief The largest logo (a low panel makes it smaller), between the smallest one and
    ///        its own size
    void setLargestLogo(int size);
    [[nodiscard]] int largestLogo() const;

    /// @brief The logo, the title and the tagline, in this order
    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief The largest logo beside the texts
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief As narrow as the longest word of the texts
    [[nodiscard]] QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief Place the logo and the texts in @p rect, or only measure them
    /// @return The height they take, with the margins
    int arrange(const QRect& rect, bool place) const;

    QList<QLayoutItem*> m_items;     ///< The logo, the title and the tagline
    int m_largestLogo = LOGO_SIZE;
};

/// @brief The hints of the Dashboard's shortcuts: all in one row where it holds them, else one
///        under another, with the keys and the commands in two columns
///
/// A panel too narrow even for the two columns puts each command under its keys, in the
/// middle, and wraps a command longer than the line. Each hint is two items: its keys, then
/// its command (a last item without its command is not arranged).
class DashboardHintsLayout : public QLayout {
public:
    static constexpr int HINT_SPACING = 48;   ///< Between the hints in one row
    static constexpr int KEYS_SPACING = 12;   ///< Between the keys and the command beside them
    static constexpr int ROW_SPACING = 8;     ///< Between the rows of the two columns
    static constexpr int STACK_SPACING = 12;  ///< Between the hints with the command under keys

    /// @brief Constructor
    /// @param parent Widget the layout arranges
    explicit DashboardHintsLayout(QWidget* parent = nullptr);
    ~DashboardHintsLayout() override;

    DashboardHintsLayout(const DashboardHintsLayout&) = delete;
    DashboardHintsLayout& operator=(const DashboardHintsLayout&) = delete;

    /// @brief Add a hint: its keys and the command they run
    void addHint(QWidget* keys, QWidget* command);

    /// @brief The keys and the command of each hint, in turn
    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief All the hints in one row
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief As narrow as the widest keys or the longest word of a command
    [[nodiscard]] QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief Place the hints in @p rect, or only measure them
    /// @return The height they take, with the margins
    int arrange(const QRect& rect, bool place) const;

    QList<QLayoutItem*> m_items;  ///< The keys and the command of each hint, in turn
};

/// @brief A recent book's card: its icon beside its texts, or above them where a narrow card
///        leaves them too little room beside it
///
/// Beside the icon the texts need TEXT_ROOM_BESIDE, or the width of their lines where that is
/// less: in a narrower column they would wrap after each word or two.
class DashboardCardLayout : public QLayout {
public:
    static constexpr int MARGIN = 12;             ///< Around the icon and the texts
    static constexpr int SPACING = 12;            ///< Between the icon and the texts
    static constexpr int TEXT_ROOM_BESIDE = 160;  ///< The texts' least room beside the icon

    /// @brief Constructor
    /// @param parent Widget the layout arranges (the card)
    explicit DashboardCardLayout(QWidget* parent = nullptr);
    ~DashboardCardLayout() override;

    DashboardCardLayout(const DashboardCardLayout&) = delete;
    DashboardCardLayout& operator=(const DashboardCardLayout&) = delete;

    /// @brief The icon, then the texts
    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

    /// @brief The icon beside the texts, which are as wide as they like
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief The icon above the texts, which are as narrow as their longest word
    [[nodiscard]] QSize minimumSize() const override;

    void setGeometry(const QRect& rect) override;

private:
    /// @brief Place the icon and the texts in @p rect, or only measure them
    /// @return The height they take, with the margins
    int arrange(const QRect& rect, bool place) const;

    QList<QLayoutItem*> m_items;  ///< The icon, then the texts
};

} // namespace gui
} // namespace kalahari
