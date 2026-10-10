/// @file test_navigator_coordinator.cpp
/// @brief The Navigator's commands on the open book project: adding, renaming, moving and
/// deleting elements, which ProjectManager saves at once

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
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

/// @brief Accept a new part, leaving the elements that end the body where they are
void acceptLeaving(QDialog& dialog) {
    if (auto* take = dialog.findChild<QCheckBox*>()) {
        take->setChecked(false);
    }
    dialog.accept();
}

/// @brief Answer a question of the program's message window with the button of its action
/// (@p doIt) or with Cancel; @p asked gets the question, with its title
Answer answerQuestion(bool doIt, QString* asked = nullptr) {
    return [doIt, asked](QDialog& dialog) {
        auto* message = qobject_cast<gui::dialogs::MessageDialog*>(&dialog);
        if (!message) {
            dialog.reject();
            return;
        }
        if (asked) {
            *asked = message->clipboardText();
        }
        (doIt ? message->acceptButton() : message->cancelButton())->click();
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

    /// The item of section @p type ("section_body"); nullptr: none
    QTreeWidgetItem* section(const QString& type) const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole + 1).toString() == type) {
                return *it;
            }
        }
        return nullptr;
    }

    /// The item of the book
    QTreeWidgetItem* bookItem() const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(tree->topLevelItemCount() > 0);
        return tree->topLevelItem(0);
    }

    /// The texts of the items under the item of the book
    std::string itemsInBook() const {
        const QTreeWidgetItem* book = bookItem();
        QStringList texts;
        for (int i = 0; i < book->childCount(); ++i) {
            texts << book->child(i)->text(0);
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    }

    /// Show the menu of @p found and choose its action @p text when the menu has it, enabled,
    /// and @p choose says so
    /// @return Whether the menu had the action, enabled
    bool inMenuOf(QTreeWidgetItem* found, const QString& text, bool choose = true) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(found != nullptr);
        tree->scrollToItem(found);
        bool enabled = false;
        bool shown = false;
        QTimer timer;
        timer.setInterval(10);
        QObject::connect(&timer, &QTimer::timeout, [&timer, &enabled, &shown, &text, choose]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            timer.stop();
            shown = true;
            for (QAction* action : menu->actions()) {
                if (action->text() == text && action->isEnabled()) {
                    enabled = true;
                    if (choose) {
                        action->trigger();
                    }
                }
            }
            menu->close();
        });
        timer.start();
        emit tree->customContextMenuRequested(tree->visualItemRect(found).center());
        REQUIRE(shown);
        return enabled;
    }

    /// Choose the action @p text of the menu of the item of element @p id
    /// @return Whether the menu had the action, enabled
    bool chooseInMenu(const QString& id, const QString& text) {
        QTreeWidgetItem* found = item(id);
        REQUIRE(found != nullptr);
        return inMenuOf(found, text);
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

    SECTION("Deleting a chapter says where its file stays") {
        const core::ProjectElement* bodyChapter = pm.findElement(bodyChapterId);
        REQUIRE(bodyChapter != nullptr);
        const QString file = pm.filePathOf(*bodyChapter);
        QString asked;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(bodyChapterId); },
                        {answerQuestion(true, &asked)}) == 1);
        CHECK(asked.contains(QStringLiteral("Its file stays in the book's folder:\n") +
                             QDir::toNativeSeparators(file)));
        CHECK(titles(novel.book().mainElements) == "Part I");
        CHECK(QFileInfo::exists(file));
    }

    SECTION("Deleting a part takes its chapters out of the project and closes their tabs") {
        // Not without the writer's word; the question says that the files stay
        QString asked;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerQuestion(false, &asked)}) == 1);
        CHECK(asked.contains(QStringLiteral("Delete \"Part I\" from the book?")));
        CHECK(asked.contains(QStringLiteral("their files stay in the book's folder")));
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(navigator.tabs->count() == 1);

        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerQuestion(true)}) == 1);
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
                    {acceptPlace(prologue, QStringLiteral("First in the part \"Part I\""))}) ==
          1);
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
                    {acceptPlace(epilogue, QStringLiteral("At the end of the main section"))}) ==
          1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II, Epilogue");

    // A new chapter of the body goes before it
    CHECK(answering(addChapter(QString()), {acceptAsItIs}) == 1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II, Chapter 4, Epilogue");
}

