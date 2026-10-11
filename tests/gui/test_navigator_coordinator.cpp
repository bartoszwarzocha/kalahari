/// @file test_navigator_coordinator.cpp
/// @brief The Navigator's commands on the open book project: adding, renaming, moving and
/// deleting elements, which ProjectManager saves at once, and the files that no element has

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/kind_words.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/dialogs/add_to_book_dialog.h>
#include <kalahari/gui/dialogs/delete_dialogs.h>
#include <kalahari/gui/dialogs/file_preview_dialog.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/utils/file_location.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QChildEvent>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
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
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <optional>
#include <string>
#include <utility>

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

/// @brief Choose the option of the place with text @p option and accept the dialog
Answer acceptOption(const QString& option) {
    return [option](QDialog& dialog) {
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

/// @brief Answer the window that deletes an element from the book with Delete (@p doIt), the
/// files too when @p withFiles says so, or with Cancel; @p said gets what the window says
Answer answerDelete(bool doIt, bool withFiles = false, QString* said = nullptr) {
    return [doIt, withFiles, said](QDialog& dialog) {
        auto* window = qobject_cast<gui::dialogs::DeleteFromBookDialog*>(&dialog);
        if (!window) {
            dialog.reject();
            return;
        }
        window->deleteFilesBox()->setChecked(withFiles);
        if (said) {
            *said = window->clipboardText();
        }
        (doIt ? window->acceptButton() : window->cancelButton())->click();
    };
}

/// @brief Answer the window that deletes files of "Unassigned Files" with Delete (@p doIt) or
/// with Cancel; @p said gets what the window says
Answer answerDeleteFiles(bool doIt, QString* said = nullptr) {
    return [doIt, said](QDialog& dialog) {
        auto* window = qobject_cast<gui::dialogs::DeleteFilesDialog*>(&dialog);
        if (!window) {
            dialog.reject();
            return;
        }
        if (said) {
            *said = window->clipboardText();
        }
        (doIt ? window->acceptButton() : window->cancelButton())->click();
    };
}

/// @brief @p text with the name of the system's Trash for "{bin}"
QString withBin(const QString& text) {
    return core::fillWords(text, {{QStringLiteral("bin"), gui::dialogs::trashWords()}});
}

/// @brief The text of chapter file @p path; empty when it cannot be read
QString textOf(const QString& path) {
    const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(path);
    return chapter ? chapter->plainText() : QString();
}

/// @brief The Trash of a test in place of the system's until the test ends; it keeps the text
/// each file had when it took it
struct TestTrash {
    bool exists = true;  ///< The files can go to it
    bool takes = true;   ///< It takes them; else it refuses them
    QStringList taken;   ///< The files it took
    QStringList texts;   ///< Their texts

    TestTrash() {
        core::setTrashForTests([this](const QString&) { return exists; },
                               [this](const QString& path) {
                                   if (!takes) {
                                       return false;
                                   }
                                   taken << path;
                                   texts << textOf(path);
                                   return QFile::remove(path);
                               });
    }
    ~TestTrash() { core::setTrashForTests({}, {}); }
    TestTrash(const TestTrash&) = delete;
    TestTrash& operator=(const TestTrash&) = delete;
};

/// @brief The file manager of a test in place of the system's until the test ends
struct TestFileManager {
    QStringList shown;  ///< What the program showed in it

    TestFileManager() {
        gui::setFileManagerForTests([this](const QString& path) {
            shown << path;
            return true;
        });
    }
    ~TestFileManager() { gui::setFileManagerForTests({}); }
    TestFileManager(const TestFileManager&) = delete;
    TestFileManager& operator=(const TestFileManager&) = delete;
};

/// @brief A new novel, open in ProjectManager until the test ends
///
/// The commands start from an empty book: the elements a novel starts with are taken out,
/// with their files, which the Navigator would list under "Unassigned Files".
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
                const std::optional<core::ProjectElement> removed =
                    pm.removeElement(list->first().id);
                REQUIRE(removed.has_value());
                if (removed) {
                    for (const QString& file : pm.filesOf(*removed)) {
                        REQUIRE(QFile::remove(file));
                    }
                }
            }
        }
        REQUIRE(pm.unassignedFiles().isEmpty());
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

