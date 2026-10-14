/// @file file_preview_dialog.h
/// @brief Preview of a chapter file that no element of the book has: its text, read-only,
/// and what can be done with the file

#pragma once

#include "kalahari/core/chapter_document.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QPushButton;
class QTextBrowser;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Preview of a file of "Unassigned Files" in the Navigator
///
/// The writer reads the text of the file before adding it back to the book or deleting it:
/// the heading has its title, its file, its status and the number of its words. The window
/// changes nothing; "Add to Book..." and "Delete File..." close it, and the program goes on
/// with what the writer chose. Close is the default button.
class FilePreviewDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the writer chose
    enum class Choice {
        Close,      ///< Nothing
        AddToBook,  ///< Add the file to the book
        Delete,     ///< Delete the file
    };

    /// @brief Create the window
    /// @param chapter The file, read
    /// @param path Absolute path of the file
    /// @param projectFolder Folder of the project: the window names the file relative to it
    /// @param parent Parent widget
    FilePreviewDialog(const core::ChapterDocument& chapter, const QString& path,
                      const QString& projectFolder, QWidget* parent = nullptr);

    /// @brief What the writer chose; Close while the window is open
    Choice choice() const { return m_choice; }

    /// @brief The text of the file
    QTextBrowser* text() const { return m_text; }

    /// @brief "Delete File..."
    QPushButton* deleteButton() const { return m_deleteButton; }

private:
    Choice m_choice = Choice::Close;
    QTextBrowser* m_text = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
