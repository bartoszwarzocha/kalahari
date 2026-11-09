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
        if (m_notice) {
            emit noticeActionClicked();
        } else if (m_readOnlyBook) {
            emit exportArchiveClicked();
        } else if (savesAs()) {
            emit saveAsClicked();
        } else {
            emit addToProjectClicked();
        }
    });
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
    m_readOnlyBook = false;
    m_notice = false;
    updateAction();
}

void StandaloneInfoBar::setBookOpen(bool open, bool readOnly) {
    m_bookOpen = open;
    m_bookReadOnly = readOnly;
    if (!m_readOnlyBook && !m_notice) {
        updateAction();
    }
}

bool StandaloneInfoBar::savesAs() const {
    return !m_readOnlyBook && !m_notice && m_filePath.isEmpty() &&
           (!m_bookOpen || m_bookReadOnly);
}

void StandaloneInfoBar::updateAction() {
    // A new file that no book can take is saved as a file of its own
    if (savesAs()) {
        //: The button of the bar at the bottom of a new file while no book can take it, as
        //: File > Save As... In Polish: Zapisz jako...
        setActionText(tr("Save As..."));
        setActionEnabled(true);
        //: In Polish: Zapisz nowy plik na dysku
        setActionToolTip(tr("Save the new file on the disk"));
        return;
    }

    // A new file goes into the book at once; a file on the disk in the Add to Book window
    setActionText(m_filePath.isEmpty() ? tr("Add to Book") : tr("Add to Book..."));
    setActionEnabled(m_bookOpen && !m_bookReadOnly);
    if (!m_bookOpen) {
        setActionToolTip(tr("No book is open"));
    } else if (m_bookReadOnly) {
        setActionToolTip(readOnlyReason());
    } else {
        setActionToolTip(tr("Add the file to the open book as a new element"));
    }
}

void StandaloneInfoBar::showReadOnlyBook(const QString& message) {
    m_filePath.clear();
    m_readOnlyBook = true;
    m_notice = false;
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

void StandaloneInfoBar::showNotice(const QString& message, const QString& actionText,
                                   const QString& actionToolTip) {
    m_filePath.clear();
    m_readOnlyBook = false;
    m_notice = true;
    setMessage(message);
    setActionText(actionText);
    setActionEnabled(true);
    setActionToolTip(actionToolTip);
}

} // namespace gui
} // namespace kalahari
