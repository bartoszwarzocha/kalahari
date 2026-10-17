/// @file standalone_info_bar.cpp
/// @brief Implementation of StandaloneInfoBar widget
///
/// OpenSpec #00033 Phase F: Info bar for standalone files

#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/gui/read_only_book.h"

namespace kalahari {
namespace gui {

StandaloneInfoBar::StandaloneInfoBar(QWidget* parent)
    : InfoBar(parent)
{
    setMessage(tr("This file is not part of a book. Some features are not available."));
    setFilePath(QString());
    setBookOpen(true);
    connect(this, &InfoBar::actionClicked, this, [this]() {
        if (m_readOnlyBook) {
            emit exportArchiveClicked();
        } else {
            emit addToProjectClicked();
        }
    });
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
    m_readOnlyBook = false;
    // A new file goes into the book at once; a file on the disk in the Add to Book window
    setActionText(path.isEmpty() ? tr("Add to Book") : tr("Add to Book..."));
}

void StandaloneInfoBar::setBookOpen(bool open, bool readOnly) {
    setActionEnabled(open && !readOnly);
    if (!open) {
        setActionToolTip(tr("No book is open"));
    } else if (readOnly) {
        setActionToolTip(readOnlyReason());
    } else {
        setActionToolTip(tr("Add the file to the open book as a new element"));
    }
}

void StandaloneInfoBar::showReadOnlyBook(const QString& message) {
    m_filePath.clear();
    m_readOnlyBook = true;
    setMessage(message);
    //: The button of the message at the bottom of the window over a book open read-only, as
    //: File > Export > Project Archive... In Polish: Eksportuj archiwum projektu...
    setActionText(tr("Export Project Archive..."));
    setActionEnabled(true);
    //: In Polish: Zapisz książkę w archiwum projektu, które można otworzyć w programie mającym
    //: jej typ
    setActionToolTip(tr("Save the book in a project archive, which a program that has its type "
                        "can open"));
}

} // namespace gui
} // namespace kalahari