/// @brief Reads each menu of @p panel with @p read as the menu is about to show, and closes it
///
/// The menu runs as for the writer: with its actions, the item they act on and the action
/// chosen while it is open. It gets no window on the screen, where the platform decides about
/// a popup: on macOS and Windows the popup of a test window was sometimes closed before the
/// test could read it.
class MenuReader : public QObject {
public:
    MenuReader(QWidget* panel, std::function<void(QMenu&)> read)
        : m_panel(panel), m_read(std::move(read)) {
        m_panel->installEventFilter(this);
    }
    ~MenuReader() override { m_panel->removeEventFilter(this); }
    MenuReader(const MenuReader&) = delete;
    MenuReader& operator=(const MenuReader&) = delete;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        // The panel's menu is polished before it shows, with all its actions. A style that
        // sets window flags of the menu as it polishes it (Windows 11) polishes it again, and
        // the menu is still read once.
        if (watched == m_panel && event->type() == QEvent::ChildPolished) {
            if (auto* menu = qobject_cast<QMenu*>(static_cast<QChildEvent*>(event)->child())) {
                connect(menu, &QMenu::aboutToShow, this, &MenuReader::read, Qt::UniqueConnection);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    /// @brief Read the menu that is about to show, and close it once it is open
    void read() {
        auto* menu = qobject_cast<QMenu*>(sender());
        if (!menu) {
            return;
        }
        menu->setAttribute(Qt::WA_DontShowOnScreen);
        m_read(*menu);
        // Closed once it is open, the menu ends its event loop
        QTimer::singleShot(0, menu, [menu]() { menu->close(); });
    }

    QWidget* m_panel;
    std::function<void(QMenu&)> m_read;
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
    /// and @p choose says so; @p toolTip gets the tooltip of the action
    /// @return Whether the menu had the action, enabled
    bool inMenuOf(QTreeWidgetItem* found, const QString& text, bool choose = true,
                  QString* toolTip = nullptr) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(found != nullptr);
        tree->scrollToItem(found);
        const QPoint at = tree->visualItemRect(found).center();
        REQUIRE(tree->itemAt(at) == found);
        bool enabled = false;
        bool shown = false;
        MenuReader reader(panel, [&enabled, &shown, &text, choose, toolTip](QMenu& menu) {
            shown = true;
            for (QAction* action : menu.actions()) {
                if (toolTip && action->text() == text) {
                    *toolTip = action->toolTip();
                }
                if (action->text() == text && action->isEnabled()) {
                    enabled = true;
                    if (choose) {
                        action->trigger();
                    }
                }
            }
        });
        emit tree->customContextMenuRequested(at);
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

TEST_CASE("Navigator commands: an element moves to the place chosen in the Move dialog, or "
          "dragged there", "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestMoveTo,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onRequestMoveTo);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::elementMovedTo,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onElementMovedTo);

    // A part with a chapter, a chapter of the body and the epilogue that ends the book
    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString partId = novel.book().mainElements.first().id;
    answering([&] { navigator.coordinator.onRequestAddChapter(partId); }, {acceptAsItIs});
    answering([&] { navigator.coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    const QString epilogueId = pm.addElement(
        pm.bookTypes().findKind(QStringLiteral("kalahari.novel"), QStringLiteral("epilogue")),
        QStringLiteral("Epilogue"), core::BookPlace::Main);
    navigator.coordinator.refreshNavigator();
    REQUIRE(titles(novel.book().mainElements) == "Part I, Chapter 2, Epilogue");
    const QString chapterId = novel.book().mainElements.at(1).id;

    SECTION("Move... puts the epilogue last in the part, as the option of the dialog says") {
        CHECK(answering([&] { navigator.chooseInMenu(epilogueId, QStringLiteral("Move...")); },
                        {acceptOption(QStringLiteral("Last in the part \"Part I\""))}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(titles(novel.book().mainElements.first().elements) == "Chapter 1, Epilogue");
        CHECK(titles(novel.savedBook().mainElements.first().elements) == "Chapter 1, Epilogue");
        CHECK(navigator.item(epilogueId)->parent() == navigator.item(partId));
        CHECK(navigator.current() == epilogueId);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Epilogue moved: Epilogue"));

        // A dialog closed without a new place changes nothing
        CHECK(answering([&] { navigator.chooseInMenu(epilogueId, QStringLiteral("Move...")); }) ==
              1);
        CHECK(answering([&] { navigator.chooseInMenu(epilogueId, QStringLiteral("Move...")); },
                        {acceptAsItIs}) == 1);
        CHECK(titles(novel.book().mainElements.first().elements) == "Chapter 1, Epilogue");
    }

    SECTION("Dragged into the part, and out of it to the start of the body") {
        emit navigator.panel->elementMovedTo(chapterId, {core::BookPlace::Main, partId, 0});
        CHECK(titles(novel.book().mainElements) == "Part I, Epilogue");
        CHECK(titles(novel.book().mainElements.first().elements) == "Chapter 2, Chapter 1");
        CHECK(titles(novel.savedBook().mainElements.first().elements) == "Chapter 2, Chapter 1");
        CHECK(navigator.item(chapterId)->parent() == navigator.item(partId));
        CHECK(navigator.current() == chapterId);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Chapter moved: Chapter 2"));

        emit navigator.panel->elementMovedTo(chapterId, {core::BookPlace::Main, QString(), 0});
        CHECK(titles(novel.book().mainElements) == "Chapter 2, Part I, Epilogue");
        CHECK(titles(novel.savedBook().mainElements) == "Chapter 2, Part I, Epilogue");
    }

    SECTION("Move... is offered for an element that has another place, of a known kind") {
        const QString titleId = pm.addElement(
            pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("title_page")),
            QStringLiteral("Title Page"), core::BookPlace::Front);
        core::ProjectElement unknown;
        unknown.id = QStringLiteral("arc");
        unknown.kind = {QStringLiteral("acme.thriller"), QStringLiteral("arc")};
        unknown.title = QStringLiteral("Arc");
        pm.project()->elementsIn(core::BookPlace::Main).append(unknown);
        navigator.coordinator.refreshNavigator();

        CHECK(navigator.inMenuOf(navigator.item(epilogueId), QStringLiteral("Move..."), false));
        CHECK(navigator.inMenuOf(navigator.item(partId), QStringLiteral("Move..."), false));
        CHECK_FALSE(
            navigator.inMenuOf(navigator.item(titleId), QStringLiteral("Move..."), false));
        QString why;
        CHECK_FALSE(navigator.inMenuOf(navigator.item(QStringLiteral("arc")),
                                       QStringLiteral("Move..."), false, &why));
        CHECK(why == QStringLiteral("The program does not have the package of this element's "
                                    "kind."));
    }

    SECTION("Move... puts an element where its kind can be, also from a place where it cannot") {
        // A title page in the body, from an older package: the front matter is its only place
        core::ProjectElement stray;
        stray.id = QStringLiteral("stray");
        stray.kind = {QStringLiteral("kalahari.base"), QStringLiteral("title_page")};
        stray.title = QStringLiteral("Title Page");
        pm.project()->elementsIn(core::BookPlace::Main).append(stray);
        navigator.coordinator.refreshNavigator();
        REQUIRE(pm.movePlacesOf(stray.id) ==
                QList<core::ElementPlace>{{core::BookPlace::Front, QString(), 0}});

        CHECK(navigator.inMenuOf(navigator.item(stray.id), QStringLiteral("Move..."), false));
        CHECK(answering([&] { navigator.chooseInMenu(stray.id, QStringLiteral("Move...")); },
                        {acceptAsItIs}) == 1);
        CHECK(titles(novel.book().frontElements) == "Title Page");
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2, Epilogue");
        CHECK(titles(novel.savedBook().frontElements) == "Title Page");
    }

    SECTION("A book that cannot be saved stays as it was, and the writer is told") {
        const QString manifest =
            QDir(novel.dir.path()).filePath(QStringLiteral("Navigator Test.klh"));
        REQUIRE(QFile::remove(manifest));
        REQUIRE(QDir().mkdir(manifest));
        QString said;
        CHECK(answering(
                  [&] {
                      emit navigator.panel->elementMovedTo(chapterId,
                                                           {core::BookPlace::Main, partId, 0});
                  },
                  {answerQuestion(true, &said)}) == 1);
        CHECK(said.contains(QStringLiteral("Move Chapter Failed")));
        CHECK(said.contains(QStringLiteral("The changes in the book could not be saved.")));
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2, Epilogue");
        CHECK(navigator.item(chapterId)->parent() == navigator.section(QStringLiteral(
                                                         "section_body")));
        REQUIRE(QDir().rmdir(manifest));
    }
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

    SECTION("Deleting a chapter leaves its file in the book's folder, under \"Unassigned "
            "Files\"") {
        TestTrash trash;
        const core::ProjectElement* bodyChapter = pm.findElement(bodyChapterId);
        REQUIRE(bodyChapter != nullptr);
        const QString file = pm.filePathOf(*bodyChapter);
        const QString name = QFileInfo(file).fileName();
        navigator.properties->showChapterProperties(chapter.id);
        QString said;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(bodyChapterId); },
                        {answerDelete(true, false, &said)}) == 1);
        INFO(said.toStdString());
        CHECK(said.contains(QStringLiteral("Delete the chapter \"Chapter 2\" from the book?")));
        CHECK(said.contains(QStringLiteral("Chapter 2 – ") + name));
        CHECK(said.contains(QStringLiteral(
            "The file stays in the book's folder. You will find it in the Navigator, under "
            "\"Unassigned Files\", where you can add it back to the book or delete it.")));
        CHECK(said.contains(QStringLiteral("Folder: ") +
                            QDir::toNativeSeparators(QFileInfo(file).absolutePath())));
        CHECK(titles(novel.book().mainElements) == "Part I");
        CHECK(titles(novel.savedBook().mainElements) == "Part I");
        CHECK(QFileInfo::exists(file));
        CHECK(trash.taken.isEmpty());
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Chapter deleted from the book: Chapter 2"));

        // The Properties panel keeps showing a chapter that is still in the book
        CHECK(navigator.properties->currentPage() == gui::PropertiesPanel::Page::Chapter);

        // The Navigator lists the file after the sections of the book, open
        QTreeWidgetItem* unassigned = navigator.section(QStringLiteral("unassigned_files"));
        REQUIRE(unassigned != nullptr);
        CHECK(unassigned->text(0) == QStringLiteral("Unassigned Files (1)"));
        CHECK(unassigned->isExpanded());
        CHECK(unassigned->parent() == navigator.bookItem());
        CHECK(navigator.bookItem()->indexOfChild(unassigned) ==
              navigator.bookItem()->childCount() - 1);
        CHECK(navigator.itemsInSection(QStringLiteral("unassigned_files")) ==
              "Chapter 2 – " + name.toStdString());
    }

    SECTION("The file of a chapter of an earlier book gets the title, the status and the notes "
            "of the chapter") {
        // Regression: a book made before Kalahari wrote them to the files has others there, so
        // the file under "Unassigned Files" went back to the book with another title
        TestTrash trash;
        const core::ProjectElement* bodyChapter = pm.findElement(bodyChapterId);
        REQUIRE(bodyChapter != nullptr);
        const QString file = pm.filePathOf(*bodyChapter);
        REQUIRE(pm.setStatus(bodyChapterId, QStringLiteral("revision")));
        REQUIRE(pm.setNotes(bodyChapterId, QStringLiteral("Check the dates")));
        std::optional<core::ChapterDocument> earlier = core::ChapterDocument::load(file);
        REQUIRE(earlier.has_value());
        earlier->setTitle(QStringLiteral("Chapter 9"));
        earlier->setStatus(QStringLiteral("draft"));
        earlier->setNotes(QString());
        REQUIRE(earlier->save(file));

        CHECK(answering([&] { navigator.coordinator.onRequestDelete(bodyChapterId); },
                        {answerDelete(true, false)}) == 1);
        const std::optional<core::ChapterDocument> kept = core::ChapterDocument::load(file);
        REQUIRE(kept.has_value());
        CHECK(kept->title() == QStringLiteral("Chapter 2"));
        CHECK(kept->status() == QStringLiteral("revision"));
        CHECK(kept->notes() == QStringLiteral("Check the dates"));
        CHECK(navigator.itemsInSection(QStringLiteral("unassigned_files")) ==
              "Chapter 2 – " + QFileInfo(file).fileName().toStdString());
    }

    SECTION("Deleting a part takes its chapters out of the book and closes their tabs") {
        TestTrash trash;
        const QString file = pm.filePathOf(chapter);
        const QString name = QFileInfo(file).fileName();

        // Not without the writer's word; the window lists the part with its chapter and its
        // file
        QString said;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(false, false, &said)}) == 1);
        INFO(said.toStdString());
        CHECK(said.contains(QStringLiteral("Delete the part \"Part I\" from the book?")));
        CHECK(said.contains(QStringLiteral(
            "Together with it, the chapter \"Chapter 1\" will be deleted from the book.")));
        CHECK(said.contains(QStringLiteral("Part I\n  Chapter 1 – ") + name));
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(navigator.tabs->count() == 1);
        CHECK(navigator.coordinator.isChapterDirty(chapter.id));

        // Deleted: the text of the chapter's tab goes to its file first
        auto* editor = qobject_cast<gui::EditorPanel*>(navigator.tabs->widget(0));
        REQUIRE(editor != nullptr);
        editor->setText(QStringLiteral("The storm came at night."));
        navigator.properties->showChapterProperties(chapter.id);
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(true)}) == 1);
        CHECK(titles(novel.book().mainElements) == "Chapter 2");
        CHECK(titles(novel.savedBook().mainElements) == "Chapter 2");
        CHECK(pm.findElement(chapter.id) == nullptr);
        CHECK(navigator.item(partId) == nullptr);
        CHECK(navigator.item(chapter.id) == nullptr);
        CHECK(navigator.tabs->count() == 0);
        CHECK_FALSE(navigator.coordinator.isChapterDirty(chapter.id));
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Part deleted from the book: Part I"));

        // The Properties panel showed the deleted chapter: it shows the book
        CHECK(navigator.properties->currentPage() == gui::PropertiesPanel::Page::Project);

        // The chapter's file stays in the book's folder, with the text of the tab
        CHECK(textOf(file).contains(QStringLiteral("The storm came at night.")));
        CHECK(trash.taken.isEmpty());
        CHECK(navigator.itemsInSection(QStringLiteral("unassigned_files")) ==
              "Chapter 1 – " + name.toStdString());
    }
}

