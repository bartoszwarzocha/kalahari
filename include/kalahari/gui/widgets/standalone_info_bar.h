/// @file standalone_info_bar.h
/// @brief Info bar widget for standalone files (not part of a project)
///
/// OpenSpec #00033 Phase F: StandaloneInfoBar widget displays an informational
/// banner when editing a standalone file, with an option to add it to a project.

#pragma once

#include "kalahari/gui/widgets/info_bar.h"

#include <QString>

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
class StandaloneInfoBar : public InfoBar {
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

signals:
    /// @brief Emitted when "Add to Project" button is clicked
    void addToProjectClicked();

private:
    QString m_filePath;         ///< Path to the standalone file
};

} // namespace gui
} // namespace kalahari
