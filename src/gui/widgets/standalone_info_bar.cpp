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
    setActionText(tr("Add to Book..."));
    setActionToolTip(tr("Add the file to the open book as a new element"));
    connect(this, &InfoBar::actionClicked, this, &StandaloneInfoBar::addToProjectClicked);
}

void StandaloneInfoBar::setFilePath(const QString& path) {
    m_filePath = path;
}

} // namespace gui
} // namespace kalahari