TEST_CASE("Navigator commands: the epilogue at the end of the last part stays the last element",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    const auto addChapter = [&navigator](const QString& groupId) {
        return [&navigator, groupId] { navigator.coordinator.onRequestAddChapter(groupId); };
    };

    // A part with a chapter and the epilogue
    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString partOne = novel.book().mainElements.at(0).id;
    answering(addChapter(partOne), {acceptAsItIs});
    const int epilogue =
        indexOf(pm.textKindsFor(core::BookPlace::Main, partOne), QStringLiteral("epilogue"));
    REQUIRE(epilogue >= 0);
    answering(addChapter(partOne), {acceptKind(epilogue)});
    REQUIRE(titles(novel.book().mainElements.at(0).elements) == "Chapter 1, Epilogue");

    SECTION("A new chapter of the body goes before it, at the end of the part") {
        CHECK(answering(addChapter(QString()), {acceptAsItIs}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I");
        CHECK(titles(novel.book().mainElements.at(0).elements) ==
              "Chapter 1, Chapter 2, Epilogue");
        CHECK(titles(novel.savedBook().mainElements.at(0).elements) ==
              "Chapter 1, Chapter 2, Epilogue");
    }

    SECTION("The writer can put it after the part") {
        const int chapter =
            indexOf(pm.textKindsFor(core::BookPlace::Main), QStringLiteral("chapter"));
        REQUIRE(chapter >= 0);
        CHECK(answering(addChapter(QString()),
                        {acceptPlace(chapter, QStringLiteral(
                                                  "At the end of the main section, after \"Part "
                                                  "I\""))}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(titles(novel.book().mainElements.at(0).elements) == "Chapter 1, Epilogue");
    }

    SECTION("A new part takes it to its end, and its chapters go before it") {
        CHECK(answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs}) == 1);
        REQUIRE(titles(novel.book().mainElements) == "Part I, Part II");
        const QString partTwo = novel.book().mainElements.at(1).id;
        CHECK(titles(novel.book().mainElements.at(0).elements) == "Chapter 1");
        CHECK(titles(novel.book().mainElements.at(1).elements) == "Epilogue");
        CHECK(titles(novel.savedBook().mainElements.at(1).elements) == "Epilogue");
        CHECK(navigator.itemsIn(partTwo) == "Epilogue [Draft]");

        CHECK(answering(addChapter(partTwo), {acceptAsItIs}) == 1);
        CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Epilogue");
    }

    SECTION("A new part leaves it where it is when the writer wants") {
        CHECK(answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptLeaving}) ==
              1);
        REQUIRE(titles(novel.book().mainElements) == "Part I, Part II");
        CHECK(titles(novel.book().mainElements.at(0).elements) == "Chapter 1, Epilogue");
        CHECK(novel.book().mainElements.at(1).elements.isEmpty());
    }
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

    // A chapter moved to the start stays after the prologue, and one moved to the end before
    // the epilogue; the chapter before the epilogue is at the end already
    core::ProjectManager& pm = novel.pm;
    REQUIRE_FALSE(pm.addElement(pm.bookTypes().findKind(QStringLiteral("kalahari.novel"),
                                                        QStringLiteral("prologue")),
                                QStringLiteral("Prologue"), core::BookPlace::Main)
                      .isEmpty());
    REQUIRE_FALSE(pm.addElement(pm.bookTypes().findKind(QStringLiteral("kalahari.novel"),
                                                        QStringLiteral("epilogue")),
                                QStringLiteral("Epilogue"), core::BookPlace::Main)
                      .isEmpty());
    navigator.coordinator.refreshNavigator();
    REQUIRE(titles(novel.book().mainElements) ==
            "Prologue, Chapter 2, Chapter 3, Chapter 1, Epilogue");
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move to Start")));
    CHECK(titles(novel.book().mainElements) ==
          "Prologue, Chapter 1, Chapter 2, Chapter 3, Epilogue");
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move to Start")));
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move to End")));
    CHECK(titles(novel.book().mainElements) ==
          "Prologue, Chapter 2, Chapter 3, Chapter 1, Epilogue");
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move to End")));
    CHECK(navigator.current() == first);
}

