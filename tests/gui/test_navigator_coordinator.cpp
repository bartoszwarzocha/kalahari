/// @file test_navigator_coordinator.cpp
/// @brief The Navigator's commands on the open book project: adding, renaming, moving and
/// deleting elements, which ProjectManager saves at once

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <optional>
#include <string>

using namespace kalahari;

namespace {

/// @brief What answers a dialog a command shows
using Answer = std::function<void(QDialog&)>;

/// @brief The dialog shown now, if any
QDialog* shownDialog() {
    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
        return dialog;
    }
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* dialog = qobject_cast<QDialog*>(widget);
        if (dialog && dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

/// @brief Run @p command, answering the dialogs it shows with @p answers in turn; a dialog
/// beyond them is rejected, so that the test never waits for it
/// @return The number of dialogs the command showed
int answering(const std::function<void()>& command, QList<Answer> answers = {}) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answers]() {
        QDialog* dialog = shownDialog();
        if (!dialog) {
            return;
        }
        ++shown;
        if (answers.isEmpty()) {
            dialog->reject();
        } else {
            answers.takeFirst()(*dialog);
        }
    });
    timer.start();
    command();
    return shown;
}

/// @brief Accept the dialog as it is
void acceptAsItIs(QDialog& dialog) {
    dialog.accept();
}

/// @brief Write @p text in the dialog's field (the title or the name) and accept it
Answer acceptWith(const QString& text) {
    return [text](QDialog& dialog) {
        if (auto* field = dialog.findChild<QLineEdit*>()) {
            field->setText(text);
        }
        dialog.accept();
    };
}

/// @brief Choose the kind at @p index of the dialog's list and accept the title it gets
Answer acceptKind(int index) {
    return [index](QDialog& dialog) {
        if (auto* kinds = dialog.findChild<QComboBox*>()) {
            kinds->setCurrentIndex(index);
        }
        dialog.accept();
    };
}

/// @brief Choose the kind at @p index of the dialog's list and the place whose option is
/// @p option, and accept the title the kind gets
Answer acceptPlace(int index, const QString& option) {
    return [index, option](QDialog& dialog) {
        if (auto* kinds = dialog.findChild<QComboBox*>()) {
            kinds->setCurrentIndex(index);
        }
        for (QRadioButton* button : dialog.findChildren<QRadioButton*>()) {
            if (button->text() == option) {
                button->click();
            }
        }
        dialog.accept();
    };
}

/// @brief Answer a question with @p button
Answer click(QMessageBox::StandardButton button) {
    return [button](QDialog& dialog) {
        auto* box = qobject_cast<QMessageBox*>(&dialog);
        if (QAbstractButton* answer = box ? box->button(button) : nullptr) {
            answer->click();
        } else {
            dialog.reject();
        }
    };
}

/// @brief A new novel, open in ProjectManager until the test ends
///
/// The commands start from an empty book: the elements a novel starts with are taken out.
struct OpenNovel {
    QTemporaryDir dir;
    core::ProjectManager& pm = core::ProjectManager::getInstance();

    OpenNovel() {
        pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(pm.createProject(dir.path(), QStringLiteral("Navigator Test"),
                                 QStringLiteral("Author"), QStringLiteral("en"), false,
                                 QStringLiteral("kalahari.novel")));
        for (const QList<core::ProjectElement>* list :
             {&book().frontElements, &book().mainElements}) {
            while (!list->isEmpty()) {
                REQUIRE(pm.removeElement(list->first().id).has_value());
            }
        }
    }
    ~OpenNovel() { pm.closeProject(false); }
    OpenNovel(const OpenNovel&) = delete;
    OpenNovel& operator=(const OpenNovel&) = delete;

    /// The book as ProjectManager has it
    const core::ProjectBook& book() const {
        const core::ProjectBook* open = pm.book();
        REQUIRE(open != nullptr);
        return *open;
    }

    /// The book as its .klh file has it
    core::ProjectBook savedBook() const {
        QStringList problems;
        const std::optional<core::BookProject> saved = core::BookProject::load(
            QDir(dir.path()).filePath(QStringLiteral("Navigator Test.klh")), problems);
        REQUIRE(saved.has_value());
        REQUIRE(saved->books.size() == 1);
        return saved->books.first();
    }
};

