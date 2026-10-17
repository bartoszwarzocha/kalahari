/// @file standalone_info_bar.h
/// @brief Info bar widget for standalone files (not part of the book), and for a book open
/// read-only
///
/// OpenSpec #00033 Phase F: StandaloneInfoBar widget displays an informational
/// banner when editing a standalone file, with an option to add it to the open book. Over a
/// book open read-only it says why, with the export of the book's archive.

#pragma once

#include "kalahari/gui/widgets/info_bar.h"

#include <QString>

namespace kalahari {
namespace gui {

/// @brief Info bar displayed for standalone files not part of the book, and for a book open
/// read-only (showReadOnlyBook())
///
/// Shows a themed banner with:
/// - Info icon
/// - Message about limited features
/// - "Add to Book..." button
/// - Close/dismiss button
///
/// Visual Design:
/// +-------------------------------------------------------------------+
/// | [i] This file is not part of a book.        [Add to Book...] [X]  |
/// |     Some features are not available.                               |
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

    /// @brief Set the file path being displayed; its button says "Add to Book" for a new file,
    /// which goes into the book at once, and "Add to Book..." for a file on the disk
    ///
    /// The bar shows a file again after showReadOnlyBook().
    /// @param path Path to the standalone file; empty for a new file that has no file yet
    void setFilePath(const QString& path);

    /// @brief Get the current file path
    /// @return Current file path
    QString filePath() const { return m_filePath; }

    /// @brief Whether a book is open: without one the file can go into none, so the button is
    /// greyed out and its tooltip says why; nor into a book open read-only (@p readOnly)
    void setBookOpen(bool open, bool readOnly = false);

    /// @brief Say why the open book is read-only, @p message, with a button that exports its
    /// archive (exportArchiveClicked()), in place of a file; setFilePath() shows a file again
    void showReadOnlyBook(const QString& message);

    /// @brief Whether it says that the open book is read-only (showReadOnlyBook()), not
    /// something of a file
    bool showsReadOnlyBook() const { return m_readOnlyBook; }

signals:
    /// @brief Emitted when the "Add to Book" button is clicked
    void addToProjectClicked();

    /// @brief Emitted when the button that exports the archive of the book open read-only is
    /// clicked (showReadOnlyBook())
    void exportArchiveClicked();

private:
    QString m_filePath;         ///< Path to the standalone file
    bool m_readOnlyBook = false;  ///< It says that the open book is read-only
};

} // namespace gui
} // namespace kalahari
