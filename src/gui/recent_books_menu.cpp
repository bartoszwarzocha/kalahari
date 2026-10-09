/// @file recent_books_menu.cpp
/// @brief File > Recent Books submenu implementation

#include "kalahari/gui/recent_books_menu.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/gui/command_registry.h"

#include <QAction>
#include <QFileInfo>

namespace kalahari {
namespace gui {

RecentBooksMenu::RecentBooksMenu(QWidget* parent)
    : QMenu(tr("Recent Books"), parent)
{
    setIcon(core::ArtProvider::getInstance().getIcon("file.open"));
    setToolTipsVisible(true);

    connect(&core::RecentBooksManager::getInstance(),
            &core::RecentBooksManager::recentFilesChanged, this, &RecentBooksMenu::rebuild);
    rebuild();
}

void RecentBooksMenu::insertInto(QMenu* menu) {
    if (menu == nullptr) {
        return;
    }
    // The command's shared action, not its text: the text is translated
    QAction* closeBook = CommandRegistry::getInstance().getAction(std::string("file.close"));
    if (closeBook != nullptr && menu->actions().contains(closeBook)) {
        menu->insertMenu(closeBook, this);
    } else {
        menu->addMenu(this);
    }
}

void RecentBooksMenu::rebuild() {
    clear();

    const QStringList files = core::RecentBooksManager::getInstance().getRecentFiles();
    if (files.isEmpty()) {
        QAction* emptyAction = addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
        return;
    }

    // Numbered for the keyboard: 1-9, then 0 for the 10th
    for (int i = 0; i < files.size(); ++i) {
        const QString& filePath = files[i];
        const QString name = QFileInfo(filePath).completeBaseName();
        const QString text = i < 9 ? QString("&%1. %2").arg(i + 1).arg(name)
                                   : QString("1&0. %1").arg(name);
        QAction* action = addAction(text);
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, [this, filePath]() {
            emit bookChosen(filePath);
        });
    }

    addSeparator();
    QAction* clearAction = addAction(tr("Clear Recent Files"));
    connect(clearAction, &QAction::triggered, this, []() {
        core::RecentBooksManager::getInstance().clearRecentFiles();
    });
}

} // namespace gui
} // namespace kalahari
