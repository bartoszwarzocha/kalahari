/// @file standalone_info_bar.h
/// @brief Info bar widget for standalone files (not part of a project)
///
/// OpenSpec #00033 Phase F: StandaloneInfoBar widget displays an informational
/// banner when editing a standalone file, with an option to add it to a project.

#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QToolButton;

namespace kalahari {
namespace gui {

/// @brief Info bar displayed for standalone files not part of a project
///
/// Shows a themed banner with:
/// - Info icon
/// - Message about limited features
/// - "Add to Project" button
/// - Close/dismiss button
///
/// Visual Design:
/// +-------------------------------------------------------------------+
/// | [i] This file is not part of a project.  [Add to Project] [X]     |
/// |     Limited features available.                                    |
/// +-------------------------------------------------------------------+
///
/// Usage:
/// @code
/// StandaloneInfoBar* infoBar = new StandaloneInfoBar(this);
/// infoBar->setFilePath("/path/to/file.txt");
/// layout->addWidget(infoBar);
///
/// connect(infoBar, &StandaloneInfoBar::addToProjectClicked,
///         this, &MyClass::onAddToProject);
/// connect(infoBar, &StandaloneInfoBar::dismissed,
///         infoBar, &QWidget::hide);
/// @endcode
class StandaloneInfoBar : public QFrame {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit StandaloneInfoBar(QWidget* parent = nullptr);

    /// @brief Destructor
    ~StandaloneInfoBar() override = default;

    /// @brief Set the file path being displayed
    /// @param path Path to the standalone file
    void setFilePath(const QString& path);

    /// @brief Get the current file path
    /// @return Current file path
    QString filePath() const { return m_filePath; }

    /// @brief Set custom message text
    /// @param message Custom message (default: "This file is not part of a project.")
    void setMessage(const QString& message);

signals:
    /// @brief Emitted when "Add to Project" button is clicked
    void addToProjectClicked();

    /// @brief Emitted when the bar is dismissed (close button clicked)
    void dismissed();

private slots:
    /// @brief Handle dismiss button click
    void onDismiss();

    /// @brief Handle theme changes
    void onThemeChanged();

private:
    /// @brief Setup the UI layout and widgets
    void setupUI();

    /// @brief Create signal/slot connections
    void createConnections();

    /// @brief Update styling based on current theme
    void updateStyling();

    /// @brief Update icons based on current theme
    void updateIcons();

    QLabel* m_iconLabel;        ///< Info icon label
    QLabel* m_messageLabel;     ///< Main message label
    QPushButton* m_addButton;   ///< "Add to Project" button
    QToolButton* m_closeButton; ///< Close/dismiss button
    QString m_filePath;         ///< Path to the standalone file
};

} // namespace gui
} // namespace kalahari