TEST_CASE("Navigator commands: the files deleted with an element go to the Trash or for good",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    TestTrash trash;

    // A part with two chapters; the first one is open, with changes
    answering([&] { navigator.coordinator.onRequestAddPart(); }, {acceptAsItIs});
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString partId = novel.book().mainElements.first().id;
    answering([&] { navigator.coordinator.onRequestAddChapter(partId); }, {acceptAsItIs});
    answering([&] { navigator.coordinator.onRequestAddChapter(partId); }, {acceptAsItIs});
    const QList<core::ProjectElement> chapters = novel.book().mainElements.first().elements;
    REQUIRE(titles(chapters) == "Chapter 1, Chapter 2");
    const QStringList files{pm.filePathOf(chapters.at(0)), pm.filePathOf(chapters.at(1))};
    navigator.coordinator.onElementSelected(chapters.at(0).id, chapters.at(0).title);
    REQUIRE(navigator.tabs->count() == 1);
    auto* editor = qobject_cast<gui::EditorPanel*>(navigator.tabs->widget(0));
    REQUIRE(editor != nullptr);
    editor->setText(QStringLiteral("The storm came at night."));
    navigator.coordinator.setChapterDirty(chapters.at(0).id, true);

    SECTION("To the Trash, with the text of the open tab") {
        QString said;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(true, true, &said)}) == 1);
        INFO(said.toStdString());
        CHECK(said.contains(withBin(QStringLiteral(
            "The files of the chapters go to the system's {bin}. You can restore them from the "
            "{bin}, but not from the program."))));
        CHECK_FALSE(said.contains(QStringLiteral("Folder:")));
        CHECK(novel.book().mainElements.isEmpty());
        CHECK(novel.savedBook().mainElements.isEmpty());
        CHECK(trash.taken == files);
        CHECK(trash.texts.value(0).contains(QStringLiteral("The storm came at night.")));
        CHECK_FALSE(QFileInfo::exists(files.at(0)));
        CHECK_FALSE(QFileInfo::exists(files.at(1)));
        CHECK(navigator.tabs->count() == 0);
        CHECK(navigator.section(QStringLiteral("unassigned_files")) == nullptr);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Part deleted from the book: Part I"));
    }

    SECTION("For good from a folder without a Trash") {
        trash.exists = false;
        QString said;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(true, true, &said)}) == 1);
        INFO(said.toStdString());
        CHECK(said.contains(withBin(QStringLiteral(
            "The files of the chapters will be gone for good: files in this folder cannot go to "
            "the {bin}."))));
        CHECK(novel.book().mainElements.isEmpty());
        CHECK(trash.taken.isEmpty());
        CHECK_FALSE(QFileInfo::exists(files.at(0)));
        CHECK_FALSE(QFileInfo::exists(files.at(1)));
        CHECK(navigator.section(QStringLiteral("unassigned_files")) == nullptr);
    }

    SECTION("A file the Trash does not take goes for good only when the writer says so") {
        trash.takes = false;
        QString asked;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(chapters.at(1).id); },
                        {answerDelete(true, true), answerQuestion(false, &asked)}) == 2);
        INFO(asked.toStdString());
        CHECK(asked.contains(withBin(QStringLiteral("The {bin} did not take the file:\n")) +
                             QDir::toNativeSeparators(files.at(1))));
        CHECK(asked.contains(QStringLiteral("Delete it for good? It cannot be restored then.")));
        CHECK(titles(novel.book().mainElements.first().elements) == "Chapter 1");
        CHECK(QFileInfo::exists(files.at(1)));
        CHECK(navigator.itemsInSection(QStringLiteral("unassigned_files")) ==
              "Chapter 2 – " + QFileInfo(files.at(1)).fileName().toStdString());

        // The writer says so
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(chapters.at(0).id); },
                        {answerDelete(true, true), answerQuestion(true)}) == 2);
        CHECK(novel.book().mainElements.first().elements.isEmpty());
        CHECK_FALSE(QFileInfo::exists(files.at(0)));
        CHECK(navigator.tabs->count() == 0);
    }

    SECTION("The files the Trash does not take are listed, and the part shown goes too") {
        trash.takes = false;
        navigator.properties->showPartProperties(partId);
        QString asked;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(true, true), answerQuestion(false, &asked)}) == 2);
        INFO(asked.toStdString());
        CHECK(asked.contains(withBin(QStringLiteral("The {bin} did not take these files:\n  ")) +
                             QDir::toNativeSeparators(files.at(0)) + QStringLiteral("\n  ") +
                             QDir::toNativeSeparators(files.at(1))));
        CHECK(asked.contains(
            QStringLiteral("Delete them for good? They cannot be restored then.")));
        CHECK(novel.book().mainElements.isEmpty());
        CHECK(QFileInfo::exists(files.at(0)));
        CHECK(QFileInfo::exists(files.at(1)));
        CHECK(navigator.properties->currentPage() == gui::PropertiesPanel::Page::Project);
    }

    SECTION("An empty part has no files") {
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(chapters.at(0).id); },
                        {answerDelete(true)}) == 1);
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(chapters.at(1).id); },
                        {answerDelete(true)}) == 1);
        QString said;
        CHECK(answering([&] { navigator.coordinator.onRequestDelete(partId); },
                        {answerDelete(true, true, &said)}) == 1);
        INFO(said.toStdString());
        CHECK(said == QStringLiteral(
                          "Delete from Book\n\nDelete the part \"Part I\" from the book?\n\n"
                          "The part is empty.\n\nPart I"));
        CHECK(novel.book().mainElements.isEmpty());
        CHECK(trash.taken.isEmpty());
        CHECK(QFileInfo::exists(files.at(0)));
        CHECK(QFileInfo::exists(files.at(1)));
    }
}

