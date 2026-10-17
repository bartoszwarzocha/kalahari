/// @file test_read_only_book.cpp
/// @brief A book whose type the program does not have, or not whole, open read-only in the
/// window: the bar at the bottom says why and exports its archive, its chapters are read and
/// not changed, the Navigator and the Properties panel change nothing, and a new file stays
/// outside it

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/read_only_book.h>
#include <kalahari/gui/widgets/standalone_info_bar.h>

#include "context_menu_reader.h"
#include "document_window.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <cstdio>

using namespace kalahari;
using gui::dialogs::MessageDialog;
using test::MenuItem;
using test::Window;

namespace {

const QString BUILT_IN = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");
const QString READ_ONLY = QStringLiteral("The book is open read-only");

/// Copy folder @p from, with the folders in it, to @p to
void copyFolder(const QString& from, const QString& to) {
    const QDir source(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = source.relativeFilePath(it.next());
        const QString target = QDir(to).filePath(file);
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(source.filePath(file), target));
    }
}

/// Take kind @p kindId out of the package in folder @p package, as a version without it
void removeKind(const QString& package, const QString& kindId) {
    QFile file(QDir(package).filePath(QStringLiteral("booktype.json")));
    REQUIRE(file.open(QIODevice::ReadOnly));
    QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    QJsonObject kinds = json.value(QStringLiteral("kinds")).toObject();
    kinds.remove(kindId);
    json.insert(QStringLiteral("kinds"), kinds);
    for (const char* list : {"front", "main", "back", "workshop", "start"}) {
        QJsonArray ids = json.value(QLatin1String(list)).toArray();
        for (qsizetype i = ids.size() - 1; i >= 0; --i) {
            if (ids.at(i).toString() == kindId) {
                ids.removeAt(i);
            }
        }
        json.insert(QLatin1String(list), ids);
    }
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(json).toJson());
}

/// What the program does not have of the book's type
enum class Missing {
    Type,  ///< The type: the novel, and the collection of stories that uses it
    Kind   ///< The prologue of the novel, which the book has: an older version of the novel
};

/// A novel "Rain" with a prologue and a chapter with text, made where the program had the
/// novel and open where it does not have it, or not whole: read-only, until the test ends
struct ReadOnlyNovel {
    QTemporaryDir booksDir;
    QTemporaryDir typesDir;
    core::ProjectManager& pm = core::ProjectManager::getInstance();
    QString manifest;
    QString chapterId;

    explicit ReadOnlyNovel(Missing missing) {
        REQUIRE(booksDir.isValid());
        REQUIRE(typesDir.isValid());
        gui::registerAllIcons();  // the cards take the icons of the program along
        pm.loadBookTypes({BUILT_IN});
        REQUIRE(pm.createProject(booksDir.path(), QStringLiteral("Rain"),
                                 QStringLiteral("Author"), QStringLiteral("en"), false,
                                 QStringLiteral("kalahari.novel")));
        manifest = pm.getManifestPath();
        REQUIRE_FALSE(pm.addElement(pm.installedBookTypes().findKind(
                                        QStringLiteral("kalahari.novel"),
                                        QStringLiteral("prologue")),
                                    QStringLiteral("Prologue"), core::BookPlace::Main)
                          .isEmpty());
        chapterId = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Storm"), core::BookPlace::Main);
        REQUIRE_FALSE(chapterId.isEmpty());
        pm.setChapterContent(chapterId, QStringLiteral("<kml><p>Dark night</p></kml>"));
        REQUIRE(pm.saveChapterContent(chapterId));
        REQUIRE(pm.closeProject(false));

        // Another computer
        const QString types = typesDir.filePath(QStringLiteral("booktypes"));
        copyFolder(BUILT_IN, types);
        if (missing == Missing::Type) {
            for (const char* id : {"kalahari.novel", "kalahari.short_stories"}) {
                REQUIRE(QDir(QDir(types).filePath(QLatin1String(id))).removeRecursively());
            }
        } else {
            removeKind(QDir(types).filePath(QStringLiteral("kalahari.novel")),
                       QStringLiteral("prologue"));
        }
        pm.loadBookTypes({types});
        REQUIRE(pm.openProject(manifest));
        REQUIRE(pm.isReadOnly());
    }
    ~ReadOnlyNovel() {
        try {
            if (pm.isProjectOpen()) {
                pm.closeProject(false);
            }
            pm.loadBookTypes({BUILT_IN});
        } catch (...) {
            // A destructor may not throw: the later tests then find this book or its types
            std::fputs("ReadOnlyNovel: the book or the book types of the test stay\n", stderr);
        }
    }
    ReadOnlyNovel(const ReadOnlyNovel&) = delete;
    ReadOnlyNovel& operator=(const ReadOnlyNovel&) = delete;
};