TEST_CASE("Navigator commands: the sections are hidden, shown and renamed, saved at once",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& coordinator = navigator.coordinator;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestShowSections, &coordinator,
                     &gui::NavigatorCoordinator::onRequestShowSections);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestRenameSection, &coordinator,
                     &gui::NavigatorCoordinator::onRequestRenameSection);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestMoveElement, &coordinator,
                     &gui::NavigatorCoordinator::onRequestMove);
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();

    // Two chapters, and an item at each end of the book
    answering([&] { coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    answering([&] { coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    answering([&] { coordinator.onRequestAddItem(QStringLiteral("front_matter")); },
              {acceptWith(QStringLiteral("For Anna"))});
    answering([&] { coordinator.onRequestAddItem(QStringLiteral("back_matter")); },
              {acceptWith(QStringLiteral("Thanks"))});
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1, Chapter 2");
    const QString first = novel.book().mainElements.at(0).id;
    const QString second = novel.book().mainElements.at(1).id;

    // A section shown in the Properties panel goes when the sections are hidden
    navigator.properties->showSectionProperties(QStringLiteral("section_body"));
    REQUIRE(navigator.properties->currentPage() == gui::PropertiesPanel::Page::Section);

    // Hidden from the book's menu: the elements one after another, each in its section
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Show Sections")));
    CHECK_FALSE(novel.book().partsLayer);
    CHECK_FALSE(novel.savedBook().partsLayer);
    CHECK(titles(novel.book().frontElements) == "For Anna");
    CHECK(titles(novel.book().backElements) == "Thanks");
    CHECK(navigator.itemsInBook() ==
          "For Anna [Draft], Chapter 1 [Draft], Chapter 2 [Draft], Thanks [Draft]");
    CHECK(navigator.section(QStringLiteral("section_body")) == nullptr);
    CHECK(navigator.properties->currentPage() == gui::PropertiesPanel::Page::Project);

    // The elements move within their sections
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move Up")));
    CHECK_FALSE(navigator.chooseInMenu(second, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(second, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().mainElements) == "Chapter 2, Chapter 1");
    CHECK(navigator.itemsInBook() ==
          "For Anna [Draft], Chapter 2 [Draft], Chapter 1 [Draft], Thanks [Draft]");

    // The book's menu adds the elements its sections had added
    QStringList requested;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAddPart,
                     [&requested]() { requested << QStringLiteral("part"); });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAddChapter,
                     [&requested](const QString& groupId) {
                         requested << QStringLiteral("chapter:") + groupId;
                     });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAddItem,
                     [&requested](const QString& section) { requested << section; });
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part")));
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Chapter")));
    CHECK(navigator.inMenuOf(navigator.bookItem(),
                             QStringLiteral("Add Item at the Beginning of the Book")));
    CHECK(navigator.inMenuOf(navigator.bookItem(),
                             QStringLiteral("Add Item at the End of the Book")));
    CHECK(requested.join(QStringLiteral(", ")).toStdString() ==
          "part, chapter:, front_matter, back_matter");

    // An item added from the book's menu says where it goes
    QString said;
    CHECK(answering([&] { coordinator.onRequestAddItem(QStringLiteral("back_matter")); },
                    {[&said](QDialog& dialog) {
                         for (const QLabel* label : dialog.findChildren<QLabel*>()) {
                             said += label->text() + QLatin1Char('\n');
                         }
                         dialog.accept();
                     }}) == 1);
    // Named after its kind, the first one the end of the book can still have
    INFO(said.toStdString());
    CHECK(said.contains(
        QStringLiteral("The acknowledgments are added at the very end of the book.")));
    CHECK(novel.book().backElements.size() == 2);

    // Shown again: the elements are in their sections, the main one expanded as when the book
    // opens
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Show Sections")));
    CHECK(novel.book().partsLayer);
    CHECK(novel.savedBook().partsLayer);
    CHECK(navigator.itemsInSection(QStringLiteral("section_frontmatter")) == "For Anna [Draft]");
    CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
          "Chapter 2 [Draft], Chapter 1 [Draft]");
    CHECK(navigator.section(QStringLiteral("section_body"))->isExpanded());
    CHECK_FALSE(navigator.section(QStringLiteral("section_frontmatter"))->isExpanded());
    CHECK(navigator.bookItem()->isExpanded());
    CHECK_FALSE(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part"), false));

    // A section gets the writer's name; the others keep theirs
    CHECK(navigator.inMenuOf(navigator.section(QStringLiteral("section_body")),
                             QStringLiteral("Rename..."), false));
    navigator.properties->showSectionProperties(QStringLiteral("section_body"));
    CHECK(answering([&] { coordinator.onRequestRenameSection(QStringLiteral("section_body")); },
                    {acceptWith(QStringLiteral(" Story "))}) == 1);
    CHECK(novel.book().sectionSet == QStringLiteral("custom"));
    CHECK(novel.book().sectionNames ==
          QStringList{QStringLiteral("Front Section"), QStringLiteral("Story"),
                      QStringLiteral("Back Section")});
    CHECK(novel.savedBook().sectionNames == novel.book().sectionNames);
    CHECK(navigator.section(QStringLiteral("section_body"))->text(0) == QStringLiteral("Story"));
    const auto propertiesShow = [&navigator](const QString& text) {
        for (const QLabel* label : navigator.properties->findChildren<QLabel*>()) {
            if (label->isVisibleTo(navigator.properties) && label->text() == text) {
                return true;
            }
        }
        return false;
    };
    CHECK(propertiesShow(QStringLiteral("Story")));
    CHECK(propertiesShow(QStringLiteral("Chapters:")));

    // The back section counts its elements, not chapters
    navigator.properties->showSectionProperties(QStringLiteral("section_backmatter"));
    CHECK(propertiesShow(QStringLiteral("Back Section")));
    CHECK(propertiesShow(QStringLiteral("Elements:")));
    CHECK_FALSE(propertiesShow(QStringLiteral("Chapters:")));

    // A dialog closed without a new name changes nothing
    CHECK(answering([&] {
              coordinator.onRequestRenameSection(QStringLiteral("section_frontmatter"));
          }) == 1);
    CHECK(novel.book().sectionName(core::BookPlace::Front) == QStringLiteral("Front Section"));
}

TEST_CASE("Properties panel: the sections of the book and their names, saved at once",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& coordinator = navigator.coordinator;
    gui::PropertiesPanel* properties = navigator.properties;
    QObject::connect(properties, &gui::PropertiesPanel::requestSections, &coordinator,
                     &gui::NavigatorCoordinator::onRequestSections);
    QObject::connect(properties, &gui::PropertiesPanel::bookChanged, &coordinator,
                     &gui::NavigatorCoordinator::refreshNavigator);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestShowSections, &coordinator,
                     &gui::NavigatorCoordinator::onRequestShowSections);
    navigator.window.resize(400, 600);
    navigator.window.show();
    properties->showProjectProperties();

    auto* sections = properties->findChild<QComboBox*>(QStringLiteral("projectSectionsCombo"));
    auto* namesRow = properties->findChild<QWidget*>(QStringLiteral("projectSectionNames"));
    REQUIRE(sections != nullptr);
    REQUIRE(namesRow != nullptr);
    const QList<QLineEdit*> names = namesRow->findChildren<QLineEdit*>();
    REQUIRE(names.size() == 3);
    QComboBox* language = nullptr;
    QLineEdit* title = nullptr;
    for (QComboBox* combo : properties->findChildren<QComboBox*>()) {
        if (combo->findData(QStringLiteral("pl")) >= 0) {
            language = combo;
        }
    }
    for (QLineEdit* field : properties->findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Enter project title")) {
            title = field;
        }
    }
    REQUIRE(language != nullptr);
    REQUIRE(title != nullptr);

    const auto choose = [sections](const char* item) {
        const int index = sections->findData(QString::fromLatin1(item));
        REQUIRE(index >= 0);
        sections->setCurrentIndex(index);
    };
    const auto rename = [](QLineEdit* field, const QString& name) {
        field->setText(name);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(field, &enter);
    };
    const auto namesInFields = [&names]() {
        QStringList texts;
        for (const QLineEdit* field : names) {
            texts << field->text();
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    };
    const auto sectionsInNavigator = [&navigator]() {
        QStringList texts;
        for (const char* type : {"section_frontmatter", "section_body", "section_backmatter"}) {
            const QTreeWidgetItem* item = navigator.section(QString::fromLatin1(type));
            texts << (item ? item->text(0) : QStringLiteral("-"));
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    };
    const auto namesRowShown = [namesRow, properties]() {
        return namesRow->isVisibleTo(properties);
    };

    // The novel's sections, their sets in the language of the book
    CHECK(sections->currentData().toString() == QStringLiteral("sections"));
    CHECK(sections->itemText(0) ==
          QStringLiteral("Front Section · Main Section · Back Section"));
    CHECK_FALSE(namesRowShown());

    // Another set: the Navigator shows its names, the .klh file has it
    choose("matter");
    CHECK(novel.book().sectionSet == QStringLiteral("matter"));
    CHECK(novel.savedBook().sectionSet == QStringLiteral("matter"));
    CHECK(sectionsInNavigator() == "Front Matter, Body, Back Matter");
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Sections renamed"));

    // The language of the book names the sets and the sections at once
    language->setCurrentIndex(language->findData(QStringLiteral("pl")));
    CHECK(novel.book().language == QStringLiteral("pl"));
    CHECK(sections->itemText(1) ==
          QStringLiteral("Strony początkowe · Tekst główny · Strony końcowe"));
    CHECK(sections->currentData().toString() == QStringLiteral("matter"));
    CHECK(sectionsInNavigator() == "Strony początkowe, Tekst główny, Strony końcowe");

    // Own names start as the names the sections have
    choose("custom");
    CHECK(namesRowShown());
    CHECK(namesInFields() == "Strony początkowe, Tekst główny, Strony końcowe");
    CHECK(novel.book().sectionSet == QStringLiteral("custom"));
    CHECK(novel.savedBook().sectionNames ==
          QStringList{QStringLiteral("Strony początkowe"), QStringLiteral("Tekst główny"),
                      QStringLiteral("Strony końcowe")});

    // The writer's name, without the spaces at its ends; an empty one is the name of the first
    // set
    rename(names.at(1), QStringLiteral(" Historia "));
    rename(names.at(2), QString());
    CHECK(novel.savedBook().sectionNames ==
          QStringList{QStringLiteral("Strony początkowe"), QStringLiteral("Historia"),
                      QStringLiteral("Sekcja końcowa")});
    CHECK(sectionsInNavigator() == "Strony początkowe, Historia, Sekcja końcowa");
    CHECK(namesInFields() == "Strony początkowe, Historia, Sekcja końcowa");

    // No sections: the elements in one list, and the names kept for when the sections return
    choose("none");
    CHECK_FALSE(novel.book().partsLayer);
    CHECK_FALSE(novel.savedBook().partsLayer);
    CHECK_FALSE(namesRowShown());
    CHECK(sectionsInNavigator() == "-, -, -");
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Sections hidden"));
    choose("custom");
    CHECK(novel.book().partsLayer);
    CHECK(sectionsInNavigator() == "Strony początkowe, Historia, Sekcja końcowa");
    CHECK(navigator.section(QStringLiteral("section_body"))->isExpanded());
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Sections shown"));

    // The field follows the Navigator's menu
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Show Sections")));
    CHECK_FALSE(novel.book().partsLayer);
    CHECK(sections->currentData().toString() == QStringLiteral("none"));
    CHECK_FALSE(namesRowShown());

    // The Navigator shows the new title of the book
    rename(title, QStringLiteral("Renamed Book"));
    CHECK(novel.book().title == QStringLiteral("Renamed Book"));
    CHECK(navigator.bookItem()->text(0) == QStringLiteral("Renamed Book"));
}
