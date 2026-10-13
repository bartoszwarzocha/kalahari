/// @file standalone_info_bar.cpp
/// @brief Implementation of StandaloneInfoBar widget
///
/// OpenSpec #00033 Phase F: Info bar for standalone files

#include "kalahari/gui/widgets/standalone_info_bar.h"

namespace kalahari {
namespace gui {

StandaloneInfoBar::StandaloneInfoBar(QWidget* parent)
    : InfoBar(parent)
{
    setMessage(tr("This file is not part of a book. Some features are not available."));
    setFilePath(QString());
    setBookOpen(true);
    connect(this, &InfoBar::actionClicked, this, &StandaloneInfoBar::addToProjectClicked);
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
    // A new file goes into the book at once; a file on the disk in the Add to Book window
    setActionText(path.isEmpty() ? tr("Add to Book") : tr("Add to Book..."));
}

void StandaloneInfoBar::setBookOpen(bool open) {
    setActionEnabled(open);
    setActionToolTip(open ? tr("Add the file to the open book as a new element")
                          : tr("No book is open"));
}

} // namespace gui
} // namespace kalahari