/// The button of the bar at the bottom of @p window
QPushButton* barButton(const Window& window) {
    auto* button = window.infoBar->findChild<QPushButton*>();
    REQUIRE(button != nullptr);
    return button;
}

/// The child of @p parent named @p name
template <typename T>
T* child(const QWidget* parent, const char* name) {
    auto* found = parent->findChild<T*>(QLatin1String(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The item of @p tree whose @p role is @p value
QTreeWidgetItem* itemOf(QTreeWidget& tree, int role, const QString& value) {
    QTreeWidgetItem* found = nullptr;
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr && found == nullptr; ++it) {
        if ((*it)->data(0, role).toString() == value) {
            found = *it;
        }
    }
    INFO("The item " << value.toStdString());
    REQUIRE(found != nullptr);
    return found;
}

/// The texts of the items of a menu, without their access keys, that are enabled (@p enabled)
/// or greyed out
QStringList textsOf(const QList<MenuItem>& items, bool enabled) {
    QStringList texts;
    for (const MenuItem& item : items) {
        if (!item.separator && item.enabled == enabled) {
            texts << QString(item.text).remove(QLatin1Char('&'));
        }
    }
    return texts;
}

/// Whether each greyed out item of a menu says that the book is open read-only
bool greyedSayWhy(const QList<MenuItem>& items) {
    return std::all_of(items.cbegin(), items.cend(), [](const MenuItem& item) {
        return item.separator || item.enabled || item.toolTip == READ_ONLY;
    });
}

} // namespace

TEST_CASE("Read-only book: the message names what the program does not have",
          "[gui][readonly]") {
    CHECK(gui::readOnlyReason() == READ_ONLY);
    CHECK(gui::readOnlyBookMessage().isEmpty());  // no book

    SECTION("its type") {
        const ReadOnlyNovel novel(Missing::Type);
        CHECK(gui::readOnlyBookMessage() ==
              QStringLiteral("The book is open read-only: the program does not have its type "
                             "\"Novel\". You can read it and export it."));
    }
    SECTION("a kind of its type, which an older version of it has not") {
        const ReadOnlyNovel novel(Missing::Kind);
        CHECK(gui::readOnlyBookMessage() ==
              QStringLiteral("The book is open read-only: the program does not have all the "
                             "book uses (Novel). You can read it and export it."));
    }
    SECTION("a book that can be changed: nothing") {
        const test::OpenNovel novel(QStringLiteral("Sun"));
        CHECK(gui::readOnlyBookMessage().isEmpty());
    }
}

TEST_CASE("Read-only book: the window reads it and exports it, and changes nothing",
          "[gui][readonly]") {
    const ReadOnlyNovel novel(Missing::Type);
    auto& pm = core::ProjectManager::getInstance();
    Window window;
    window.documents.onProjectOpened(novel.manifest);
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);

    // The Navigator takes no drag
    CHECK(window.navigatorPanel->isReadOnly());
    CHECK(tree->dragDropMode() == QAbstractItemView::NoDragDrop);

    // The bar at the bottom says why, and exports the archive of the book
    CHECK_FALSE(window.infoBar->isHidden());
    CHECK(window.infoBar->showsReadOnlyBook());
    CHECK(window.infoBar->message() == gui::readOnlyBookMessage());
    QPushButton* exportButton = barButton(window);
    CHECK(exportButton->text() == QStringLiteral("Export Project Archive..."));
    CHECK(exportButton->isEnabled());
    QStringList said;
    CHECK(test::answering([exportButton]() { exportButton->click(); },
                          MessageDialog::Answer::Cancel, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Export Project Archive")));

    // Its chapter is read, and nothing changes it
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    REQUIRE(window.tabs->count() == 1);
    editor::BookEditor* chapter = window.editor(0)->getBookEditor();
    CHECK(chapter->isReadOnly());
    CHECK(chapter->plainText() == QStringLiteral("Dark night"));
    chapter->setCursorPosition({0, 0});
    chapter->typeText(QStringLiteral("So "));
    CHECK(chapter->plainText() == QStringLiteral("Dark night"));
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(window.infoBar->showsReadOnlyBook());  // under the chapter as well

    // The Properties panel: the type greyed out, saying why; the fields only read
    window.properties->showProjectProperties();
    auto* type = child<QLabel>(window.properties, "projectTypeLabel");
    CHECK(type->text() == QStringLiteral("Novel (not in the program)"));
    CHECK_FALSE(type->isEnabled());
    CHECK(type->toolTip() == gui::readOnlyBookMessage());
    CHECK(child<QLineEdit>(window.properties, "projectTitleEdit")->isReadOnly());
    CHECK(child<QLineEdit>(window.properties, "projectAuthorEdit")->isReadOnly());
    CHECK(child<QLineEdit>(window.properties, "projectGenreEdit")->isReadOnly());
    CHECK_FALSE(child<QComboBox>(window.properties, "projectLanguageCombo")->isEnabled());
    window.properties->showChapterProperties(novel.chapterId);
    CHECK(child<QLineEdit>(window.properties, "chapterTitleEdit")->isReadOnly());
    CHECK_FALSE(child<QComboBox>(window.properties, "chapterStatusCombo")->isEnabled());
    CHECK(child<QTextEdit>(window.properties, "chapterNotesEdit")->isReadOnly());

    // New File: a file outside the book, asked nothing, which cannot go into the book
    const qsizetype elements = pm.book()->mainElements.size();
    CHECK(test::accepting([&window]() { window.documents.onNewDocument(); }) == 0);
    REQUIRE(window.tabs->count() == 2);
    CHECK(window.editor(1)->property("isStandaloneFile").toBool());
    CHECK_FALSE(window.infoBar->showsReadOnlyBook());
    CHECK_FALSE(barButton(window)->isEnabled());
    CHECK(barButton(window)->toolTip() == READ_ONLY);
    window.documents.onAddToProject();
    CHECK(window.statusBar.currentMessage() == READ_ONLY);
    CHECK(pm.book()->mainElements.size() == elements);

    // Dismissed, the message stays away until a book opens again
    window.tabs->setCurrentIndex(0);
    REQUIRE(window.infoBar->showsReadOnlyBook());
    emit window.infoBar->dismissed();
    window.tabs->setCurrentIndex(1);
    CHECK_FALSE(window.infoBar->isHidden());  // the file's own bar
    window.tabs->setCurrentIndex(0);
    CHECK(window.infoBar->isHidden());

    // Closed, the window is as for any book
    window.documents.prepareForProjectClose();  // as the main window does
    REQUIRE(pm.closeProject(false));
    window.documents.onProjectClosed();
    CHECK_FALSE(window.navigatorPanel->isReadOnly());
    CHECK(tree->dragDropMode() == QAbstractItemView::InternalMove);
    CHECK_FALSE(window.infoBar->showsReadOnlyBook());
}

TEST_CASE("Read-only book: the Navigator's menus open and show, and change nothing",
          "[gui][readonly]") {
    const ReadOnlyNovel novel(Missing::Kind);
    Window window;
    window.window.setAttribute(Qt::WA_DontShowOnScreen);
    window.window.resize(800, 900);
    window.navigatorPanel->setGeometry(0, 0, 400, 900);
    window.window.show();
    window.documents.onProjectOpened(novel.manifest);
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->expandAll();

    const auto menuOf = [&window, tree](QTreeWidgetItem* item) {
        tree->scrollToItem(item);
        test::ContextMenuReader reader(window.navigatorPanel);
        emit tree->customContextMenuRequested(tree->visualItemRect(item).center());
        REQUIRE(reader.menusRead() == 1);
        return reader.items();
    };

    // A chapter opens and shows its properties, only read
    const QList<MenuItem> chapter = menuOf(itemOf(*tree, Qt::UserRole, novel.chapterId));
    CHECK(textsOf(chapter, true) == QStringList{"Open", "Properties..."});
    CHECK(textsOf(chapter, false) ==
          QStringList{"Rename...", "Delete...", "Move...", "Move to Start", "Move Up",
                      "Move Down", "Move to End", "Set Status"});
    CHECK(greyedSayWhy(chapter));

    // The book's item: its properties are changed in the Book Properties window
    const QList<MenuItem> book =
        menuOf(itemOf(*tree, Qt::UserRole + 1, QStringLiteral("document")));
    CHECK(textsOf(book, false).contains(QStringLiteral("Book Properties...")));
    CHECK(greyedSayWhy(book));
}