/// @brief The Navigator with its coordinator
struct Navigator {
    QWidget window;
    gui::NavigatorPanel* panel = new gui::NavigatorPanel(&window);
    gui::PropertiesPanel* properties = new gui::PropertiesPanel(&window);
    QTabWidget* tabs = new QTabWidget(&window);
    QStatusBar* statusBar = new QStatusBar(&window);
    gui::NavigatorCoordinator coordinator{panel, properties, tabs, statusBar, &window};

    Navigator() { coordinator.refreshNavigator(); }

    /// The item of element @p id in the tree
    QTreeWidgetItem* item(const QString& id) const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).toString() == id) {
                return *it;
            }
        }
        return nullptr;
    }

    /// The element of the current item of the tree; empty: none or not an element
    QString current() const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        return tree->currentItem() ? tree->currentItem()->data(0, Qt::UserRole).toString()
                                   : QString();
    }

    /// The texts of the items under the item of element @p id
    std::string itemsIn(const QString& id) const {
        const QTreeWidgetItem* parent = item(id);
        REQUIRE(parent != nullptr);
        QStringList texts;
        for (int i = 0; i < parent->childCount(); ++i) {
            texts << parent->child(i)->text(0);
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    }

    /// Choose the action @p text of the menu of the item of element @p id
    /// @return Whether the menu had the action, enabled
    bool chooseInMenu(const QString& id, const QString& text) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        QTreeWidgetItem* found = item(id);
        REQUIRE(found != nullptr);
        tree->scrollToItem(found);
        bool chosen = false;
        bool shown = false;
        QTimer timer;
        timer.setInterval(10);
        QObject::connect(&timer, &QTimer::timeout, [&timer, &chosen, &shown, &text]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            timer.stop();
            shown = true;
            for (QAction* action : menu->actions()) {
                if (action->text() == text && action->isEnabled()) {
                    chosen = true;
                    action->trigger();
                }
            }
            menu->close();
        });
        timer.start();
        emit tree->customContextMenuRequested(tree->visualItemRect(found).center());
        REQUIRE(shown);
        return chosen;
    }

    /// The texts of the items in section @p type ("section_body")
    std::string itemsInSection(const QString& type) const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        const QTreeWidgetItem* section = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it && !section; ++it) {
            if ((*it)->data(0, Qt::UserRole + 1).toString() == type) {
                section = *it;
            }
        }
        INFO("Section " << type.toStdString());
        REQUIRE(section != nullptr);
        QStringList texts;
        for (int i = 0; i < section->childCount(); ++i) {
            texts << section->child(i)->text(0);
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    }
};

/// @brief Titles of @p elements, in order
std::string titles(const QList<core::ProjectElement>& elements) {
    QStringList list;
    for (const core::ProjectElement& element : elements) {
        list << element.title;
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

/// @brief Index of kind @p kindId in @p kinds; -1 when it is not there
int indexOf(const QList<core::KindRef>& kinds, const QString& kindId) {
    for (int i = 0; i < kinds.size(); ++i) {
        if (kinds.at(i).kind->id == kindId) {
            return i;
        }
    }
    return -1;
}

} // namespace

TEST_CASE("Navigator commands: parts, chapters and items of the kinds chosen, saved at once",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;

    // A part, with the title of its kind
    CHECK(answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs}) == 1);
    REQUIRE(novel.book().mainElements.size() == 1);
    const core::ProjectElement part = novel.book().mainElements.first();
    CHECK(part.title == QStringLiteral("Part I"));
    CHECK(part.kind.toString() == QStringLiteral("kalahari.base:part"));

    // A chapter in the part: the type's main kind, numbered
    CHECK(answering([&] { navigator.coordinator.onRequestAddChapter(part.id); },
                    {acceptAsItIs}) == 1);
    REQUIRE(novel.book().mainElements.first().elements.size() == 1);
    const core::ProjectElement chapter = novel.book().mainElements.first().elements.first();
    CHECK(chapter.title == QStringLiteral("Chapter 1"));
    CHECK(chapter.kind.toString() == QStringLiteral("kalahari.base:chapter"));
    CHECK(QFileInfo::exists(pm.filePathOf(chapter)));

    // A prologue in the body itself, chosen from the kinds of the body; its title follows it,
    // and it opens the body
    const int prologue = indexOf(pm.textKindsFor(core::BookPlace::Main), QStringLiteral("prologue"));
    REQUIRE(prologue >= 0);
    CHECK(answering([&] { navigator.coordinator.onRequestAddChapter(QString()); },
                    {acceptKind(prologue)}) == 1);
    CHECK(titles(novel.book().mainElements) == "Prologue, Part I");
    CHECK(novel.book().mainElements.first().kind.toString() ==
          QStringLiteral("kalahari.novel:prologue"));

    // An item of the front matter, with a title of its own
    CHECK(answering([&] { navigator.coordinator.onRequestAddItem(QStringLiteral("front_matter")); },
                    {acceptWith(QStringLiteral("For Anna"))}) == 1);
    CHECK(titles(novel.book().frontElements) == "For Anna");

    // A dialog closed without adding adds nothing
    CHECK(answering([&] { navigator.coordinator.onRequestAddPart(); }) == 1);
    CHECK(titles(novel.book().mainElements) == "Prologue, Part I");

    // The project is saved at once, and the navigator shows it
    CHECK(titles(novel.savedBook().mainElements) == "Prologue, Part I");
    CHECK(titles(novel.savedBook().mainElements.last().elements) == "Chapter 1");
    CHECK(titles(novel.savedBook().frontElements) == "For Anna");
    CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
          "Prologue [Draft], Part I");
    CHECK(navigator.itemsIn(part.id) == "Chapter 1 [Draft]");
    CHECK(navigator.itemsInSection(QStringLiteral("section_frontmatter")) == "For Anna [Draft]");
}

