/// @file recent_books_menu.cpp
/// @brief File > Recent Books submenu implementation

#include "kalahari/gui/recent_books_menu.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/utils/access_keys.h"

#include <QAction>
#include <QFileInfo>

namespace kalahari {
namespace gui {

RecentBooksMenu::RecentBooksMenu(QWidget* parent)
    : QMenu(MenuBuilder::menuText("Recent Books"), parent)
{
    // In the icon colors after they change, as the other submenus
    MenuBuilder::showSubmenuIcon(this, MenuBuilder::submenuIconId("Recent Books"));
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
        // A line that only tells: disabled, without an access key
        QAction* emptyAction = addAction(tr("No Recent Books"));
        emptyAction->setEnabled(false);
        return;
    }

    // Numbered for the keyboard: 1-9, then 0 for the 10th (an ampersand in a name is shown
    // as it is)
    for (int i = 0; i < files.size(); ++i) {
        const QString& filePath = files[i];
        const QString name = literalMenuText(QFileInfo(filePath).completeBaseName());
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
    QAction* clearAction = addAction(tr("&Clear Recent Books"));
    connect(clearAction, &QAction::triggered, this, []() {
        core::RecentBooksManager::getInstance().clearRecentFiles();
    });
}

} // namespace gui
} // namespace kalahari
