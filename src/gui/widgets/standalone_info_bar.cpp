// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file standalone_info_bar.cpp
/// @brief Implementation of StandaloneInfoBar widget
///
/// OpenSpec #00033 Phase F: Info bar for standalone files

#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/gui/read_only_book.h"

namespace kalahari::gui {

StandaloneInfoBar::StandaloneInfoBar(QWidget* parent)
    : InfoBar(parent)
{
    // Its own colors, which stand out from the text and from the other bars; the gear opens
    // them in the Settings (settingsClicked())
    setColorKeys(BACKGROUND_COLOR_KEY, BORDER_COLOR_KEY);
    setEmphasisColorKeys(TEXT_COLOR_KEY, BUTTON_COLOR_KEY, BUTTON_TEXT_COLOR_KEY);
    //: The gear of the bar at the bottom of the text, with Undo after a command. In Polish:
    //: Kolory paska
    showSettingsButton(tr("Colors of the Bar"));
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
    // Only a notice has a second button
    connect(this, &InfoBar::secondActionClicked, this, [this]() {
        if (m_notice) {
            emit noticeSecondActionClicked();
        }
    });
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
    m_readOnlyBook = false;
    m_notice = false;
    setSecondActionText(QString());
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
    setSecondActionText(QString());
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
                                   const QString& actionToolTip, const QString& secondText,
                                   const QString& secondToolTip) {
    m_filePath.clear();
    m_readOnlyBook = false;
    m_notice = true;
    setMessage(message);
    setActionText(actionText);
    setActionEnabled(true);
    setActionToolTip(actionToolTip);
    setSecondActionText(secondText);
    setSecondActionToolTip(secondToolTip);
}

} // namespace kalahari::gui
