// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file navigator_panel_series.cpp
/// @brief NavigatorPanel: the books of the project, one book or the volumes of a series

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QFont>
#include <QKeyCombination>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <optional>

namespace kalahari::gui {

namespace {

/// The type of the node of a book (Qt::UserRole + 1)
constexpr QLatin1String BOOK_TYPE("book");
/// The role of the type of a node
constexpr int TYPE_ROLE = Qt::UserRole + 1;

/// @brief The step of the keys of a volume: -1 Alt+Up (earlier), 1 Alt+Down (later), 0 Delete;
/// none for other keys
std::optional<int> volumeStepOf(QKeyCombination keys) {
    if (keys == QKeyCombination(Qt::AltModifier, Qt::Key_Up)) {
        return -1;
    }
    if (keys == QKeyCombination(Qt::AltModifier, Qt::Key_Down)) {
        return 1;
    }
    if (keys == QKeyCombination(Qt::Key_Delete)) {
        return 0;
    }
    return std::nullopt;
}

}  // namespace

QString NavigatorPanel::volumeName(const core::ProjectBook& book, qsizetype index) {
    const QString number = QLocale().toString(index + 1);
    if (book.title.trimmed().isEmpty()) {
        //: The node of a volume of a series without a title in the Navigator; %1: its number.
        //: In Polish: Tom %1
        return tr("Volume %1").arg(number);
    }
    //: The node of a volume of a series in the Navigator: %1 its number, %2 its title. In
    //: Polish: Tom %1: %2
    return tr("Volume %1: %2").arg(number, book.title);
}

QString NavigatorPanel::seriesName(const core::BookProject& project) {
    if (!project.series.isEmpty() || project.books.isEmpty()) {
        return project.series;
    }
    return project.books.constFirst().title;
}

QString NavigatorPanel::bookIdOf(const QTreeWidgetItem* item) {
    for (const QTreeWidgetItem* node = item; node != nullptr; node = node->parent()) {
        if (node->data(0, TYPE_ROLE).toString() == BOOK_TYPE) {
            return node->data(0, BOOK_ROLE).toString();
        }
    }
    return {};
}

qsizetype NavigatorPanel::dropBookOf(const core::BookProject& project, const QString& elementId,
                                     const QTreeWidgetItem* target) {
    const qsizetype book = project.bookIndexOf(elementId);
    const QString targetBook = bookIdOf(target);
    if (book < 0 || targetBook.isEmpty() || targetBook == project.books.at(book).id) {
        return book;
    }
    return -1;
}

void NavigatorPanel::requestBookProperties(const QString& type) {
    if (type == QLatin1String("document") && showsVolumes()) {
        emit requestSeriesProperties();
    } else {
        emit requestProperties(QString());  // the properties of the book
    }
}

QTreeWidgetItem* NavigatorPanel::bookItem(const QString& bookId) const {
    QTreeWidgetItem* root =
        m_treeWidget->topLevelItemCount() > 0 ? m_treeWidget->topLevelItem(0) : nullptr;
    for (int i = 0; root != nullptr && i < root->childCount(); ++i) {
        QTreeWidgetItem* child = root->child(i);
        if (child->data(0, TYPE_ROLE).toString() == BOOK_TYPE &&
            child->data(0, BOOK_ROLE).toString() == bookId) {
            return child;
        }
    }
    return nullptr;
}

void NavigatorPanel::showCurrentBook() {
    const core::ProjectBook* current = m_book.book();
    const QString currentId = current != nullptr && m_book.isSeries() ? current->id : QString();
    QTreeWidgetItem* root =
        m_treeWidget->topLevelItemCount() > 0 ? m_treeWidget->topLevelItem(0) : nullptr;
    for (int i = 0; root != nullptr && i < root->childCount(); ++i) {
        QTreeWidgetItem* child = root->child(i);
        if (child->data(0, TYPE_ROLE).toString() != BOOK_TYPE) {
            continue;
        }
        QFont font = child->font(0);
        font.setBold(!currentId.isEmpty() && child->data(0, BOOK_ROLE).toString() == currentId);
        child->setFont(0, font);
    }
}

void NavigatorPanel::chooseBookOf(const QTreeWidgetItem* item) {
    if (const QString bookId = bookIdOf(item); !bookId.isEmpty()) {
        m_book.setCurrentBook(bookId);
    }
}

QTreeWidgetItem* NavigatorPanel::addBookItem(QTreeWidgetItem* rootItem,
                                             const core::ProjectBook* book, qsizetype index,
                                             const core::BookTypeRegistry& registry) {
    // The book, which the writer can name ("Book"), or a volume of a series ("Volume 1: Sand")
    const core::ProjectBook noBook;
    const core::ProjectBook& names = book != nullptr ? *book : noBook;
    auto* bookItem = new QTreeWidgetItem(rootItem);
    bookItem->setText(0, index < 0 ? bookName(names) : volumeName(names, index));
    bookItem->setData(0, Qt::UserRole, QString());
    bookItem->setData(0, TYPE_ROLE, BOOK_TYPE);
    bookItem->setData(0, BOOK_ROLE, names.id);
    bookItem->setIcon(0, core::ArtProvider::getInstance().getIcon(getIconIdForType(BOOK_TYPE),
                                                                 core::IconContext::TreeView));
    bookItem->setFlags(bookItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    if (book != nullptr && !book->partsLayer) {
        // A book without sections: the elements of its three sections in reading order. The
        // book's item takes the drops of the elements.
        bookItem->setFlags(bookItem->flags() | Qt::ItemIsDropEnabled);
        addElementItems(bookItem, book->frontElements, registry);
        addElementItems(bookItem, book->mainElements, registry);
        addElementItems(bookItem, book->backElements, registry);
    } else {
        // The three sections of the book with its names, also when empty, so that elements can
        // be added to them. Elements are dragged to the sections and the groups, which
        // therefore take drops.
        QTreeWidgetItem* frontMatterItem =
            addSectionItem(bookItem, names.sectionName(core::BookPlace::Front),
                           QStringLiteral("section_frontmatter"));
        QTreeWidgetItem* bodyItem = addSectionItem(
            bookItem, names.sectionName(core::BookPlace::Main), QStringLiteral("section_body"));
        QTreeWidgetItem* backMatterItem =
            addSectionItem(bookItem, names.sectionName(core::BookPlace::Back),
                           QStringLiteral("section_backmatter"));
        if (book != nullptr) {
            addElementItems(frontMatterItem, book->frontElements, registry);
            addElementItems(bodyItem, book->mainElements, registry);
            addElementItems(backMatterItem, book->backElements, registry);
        }
        frontMatterItem->setExpanded(false);  // Collapsed by default
        bodyItem->setExpanded(true);          // Expand body by default
        backMatterItem->setExpanded(false);   // Collapsed by default
    }

    // The files of the book outside its text, after its text
    if (book != nullptr) {
        addBookFileItems(bookItem, book->files);
    }
    bookItem->setExpanded(true);
    return bookItem;
}

bool NavigatorPanel::showsVolumes() const {
    const QTreeWidgetItem* root =
        m_treeWidget->topLevelItemCount() > 0 ? m_treeWidget->topLevelItem(0) : nullptr;
    int books = 0;
    for (int i = 0; root != nullptr && i < root->childCount(); ++i) {
        if (root->child(i)->data(0, TYPE_ROLE).toString() == BOOK_TYPE) {
            ++books;
        }
    }
    return books > 1;
}

QString NavigatorPanel::bookNodeKey(const QTreeWidgetItem* item) const {
    const QString bookId = bookIdOf(item);
    if (bookId.isEmpty() || !showsVolumes()) {
        return {};
    }
    return QStringLiteral("book:%1:%2").arg(bookId, item->data(0, TYPE_ROLE).toString());
}

QTreeWidgetItem* NavigatorPanel::itemOfBookNodeKey(const QString& key) const {
    const QStringList parts = key.split(QLatin1Char(':'));
    if (parts.size() != 3 || parts.constFirst() != QLatin1String("book")) {
        return nullptr;
    }
    QTreeWidgetItem* book = bookItem(parts.at(1));
    if (book == nullptr || parts.at(2) == BOOK_TYPE) {
        return book;
    }
    return findItemByTypeAndTextRecursive(book, parts.at(2), QString());
}

QAction* NavigatorPanel::addNewVolumeAction(QMenu& menu) const {
    QAction* action = menu.addAction(
        core::ArtProvider::getInstance().getIcon("book.newVolume", core::IconContext::Menu),
        //: The command of the series and of its volumes in the Navigator, as Book > New
        //: Volume... In Polish: N&owy tom...
        tr("New &Volume..."));
    connect(action, &QAction::triggered, this, &NavigatorPanel::requestNewVolume);
    return action;
}

QList<QAction*> NavigatorPanel::addVolumeActions(QMenu& menu, const QTreeWidgetItem* item) {
    auto& artProvider = core::ArtProvider::getInstance();
    const QString bookId = bookIdOf(item);
    const core::BookProject* project = m_book.project();
    const qsizetype index = project != nullptr ? project->bookIndex(bookId) : -1;
    const qsizetype count = project != nullptr ? project->books.size() : 0;

    QList<QAction*> actions{addNewVolumeAction(menu)};
    QAction* earlier = menu.addAction(
        artProvider.getIcon("navigation.up", core::IconContext::Menu),
        //: The command of a volume in the Navigator: it comes before the volume before it.
        //: In Polish: Tom wcześ&niej
        menuText(tr("Volume Ear&lier"), QKeyCombination(Qt::AltModifier, Qt::Key_Up)));
    earlier->setEnabled(index > 0);
    connect(earlier, &QAction::triggered, this,
            [this, bookId]() { emit requestMoveVolume(bookId, -1); });
    QAction* later = menu.addAction(
        artProvider.getIcon("navigation.down", core::IconContext::Menu),
        //: The command of a volume in the Navigator: it comes after the volume after it. In
        //: Polish: Tom &później
        menuText(tr("Volume La&ter"), QKeyCombination(Qt::AltModifier, Qt::Key_Down)));
    later->setEnabled(index >= 0 && index < count - 1);
    connect(later, &QAction::triggered, this,
            [this, bookId]() { emit requestMoveVolume(bookId, 1); });
    actions << earlier << later;
    return actions;
}

QAction* NavigatorPanel::addDeleteVolumeAction(QMenu& menu, const QTreeWidgetItem* item) {
    const QString bookId = bookIdOf(item);
    QAction* action = menu.addAction(
        core::ArtProvider::getInstance().getIcon("edit.delete", core::IconContext::Menu),
        //: The command of a volume in the Navigator. In Polish: &Usuń tom...
        menuText(tr("&Delete Volume..."), QKeyCombination(Qt::Key_Delete)));
    connect(action, &QAction::triggered, this,
            [this, bookId]() { emit requestDeleteVolume(bookId); });
    return action;
}

QList<QAction*> NavigatorPanel::addBookActions(QMenu& menu, const QTreeWidgetItem* item) {
    if (showsVolumes()) {
        return addVolumeActions(menu, item);
    }
    QList<QAction*> actions{
        addRenameAction(menu, [this]() { emit requestRenameNode(QStringLiteral("book")); })};
    // A book becomes volume 1 of a series with its second volume
    if (m_book.isProjectOpen()) {
        actions << addNewVolumeAction(menu);
    }
    return actions;
}

QString NavigatorPanel::bookPropertiesText() const {
    if (showsVolumes()) {
        //: The command of a volume of a series in the Navigator. In Polish: &Właściwości
        //: tomu...
        return tr("Volume P&roperties...");
    }
    return tr("Book P&roperties...");
}

QList<QAction*> NavigatorPanel::addVolumeRemovalActions(QMenu& menu,
                                                        const QTreeWidgetItem* item) {
    if (!showsVolumes()) {
        return {};
    }
    return {addDeleteVolumeAction(menu, item)};
}

void NavigatorPanel::addProjectActions(QMenu& menu, QList<QAction*>& changing) {
    auto& artProvider = core::ArtProvider::getInstance();
    if (!showsVolumes()) {
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("view.properties", core::IconContext::Menu),
            //: The command of the project's node in the Navigator. In Polish: Właściwości
            //: &projektu...
            tr("Project P&roperties..."));
        propertiesAction->setEnabled(false);
        //: Why Project Properties... is grey. In Polish: Właściwości projektu (w
        //: przygotowaniu)
        const QString why = tr("Project Properties (in preparation)");
        propertiesAction->setToolTip(why);
        propertiesAction->setStatusTip(why);
        return;
    }
    changing << addNewVolumeAction(menu);
    QAction* properties = menu.addAction(
        artProvider.getIcon("view.properties", core::IconContext::Menu),
        //: The command of the node of a series in the Navigator: its name and its volumes in
        //: the Properties panel. In Polish: &Właściwości serii...
        menuText(tr("Series P&roperties..."),
                 QKeyCombination(Qt::AltModifier, Qt::Key_Return)));
    connect(properties, &QAction::triggered, this, [this]() {
        emit requestSeriesProperties();
        emit requestPropertiesPanel();
    });
}

bool NavigatorPanel::runVolumeKey(QKeyCombination keys, const QTreeWidgetItem* item,
                                  bool run) {
    if (item == nullptr || item->data(0, TYPE_ROLE).toString() != BOOK_TYPE ||
        !showsVolumes()) {
        return false;
    }
    const std::optional<int> step = volumeStepOf(keys);
    if (!step.has_value()) {
        return false;
    }
    if (run && changeRefusal().isEmpty()) {
        const QString bookId = bookIdOf(item);
        if (*step == 0) {
            emit requestDeleteVolume(bookId);
        } else {
            emit requestMoveVolume(bookId, *step);
        }
    }
    return true;
}

void NavigatorPanel::revealBook(const QString& bookId) {
    if (QTreeWidgetItem* item = bookItem(bookId)) {
        item->setExpanded(true);
        m_treeWidget->setCurrentItem(item);
        m_treeWidget->scrollToItem(item, QAbstractItemView::EnsureVisible);
    }
}

QString NavigatorPanel::menuText(const QString& text, QKeyCombination keys) {
    if (QCoreApplication::testAttribute(Qt::AA_DontShowShortcutsInContextMenus)) {
        return text;
    }
    return text + QLatin1Char('\t') + ShortcutRules::keysText(keys);
}

}  // namespace kalahari::gui
