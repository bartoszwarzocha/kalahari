// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file standalone_info_bar.h
/// @brief Info bar widget for standalone files (not part of the book), for a book open
/// read-only, and for a notice of what was just done to a text
///
/// OpenSpec #00033 Phase F: StandaloneInfoBar widget displays an informational
/// banner when editing a standalone file, with an option to add it to the open book. Over a
/// book open read-only it says why, with the export of the book's archive. Under a text it
/// can say what was just done to it, with a button that undoes it (showNotice()).

#ifndef KALAHARI_GUI_WIDGETS_STANDALONE_INFO_BAR_H
#define KALAHARI_GUI_WIDGETS_STANDALONE_INFO_BAR_H

#include "kalahari/gui/widgets/info_bar.h"

#include <QString>

namespace kalahari::gui {

/// @brief Info bar displayed for standalone files not part of the book, for a book open
/// read-only (showReadOnlyBook()), and for a notice under a text (showNotice())
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
    /// The colors of the bar, in the theme's "editor" section (Settings > Editor > Colors, Bar
    /// Under the Text): it stands out from the text, its button too
    static constexpr const char* BACKGROUND_COLOR_KEY = "noticeBarBackground";
    static constexpr const char* BORDER_COLOR_KEY = "noticeBarBorder";
    static constexpr const char* TEXT_COLOR_KEY = "noticeBarText";
    static constexpr const char* BUTTON_COLOR_KEY = "noticeBarButton";
    static constexpr const char* BUTTON_TEXT_COLOR_KEY = "noticeBarButtonText";

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
    [[nodiscard]] QString filePath() const { return m_filePath; }

    /// @brief Whether a book is open: without one the file can go into none, so the button is
    /// greyed out and its tooltip says why; nor into a book open read-only (@p readOnly). A new
    /// file (setFilePath() with an empty path) is then saved on the disk: the button says
    /// "Save As..." (saveAsClicked())
    void setBookOpen(bool open, bool readOnly = false);

    /// @brief Say why the open book is read-only, @p message, with a button that exports its
    /// archive (exportArchiveClicked()), in place of a file; setFilePath() shows a file again
    void showReadOnlyBook(const QString& message);

    /// @brief Whether it says that the open book is read-only (showReadOnlyBook()), not
    /// something of a file
    [[nodiscard]] bool showsReadOnlyBook() const { return m_readOnlyBook; }

    /// @brief Say @p message of what was just done to the text in front, with a button
    /// @p actionText (noticeActionClicked()) and a second one @p secondText
    /// (noticeSecondActionClicked()), in place of a file; setFilePath() and showReadOnlyBook()
    /// show what they show again
    /// @param message The notice
    /// @param actionText Its button, with a mnemonic; empty: none
    /// @param actionToolTip The tooltip of its button
    /// @param secondText Its second button, with a mnemonic; empty: none
    /// @param secondToolTip The tooltip of its second button
    void showNotice(const QString& message, const QString& actionText,
                    const QString& actionToolTip = QString(),
                    const QString& secondText = QString(),
                    const QString& secondToolTip = QString());

    /// @brief Whether it shows a notice (showNotice())
    [[nodiscard]] bool showsNotice() const { return m_notice; }

signals:
    /// @brief Emitted when the "Add to Book" button is clicked
    void addToProjectClicked();

    /// @brief Emitted when the button that exports the archive of the book open read-only is
    /// clicked (showReadOnlyBook())
    void exportArchiveClicked();

    /// @brief Emitted when the button of a new file that no book can take, "Save As...", is
    /// clicked (setBookOpen())
    void saveAsClicked();

    /// @brief Emitted when the button of a notice is clicked (showNotice())
    void noticeActionClicked();

    /// @brief Emitted when the second button of a notice is clicked (showNotice())
    void noticeSecondActionClicked();

private:
    /// Whether the button saves a new file that no book can take ("Save As...")
    [[nodiscard]] bool savesAs() const;

    /// The text, the state and the tooltip of the button for the file and the book
    void updateAction();

    QString m_filePath;         ///< Path to the standalone file
    bool m_readOnlyBook = false;  ///< It says that the open book is read-only
    bool m_notice = false;        ///< It shows a notice (showNotice())
    bool m_bookOpen = true;       ///< A book is open (setBookOpen())
    bool m_bookReadOnly = false;  ///< The open book is read-only (setBookOpen())
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_STANDALONE_INFO_BAR_H