TEST_CASE("Navigator: the files that no element has go back to the book or are deleted",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    auto& coordinator = navigator.coordinator;
    TestTrash trash;
    TestFileManager fileManager;
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();
    const auto unassigned = [&navigator]() {
        return navigator.section(QStringLiteral("unassigned_files"));
    };
    const auto fileItem = [&unassigned](const core::ProjectElement& element) {
        QTreeWidgetItem* files = unassigned();
        for (int i = 0; files && i < files->childCount(); ++i) {
            if (files->child(i)->data(0, Qt::UserRole).toString() == element.file) {
                return files->child(i);
            }
        }
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    const auto pathOf = [&novel](const core::ProjectElement& element) {
        return QDir::cleanPath(QDir(novel.dir.path()).filePath(element.file));
    };
    const auto deleteFromBook = [&coordinator](const QString& id) {
        REQUIRE(answering([&coordinator, &id] { coordinator.onRequestDelete(id); },
                          {answerDelete(true)}) == 1);
    };

    // A prologue and three chapters; the prologue, the second chapter (a revision with notes
    // and a text) and the third one go from the book, and their files stay
    for (int i = 0; i < 3; ++i) {
        answering([&] { coordinator.onRequestAddChapter(QString()); }, {acceptAsItIs});
    }
    const int prologueKind =
        indexOf(pm.textKindsFor(core::BookPlace::Main), QStringLiteral("prologue"));
    REQUIRE(prologueKind >= 0);
    answering([&] { coordinator.onRequestAddChapter(QString()); }, {acceptKind(prologueKind)});
    const QList<core::ProjectElement> start = novel.book().mainElements;
    REQUIRE(titles(start) == "Prologue, Chapter 1, Chapter 2, Chapter 3");
    const core::ProjectElement& prologue = start.at(0);
    const core::ProjectElement& first = start.at(1);
    const core::ProjectElement& second = start.at(2);
    const core::ProjectElement& third = start.at(3);
    REQUIRE(pm.setStatus(second.id, QStringLiteral("revision")));
    REQUIRE(pm.setNotes(second.id, QStringLiteral("Check the dates.")));
    pm.setChapterContent(second.id, QStringLiteral("<kml><p>The storm came at night.</p></kml>"));
    REQUIRE(pm.saveChapterContent(second.id));
    for (const core::ProjectElement* gone : {&prologue, &second, &third}) {
        deleteFromBook(gone->id);
    }
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1");

    // Listed by the names of their files, with their titles, and open: they are new
    REQUIRE(unassigned() != nullptr);
    CHECK(unassigned()->text(0) == QStringLiteral("Unassigned Files (3)"));
    CHECK(unassigned()->isExpanded());
    const auto shown = [](const core::ProjectElement& element) {
        return element.title + QStringLiteral(" – ") + QFileInfo(element.file).fileName();
    };
    CHECK(navigator.itemsInSection(QStringLiteral("unassigned_files")) ==
          QStringList{shown(second), shown(third), shown(prologue)}
              .join(QStringLiteral(", "))
              .toStdString());
    REQUIRE(fileItem(second) != nullptr);
    CHECK(fileItem(second)->toolTip(0) == QDir::toNativeSeparators(pathOf(second)));

    // The menus of a file and of the item of the files; a file shows its text when it is
    // opened
    QStringList requested;
    const auto request = [&requested](const char* what) {
        return [&requested, what](const QString& file) {
            requested << QString::fromLatin1(what) + QLatin1Char(' ') + file;
        };
    };
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestPreviewUnassigned,
                     request("preview"));
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAddUnassigned,
                     request("add"));
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestDeleteUnassigned,
                     request("delete"));
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestDeleteAllUnassigned,
                     [&requested]() { requested << QStringLiteral("delete all"); });
    CHECK(navigator.inMenuOf(fileItem(second), QStringLiteral("Preview")));
    CHECK(navigator.inMenuOf(fileItem(second), QStringLiteral("Add to Book...")));
    CHECK(navigator.inMenuOf(fileItem(second), QStringLiteral("Delete File...")));
    CHECK(navigator.inMenuOf(unassigned(), QStringLiteral("Delete All...")));
    auto* tree = navigator.panel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    emit tree->itemActivated(fileItem(second), 0);
    const QString file = second.file;
    CHECK(requested.join(QStringLiteral(", ")) ==
          QStringLiteral("preview %1, add %1, delete %1, delete all, preview %1").arg(file));
    CHECK(navigator.tabs->count() == 0);
    CHECK(navigator.inMenuOf(fileItem(second), QStringLiteral("Show in Folder")));
    CHECK(navigator.inMenuOf(unassigned(), QStringLiteral("Show in Folder")));
    CHECK(fileManager.shown ==
          QStringList{pathOf(second), QFileInfo(pathOf(second)).absolutePath()});

    // Closed by the writer, the item stays closed
    unassigned()->setExpanded(false);
    coordinator.refreshNavigator();
    CHECK_FALSE(unassigned()->isExpanded());

    // The preview: the text of the file, read-only, and what the element would take from it
    QString said;
    QString text;
    CHECK(answering([&] { coordinator.onRequestPreviewUnassigned(second.file); },
                    {[&said, &text](QDialog& dialog) {
                         if (auto* preview =
                                 qobject_cast<gui::dialogs::FilePreviewDialog*>(&dialog)) {
                             for (const QLabel* label : dialog.findChildren<QLabel*>()) {
                                 said += label->text() + QLatin1Char('\n');
                             }
                             text = preview->text()->toPlainText();
                             CHECK(preview->text()->isReadOnly());
                             CHECK(preview->cancelButton()->isDefault());
                         }
                         dialog.reject();
                     }}) == 1);
    INFO(said.toStdString());
    CHECK(said.contains(QStringLiteral("Chapter 2\n")));
    CHECK(said.contains(QDir::toNativeSeparators(second.file) +
                        QStringLiteral(" · Status: Revision · Words: 5")));
    CHECK(text == QStringLiteral("The storm came at night."));
    CHECK(titles(novel.book().mainElements) == "Chapter 1");

    // From the preview to the book: the kind its file's name says, its title and its status
    QString kind;
    QString title;
    QString status;
    CHECK(answering([&] { coordinator.onRequestPreviewUnassigned(second.file); },
                    {[](QDialog& dialog) {
                         if (auto* preview =
                                 qobject_cast<gui::dialogs::FilePreviewDialog*>(&dialog)) {
                             preview->acceptButton()->click();
                         } else {
                             dialog.reject();
                         }
                     },
                     [&kind, &title, &status](QDialog& dialog) {
                         auto* adding = qobject_cast<gui::dialogs::AddToBookDialog*>(&dialog);
                         if (!adding) {
                             dialog.reject();
                             return;
                         }
                         kind = adding->kindBox()->currentText();
                         title = adding->titleEdit()->text();
                         status = adding->statusLabel()->text();
                         CHECK(adding->copyButton() == nullptr);
                         adding->acceptButton()->click();
                     }}) == 2);
    CHECK(kind == QStringLiteral("Chapter"));
    CHECK(title == QStringLiteral("Chapter 2"));
    CHECK(status == QStringLiteral("Revision (from the file)"));

    // Back in the book at the end of the body, with the status and the notes of the file
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1, Chapter 2");
    const core::ProjectElement back = novel.book().mainElements.at(1);
    CHECK(back.file == second.file);
    CHECK(back.status == QStringLiteral("revision"));
    CHECK(pm.notes(back.id) == QStringLiteral("Check the dates."));
    CHECK(titles(novel.savedBook().mainElements) == "Chapter 1, Chapter 2");
    CHECK(navigator.current() == back.id);
    CHECK(navigator.statusBar->currentMessage() ==
          QStringLiteral("Chapter added to the book: Chapter 2"));
    CHECK(unassigned()->text(0) == QStringLiteral("Unassigned Files (2)"));
    CHECK_FALSE(unassigned()->isExpanded());
    CHECK(fileItem(second) == nullptr);

    // The prologue goes back as a prologue, with the writer's title, which its file takes
    CHECK(answering([&] { coordinator.onRequestAddUnassigned(prologue.file); },
                    {[&kind](QDialog& dialog) {
                         auto* adding = qobject_cast<gui::dialogs::AddToBookDialog*>(&dialog);
                         if (!adding) {
                             dialog.reject();
                             return;
                         }
                         kind = adding->kindBox()->currentText();
                         adding->titleEdit()->setText(QStringLiteral(" Before "));
                         adding->acceptButton()->click();
                     }}) == 1);
    CHECK(kind == QStringLiteral("Prologue"));
    CHECK(titles(novel.book().mainElements) == "Before, Chapter 1, Chapter 2");
    const std::optional<core::ChapterDocument> renamed =
        core::ChapterDocument::load(pathOf(prologue));
    CHECK((renamed && renamed->title() == QStringLiteral("Before")));

    // From the preview to deleting the file: it goes to the Trash
    said.clear();
    CHECK(answering([&] { coordinator.onRequestPreviewUnassigned(third.file); },
                    {[](QDialog& dialog) {
                         if (auto* preview =
                                 qobject_cast<gui::dialogs::FilePreviewDialog*>(&dialog)) {
                             preview->deleteButton()->click();
                         } else {
                             dialog.reject();
                         }
                     },
                     answerDeleteFiles(true, &said)}) == 2);
    const QString thirdName = QFileInfo(third.file).fileName();
    CHECK(said.contains(QStringLiteral("Delete the file \"") + thirdName +
                        QStringLiteral("\"?")));
    CHECK(said.contains(QStringLiteral("Chapter 3 – ") + thirdName));
    CHECK(said.contains(withBin(QStringLiteral(
        "The file goes to the system's {bin}. You can restore it from the {bin}, but not from "
        "the program."))));
    CHECK(trash.taken == QStringList{pathOf(third)});
    CHECK(unassigned() == nullptr);
    CHECK(navigator.statusBar->currentMessage() ==
          withBin(QStringLiteral("File moved to the {bin}: ")) + thirdName);

    // More files open the item
    for (const QString& id : {novel.book().mainElements.at(0).id, first.id, back.id}) {
        deleteFromBook(id);
    }
    REQUIRE(unassigned() != nullptr);
    CHECK(unassigned()->text(0) == QStringLiteral("Unassigned Files (3)"));
    CHECK(unassigned()->isExpanded());

    // A file deleted outside the program: the Navigator shows the files there are
    REQUIRE(QFile::remove(pathOf(prologue)));
    QString asked;
    CHECK(answering([&] { coordinator.onRequestAddUnassigned(prologue.file); },
                    {answerQuestion(true, &asked)}) == 1);
    CHECK(asked.contains(QStringLiteral("The file could not be read:\n") +
                         QDir::toNativeSeparators(pathOf(prologue))));
    CHECK(unassigned()->text(0) == QStringLiteral("Unassigned Files (2)"));
    CHECK(answering([&] { coordinator.onRequestDeleteUnassigned(prologue.file); }) == 0);

    // All of them, from a folder without a Trash: gone for good, when the writer says so
    trash.exists = false;
    said.clear();
    CHECK(answering([&] { coordinator.onRequestDeleteAllUnassigned(); },
                    {answerDeleteFiles(false, &said)}) == 1);
    INFO(said.toStdString());
    CHECK(said.contains(QStringLiteral("Delete all unassigned files (2)?")));
    CHECK(said.contains(withBin(QStringLiteral(
        "The files will be gone for good: files in this folder cannot go to the {bin}."))));
    CHECK(unassigned()->text(0) == QStringLiteral("Unassigned Files (2)"));
    CHECK(answering([&] { coordinator.onRequestDeleteAllUnassigned(); },
                    {answerDeleteFiles(true)}) == 1);
    CHECK(unassigned() == nullptr);
    CHECK_FALSE(QFileInfo::exists(pathOf(first)));
    CHECK_FALSE(QFileInfo::exists(pathOf(second)));
    CHECK(trash.taken == QStringList{pathOf(third)});
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Files deleted: 2"));
    CHECK(pm.unassignedFiles().isEmpty());
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
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part...")));
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Chapter...")));
    CHECK(navigator.inMenuOf(navigator.bookItem(),
                             QStringLiteral("Add Item at the Beginning of the Book...")));
    CHECK(navigator.inMenuOf(navigator.bookItem(),
                             QStringLiteral("Add Item at the End of the Book...")));
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
    CHECK_FALSE(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part..."), false));

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
    navigator.panel->resize(400, 600);
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
        if (field->placeholderText() == QStringLiteral("Enter the title of the book")) {
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

TEST_CASE("Properties panel: leaving a field without a change saves nothing",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    gui::PropertiesPanel* properties = navigator.properties;
    QObject::connect(properties, &gui::PropertiesPanel::requestSections, &navigator.coordinator,
                     &gui::NavigatorCoordinator::onRequestSections);
    const QString chapterId = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                            QStringLiteral("The Storm"), core::BookPlace::Main);
    REQUIRE_FALSE(chapterId.isEmpty());

    // The writer's own names of the sections
    properties->showProjectProperties();
    auto* sections = properties->findChild<QComboBox*>(QStringLiteral("projectSectionsCombo"));
    auto* namesRow = properties->findChild<QWidget*>(QStringLiteral("projectSectionNames"));
    REQUIRE(sections != nullptr);
    REQUIRE(namesRow != nullptr);
    sections->setCurrentIndex(
        sections->findData(QString::fromLatin1(core::ProjectBook::CUSTOM_SECTIONS)));
    REQUIRE(novel.book().sections().set == QLatin1String(core::ProjectBook::CUSTOM_SECTIONS));

    // A save would write the file of the book again
    const QString manifest =
        QDir(novel.dir.path()).filePath(QStringLiteral("Navigator Test.klh"));
    {
        QFile file(manifest);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write("not saved") == 9);
    }
    const auto saved = [&manifest]() {
        QFile file(manifest);
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll() != "not saved";
    };
    const auto lineWithText = [properties](const QString& text) -> QLineEdit* {
        for (QLineEdit* edit : properties->findChildren<QLineEdit*>()) {
            if (edit->isVisibleTo(properties) && edit->text() == text) {
                return edit;
            }
        }
        return nullptr;
    };

    QLineEdit* title = lineWithText(QStringLiteral("Navigator Test"));
    QLineEdit* author = lineWithText(QStringLiteral("Author"));
    REQUIRE(title != nullptr);
    REQUIRE(author != nullptr);
    emit title->editingFinished();
    emit author->editingFinished();
    REQUIRE(QMetaObject::invokeMethod(namesRow, "editingFinished"));
    properties->showChapterProperties(chapterId);
    QLineEdit* chapterTitle = lineWithText(QStringLiteral("The Storm"));
    REQUIRE(chapterTitle != nullptr);
    emit chapterTitle->editingFinished();
    CHECK_FALSE(saved());

    // A changed name is saved
    properties->showProjectProperties();
    const QList<QLineEdit*> names = namesRow->findChildren<QLineEdit*>();
    REQUIRE(names.size() == 3);
    names.at(1)->setText(QStringLiteral("The Middle"));
    REQUIRE(QMetaObject::invokeMethod(namesRow, "editingFinished"));
    CHECK(saved());
}
