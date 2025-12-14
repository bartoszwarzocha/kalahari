/// @file recent_book_card.h
/// @brief Recent book card widget for Dashboard
///
/// OpenSpec #00036: Enhanced Dashboard with recent books cards

#pragma once

#include <QFrame>
#include <QString>

class QLabel;

namespace kalahari {
namespace gui {

/// @brief Card widget displaying a single recent book entry
///
/// Displays book information in a horizontal layout:
/// - Icon (48x48) on the left
/// - Title, stats, and date on the right
///
/// Features:
/// - Hover effect (highlight background)
/// - Click to open the book
///
/// Example usage:
/// @code
/// RecentBookCard* card = new RecentBookCard("/path/to/book.klh", this);
/// connect(card, &RecentBookCard::clicked, this, &MyWidget::onCardClicked);
/// @endcode
class RecentBookCard : public QFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param filePath Full path to the .klh file
    /// @param parent Parent widget
    explicit RecentBookCard(const QString& filePath, QWidget* parent = nullptr);

    /// @brief Get the file path associated with this card
    /// @return Full path to the .klh file
    QString filePath() const { return m_filePath; }

signals:
    /// @brief Emitted when the card is clicked
    /// @param filePath Path to the file to open
    void clicked(const QString& filePath);

protected:
    /// @brief Handle mouse press event
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Handle mouse enter event (hover start)
    void enterEvent(QEnterEvent* event) override;

    /// @brief Handle mouse leave event (hover end)
    void leaveEvent(QEvent* event) override;

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Extract project title from file path
    /// @return Project title (filename without extension)
    QString extractTitle() const;

    /// @brief Extract stats from the project (placeholder for now)
    /// @return Stats string like "12 chapters, 45,000 words"
    QString extractStats() const;

    /// @brief Format last modified date
    /// @return Formatted date string
    QString formatDate() const;

    /// @brief Update card appearance based on hover state
    /// @param hovered true if mouse is over the card
    void updateHoverState(bool hovered);

    QString m_filePath;       ///< Full path to the .klh file
    QLabel* m_iconLabel;      ///< Icon label (48x48)
    QLabel* m_titleLabel;     ///< Title label (bold, larger)
    QLabel* m_statsLabel;     ///< Stats label
    QLabel* m_dateLabel;      ///< Date label
};

} // namespace gui
} // namespace kalahari