TEST_CASE("Navigator commands: renaming, moving and deleting, saved at once",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;

    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString partId = novel.book().mainElements.first().id;
    answering([&] { navigator.coordinator.onRequestAddChapter(partId); }, {acceptAsItIs});
    answering([&] { navigator.coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 2);
    REQUIRE(novel.book().mainElements.first().elements.size() == 1);
    const core::ProjectElement chapter = novel.book().mainElements.first().elements.first();
    const QString bodyChapterId = novel.book().mainElements.last().id;
    CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");

    // The chapter is open, with unsaved changes
    navigator.coordinator.onElementSelected(chapter.id, chapter.title);
    REQUIRE(navigator.tabs->count() == 1);
    navigator.coordinator.setChapterDirty(chapter.id, true);

    SECTION("Renaming changes the title in the project, the tree and the tab") {
        CHECK(answering([&] { navigator.coordinator.onRequestRename(chapter.id, QString()); },
                        {acceptWith(QStringLiteral("  The Storm "))}) == 1);
        CHECK(pm.findElement(chapter.id)->title == QStringLiteral("The Storm"));
        CHECK(titles(novel.savedBook().mainElements.first().elements) == "The Storm");
        CHECK(navigator.itemsIn(partId) == "The Storm [Draft]");
        CHECK(navigator.tabs->tabText(0) == QStringLiteral("*The Storm"));

        // A dialog closed without a new name changes nothing
        CHECK(answering([&] { navigator.coordinator.onRequestRename(chapter.id, QString()); }) ==
              1);
        CHECK(pm.findElement(chapter.id)->title == QStringLiteral("The Storm"));
    }

    SECTION("Moving changes the order of the list, also by dragging") {
        CHECK(answering([&] { navigator.coordinator.onRequestMove(bodyChapterId, -1); }) == 0);
        CHECK(titles(novel.book().mainElements) == "Chapter 2, Part I");
        CHECK(titles(novel.savedBook().mainElements) == "Chapter 2, Part I");
        CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
              "Chapter 2 [Draft], Part I");
        CHECK(navigator.current() == bodyChapterId);

        // Beyond the end of the list nothing moves
        navigator.coordinator.onRequestMove(bodyChapterId, -1);
        CHECK(titles(novel.book().mainElements) == "Chapter 2, Part I");

        // Dragged to the end
        navigator.coordinator.onElementMoved(bodyChapterId, 1);
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(titles(novel.savedBook().mainElements) == "Part I, Chapter 2");
        CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
              "Part I, Chapter 2 [Draft]");
    }

    SECTION("Deleting a part takes its chapters out of the project and closes their tabs") {
        // Not without the writer's word
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {click(QMessageBox::No)}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(navigator.tabs->count() == 1);

        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {click(QMessageBox::Yes)}) == 1);
        CHECK(titles(novel.book().mainElements) == "Chapter 2");
        CHECK(titles(novel.savedBook().mainElements) == "Chapter 2");
        CHECK(pm.findElement(chapter.id) == nullptr);
        CHECK(navigator.item(partId) == nullptr);
        CHECK(navigator.item(chapter.id) == nullptr);
        CHECK(navigator.tabs->count() == 0);
        CHECK_FALSE(navigator.coordinator.isChapterDirty(chapter.id));

        // The chapter's file stays in the project's folder
        CHECK(QFileInfo::exists(
            QDir(novel.dir.path()).filePath(chapter.file)));
    }
}

TEST_CASE("Navigator commands: the prologue and the epilogue go where the writer chooses",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    const auto addChapter = [&navigator](const QString& groupId) {
        return [&navigator, groupId] { navigator.coordinator.onRequestAddChapter(groupId); };
    };

    // Two parts, with a chapter each
    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 2);
    const QString partOne = novel.book().mainElements.at(0).id;
    const QString partTwo = novel.book().mainElements.at(1).id;
    CHECK(navigator.current() == partTwo);
    answering(addChapter(partOne), {acceptAsItIs});
    answering(addChapter(partTwo), {acceptAsItIs});
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2");

    // The navigator shows the new element, also in a part that was collapsed
    REQUIRE(navigator.item(partTwo) != nullptr);
    CHECK(navigator.item(partTwo)->isExpanded());
    CHECK(navigator.current() == novel.book().mainElements.at(1).elements.first().id);

    // The prologue first in the first part
    const int prologue = indexOf(pm.textKindsFor(core::BookPlace::Main), QStringLiteral("prologue"));
    REQUIRE(prologue >= 0);
    CHECK(answering(addChapter(QString()),
                    {acceptPlace(prologue, QStringLiteral("First in \"Part I\""))}) == 1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II");
    CHECK(titles(novel.book().mainElements.at(0).elements) == "Prologue, Chapter 1");
    CHECK(titles(novel.savedBook().mainElements.at(0).elements) == "Prologue, Chapter 1");
    CHECK(navigator.itemsIn(partOne) == "Prologue [Draft], Chapter 1 [Draft]");

    // The epilogue, from the menu of the second part, closes that part
    const int epilogue =
        indexOf(pm.textKindsFor(core::BookPlace::Main, partTwo), QStringLiteral("epilogue"));
    REQUIRE(epilogue >= 0);
    CHECK(answering(addChapter(partTwo), {acceptKind(epilogue)}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Epilogue");

    // A new chapter of that part goes before the epilogue
    CHECK(answering(addChapter(partTwo), {acceptAsItIs}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Chapter 3, Epilogue");

    // Moved to the end of the body
    const QString epilogueId = novel.book().mainElements.at(1).elements.last().id;
    REQUIRE(pm.removeElement(epilogueId).has_value());
    CHECK(answering(addChapter(partTwo),
                    {acceptPlace(epilogue, QStringLiteral("At the end of the body of the book"))}) ==
          1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II, Epilogue");
}

TEST_CASE("Navigator menu: moving an element to the start and to the end of its list",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::elementMoved, &navigator.coordinator,
                     &gui::NavigatorCoordinator::onElementMoved);
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();

    for (int i = 0; i < 3; ++i) {
        answering([&] { navigator.coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    }
    const QList<core::ProjectElement>& body = novel.book().mainElements;
    REQUIRE(titles(body) == "Chapter 1, Chapter 2, Chapter 3");
    const QString first = body.at(0).id;
    const QString last = body.at(2).id;

    // The first element cannot go to the start, the last one cannot go to the end
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move to Start")));
    CHECK_FALSE(navigator.chooseInMenu(last, QStringLiteral("Move to End")));

    // The moved element stays the current one
    CHECK(navigator.chooseInMenu(last, QStringLiteral("Move to Start")));
    CHECK(titles(novel.book().mainElements) == "Chapter 3, Chapter 1, Chapter 2");
    CHECK(navigator.current() == last);
    CHECK(navigator.chooseInMenu(last, QStringLiteral("Move to End")));
    CHECK(titles(novel.book().mainElements) == "Chapter 1, Chapter 2, Chapter 3");
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move to End")));
    CHECK(titles(novel.book().mainElements) == "Chapter 2, Chapter 3, Chapter 1");
    CHECK(navigator.current() == first);
    CHECK(titles(novel.savedBook().mainElements) == "Chapter 2, Chapter 3, Chapter 1");
    CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
          "Chapter 2 [Draft], Chapter 3 [Draft], Chapter 1 [Draft]");
}
