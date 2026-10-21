/// @file test_navigator_coordinator.cpp
/// @brief The Navigator's commands on the open book project: adding, renaming, moving and
/// deleting elements, which ProjectManager saves at once, and the files that no element has

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/art_provider.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/file_trash.h>
#include <kalahari/core/kind_words.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/gui/dialogs/add_to_book_dialog.h>
#include <kalahari/gui/dialogs/delete_dialogs.h>
#include <kalahari/gui/dialogs/file_preview_dialog.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/new_file_dialog.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/material_downloader.h>
#include <kalahari/gui/navigator_coordinator.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/image_preview_panel.h>
#include <kalahari/gui/panels/material_page.h>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/gui/panels/properties_panel.h>
#include <kalahari/gui/paste_target.h>
#include <kalahari/gui/utils/access_keys.h>
#include <kalahari/gui/utils/file_location.h>

#include "context_menu_reader.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QChildEvent>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QUrl>

#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;

namespace {

/// @brief What answers a dialog a command shows
using Answer = std::function<void(QDialog&)>;

/// @brief Write @p text in place of the text of the editor of tab @p index, as the writer
/// does: its text differs from the saved text then
void write(const QTabWidget& tabs, int index, const QString& text) {
    auto* panel = qobject_cast<gui::EditorPanel*>(tabs.widget(index));
    REQUIRE(panel != nullptr);
    panel->getBookEditor()->replaceWithKml(editor::ClipboardHandler::textToKml(text));
}

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

/// @brief What the writer chooses in the Add Element window
struct AddChoice {
    QString kind = {};   ///< The id of the kind of the tile ("prologue"); empty: the first one
    QString title = {};  ///< The title; empty: the one the window proposes
    std::optional<core::ElementPlace> place = {};  ///< The place; none: the one proposed
    QString status = {};  ///< "draft", "revision" or "final"; empty: the one proposed
    bool take = true;  ///< A new group takes the elements that close the main section
};

/// @brief The window of files that chooses the file of a new material, in place of the
/// system's until the test ends: it chooses @c file
struct TestFileChooser {
    QString file;         ///< The file it chooses; empty: Cancel
    QStringList folders;  ///< The folders it was opened in

    explicit TestFileChooser(QString chosen) : file(std::move(chosen)) {
        gui::dialogs::NewFileDialog::setFileChooserForTests([this](const QString& folder) {
            folders << folder;
            return file;
        });
    }
    ~TestFileChooser() { gui::dialogs::NewFileDialog::setFileChooserForTests({}); }
    TestFileChooser(const TestFileChooser&) = delete;
    TestFileChooser& operator=(const TestFileChooser&) = delete;
};

/// @brief Answer the Add Element window with @p choice and Add; another window, or a choice
/// the window does not have, is rejected
Answer addAs(const AddChoice& choice = {}) {
    return [choice](QDialog& dialog) {
        auto* window = qobject_cast<gui::dialogs::NewFileDialog*>(&dialog);
        if (!window) {
            dialog.reject();
            return;
        }
        if (!choice.kind.isEmpty()) {
            // The first tile of the kind that can be chosen; the names of the parts of the book
            // have no kind
            const auto chooseTile = [window, &choice]() {
                QListWidget* tiles = window->kindList();
                for (int row = 0; row < tiles->count(); ++row) {
                    QListWidgetItem* tile = tiles->item(row);
                    if (!tile->data(Qt::UserRole).isValid() ||
                        !tile->flags().testFlag(Qt::ItemIsEnabled)) {
                        continue;
                    }
                    tiles->setCurrentItem(tile);
                    if (window->kind() && window->kind().kind->id == choice.kind) {
                        return true;
                    }
                }
                return false;
            };
            // Among the tiles shown, else those of the other choices at the top of the window
            bool found = chooseTile();
            if (QButtonGroup* choices = window->chooserButtons(); !found && choices) {
                for (QAbstractButton* button : choices->buttons()) {
                    button->click();
                    found = chooseTile();
                    if (found) {
                        break;
                    }
                }
            }
            if (!found) {
                dialog.reject();
                return;
            }
        }
        // A material gets its file from the window of files of the test (TestFileChooser)
        if (window->kind() && window->kind().kind->isMaterial() &&
            window->materialFile().isEmpty()) {
            window->chooseButton()->click();
        }
        if (!choice.title.isEmpty()) {
            window->titleEdit()->setText(choice.title);
        }
        if (choice.place) {
            window->picker()->setPlace(*choice.place);
        }
        if (!choice.status.isEmpty()) {
            window->statusBox()->setCurrentIndex(window->statusBox()->findData(choice.status));
        }
        window->takeBox()->setChecked(choice.take);
        if (window->createButton()->isEnabled()) {
            window->createButton()->click();
        } else {
            dialog.reject();
        }
    };
}

/// @brief Add Element on section @p place of the book
gui::AddTarget onSection(core::BookPlace place) {
    gui::AddTarget target;
    target.scope = gui::AddTarget::Scope::Section;
    target.place = place;
    return target;
}

/// @brief Add Element on group @p groupId of the book (a part)
gui::AddTarget onGroup(const QString& groupId) {
    gui::AddTarget target;
    target.scope = gui::AddTarget::Scope::Group;
    target.id = groupId;
    return target;
}

/// @brief Add Element on element @p elementId of the book: right after it
gui::AddTarget onElement(const QString& elementId) {
    gui::AddTarget target;
    target.scope = gui::AddTarget::Scope::Element;
    target.id = elementId;
    return target;
}

/// @brief Add Element on the Workshop: all its kinds
gui::AddTarget onWorkshop() {
    gui::AddTarget target;
    target.scope = gui::AddTarget::Scope::Workshop;
    return target;
}

/// @brief Kind @p kindId of a novel: of the novel package, else of the base package
core::KindRef novelKind(const QString& kindId) {
    const core::BookTypeRegistry& types = core::ProjectManager::getInstance().bookTypes();
    const core::KindRef kind = types.findKind(QStringLiteral("kalahari.novel"), kindId);
    return kind ? kind : types.findKind(QStringLiteral("kalahari.base"), kindId);
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

/// @brief The programs that open files, in place of the system's until the test ends: no
/// program starts
struct TestFileOpener {
    bool opens = true;   ///< The files open; else the system cannot open them
    QStringList opened;  ///< The files the program asked to open

    TestFileOpener() {
        gui::setFileOpenerForTests([this](const QString& path) {
            opened << path;
            return opens;
        });
    }
    ~TestFileOpener() { gui::setFileOpenerForTests({}); }
    TestFileOpener(const TestFileOpener&) = delete;
    TestFileOpener& operator=(const TestFileOpener&) = delete;
};

/// @brief Close the program's message window; @p said gets what it says, with its title
Answer answerMessage(QString* said) {
    return [said](QDialog& dialog) {
        if (auto* message = qobject_cast<gui::dialogs::MessageDialog*>(&dialog)) {
            *said = message->clipboardText();
        }
        dialog.reject();
    };
}

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

    /// The project as its .klh file has it
    core::BookProject savedProject() const {
        QStringList problems;
        const std::optional<core::BookProject> saved = core::BookProject::load(
            QDir(dir.path()).filePath(QStringLiteral("Navigator Test.klh")), problems);
        REQUIRE(saved.has_value());
        return *saved;
    }

    /// The book as its .klh file has it
    core::ProjectBook savedBook() const {
        const core::BookProject saved = savedProject();
        REQUIRE(saved.books.size() == 1);
        return saved.books.first();
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

    /// Add Element on @p target, its window answered with @p answers
    /// @return The number of windows shown
    int add(const gui::AddTarget& target, QList<Answer> answers = {}) {
        return answering([this, &target] { coordinator.onRequestAdd(target); },
                         std::move(answers));
    }

    /// Add an element of kind @p kindId (novelKind()) to @p place or group @p groupId, with the
    /// title and at the place that Add Element proposes, without its window, and show the book
    /// as the Navigator does after adding: the commands of the tests start from it
    QString added(const QString& kindId, core::BookPlace place = core::BookPlace::Main,
                  const QString& groupId = QString()) {
        auto& pm = core::ProjectManager::getInstance();
        const core::KindRef kind = novelKind(kindId);
        REQUIRE(kind);
        const QString id = pm.addElement(kind, pm.defaultTitle(kind), place, groupId);
        REQUIRE_FALSE(id.isEmpty());
        coordinator.refreshNavigator();
        return id;
    }

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

    /// The item of the project, with the book and the Workshop under it
    QTreeWidgetItem* projectItem() const {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(tree->topLevelItemCount() > 0);
        return tree->topLevelItem(0);
    }

    /// The item of the book
    QTreeWidgetItem* bookItem() const {
        QTreeWidgetItem* book = section(QStringLiteral("book"));
        REQUIRE(book != nullptr);
        return book;
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
                // The text without its access key ("Mo&ve..." is "Move...") and its keys
                const bool named =
                    gui::withoutAccessKey(action->text().section(QLatin1Char('\t'), 0, 0)) == text;
                if (toolTip && named) {
                    *toolTip = action->toolTip();
                }
                if (named && action->isEnabled()) {
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

    /// The texts of the actions of the menu of @p found, without their access keys and keys;
    /// @p bold gets the texts of the actions in bold
    QStringList menuTexts(QTreeWidgetItem* found, QStringList* bold = nullptr) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(found != nullptr);
        tree->scrollToItem(found);
        QStringList texts;
        MenuReader reader(panel, [&texts, bold](QMenu& menu) {
            for (QAction* action : menu.actions()) {
                if (action->isSeparator()) {
                    continue;
                }
                const QString text =
                    gui::withoutAccessKey(action->text().section(QLatin1Char('\t'), 0, 0));
                texts << text;
                if (bold && action->font().bold()) {
                    *bold << text;
                }
            }
        });
        emit tree->customContextMenuRequested(tree->visualItemRect(found).center());
        return texts;
    }

    /// The commands of the menu of @p found that add: "Add..." and those named after kinds
    QStringList addCommandsOf(QTreeWidgetItem* found) {
        QStringList adding;
        for (const QString& text : menuTexts(found)) {
            if (text.startsWith(QStringLiteral("Add "), Qt::CaseSensitive) ||
                text == QStringLiteral("Add...")) {
                adding << text;
            }
        }
        return adding;
    }

    /// Press @p key with @p modifiers in the tree, on its current item
    /// @return Whether the tree took the key before the window's commands
    bool press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        QKeyEvent override(QEvent::ShortcutOverride, key, modifiers);
        QCoreApplication::sendEvent(tree, &override);
        QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
        QCoreApplication::sendEvent(tree, &pressed);
        return override.isAccepted();
    }

    /// Make @p found the current item of the tree
    void select(QTreeWidgetItem* found) {
        auto* tree = panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        REQUIRE(found != nullptr);
        tree->setCurrentItem(found);
    }

    /// The "+" button of the Navigator's bar
    QToolButton* addButton() const {
        QToolButton* plus = nullptr;
        for (QToolButton* button : panel->findChildren<QToolButton*>()) {
            if (button->toolTip().startsWith(QStringLiteral("Add... ("))) {
                plus = button;
            }
        }
        REQUIRE(plus != nullptr);
        return plus;
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

} // namespace

TEST_CASE("Navigator commands: Add Element adds parts, chapters and items of the kinds chosen, "
          "saved at once", "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;

    // A part, with the title of its kind; it has no text to open
    CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs({.kind = "part"})}) == 1);
    REQUIRE(novel.book().mainElements.size() == 1);
    const core::ProjectElement part = novel.book().mainElements.first();
    CHECK(part.title == QStringLiteral("Part I"));
    CHECK(part.kind.toString() == QStringLiteral("kalahari.base:part"));
    CHECK(navigator.current() == part.id);
    CHECK(navigator.tabs->count() == 0);
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Part added: Part I"));

    // A chapter in the part: the type's main kind, numbered, a draft; it opens in the editor
    // with the cursor at the start of its text
    CHECK(navigator.add(onGroup(part.id), {addAs()}) == 1);
    REQUIRE(novel.book().mainElements.first().elements.size() == 1);
    const core::ProjectElement chapter = novel.book().mainElements.first().elements.first();
    CHECK(chapter.title == QStringLiteral("Chapter 1"));
    CHECK(chapter.kind.toString() == QStringLiteral("kalahari.base:chapter"));
    CHECK(chapter.status == QStringLiteral("draft"));
    CHECK(QFileInfo::exists(pm.filePathOf(chapter)));
    CHECK(navigator.current() == chapter.id);
    REQUIRE(navigator.tabs->count() == 1);
    auto* editor = qobject_cast<gui::EditorPanel*>(navigator.tabs->currentWidget());
    REQUIRE(editor != nullptr);
    CHECK(editor->property("elementId").toString() == chapter.id);
    CHECK(editor->getBookEditor()->cursorPosition() == editor::CursorPosition{});

    // A prologue in the body itself, chosen from the kinds of the section, with its status;
    // its title follows it, and it opens the body
    CHECK(navigator.add(onSection(core::BookPlace::Main),
                        {addAs({.kind = "prologue", .status = "revision"})}) == 1);
    CHECK(titles(novel.book().mainElements) == "Prologue, Part I");
    CHECK(novel.book().mainElements.first().kind.toString() ==
          QStringLiteral("kalahari.novel:prologue"));
    CHECK(novel.book().mainElements.first().status == QStringLiteral("revision"));

    // After an element: its kind, right after it
    CHECK(navigator.add(onElement(chapter.id), {addAs()}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 1, Chapter 2");

    // An item of the front section, with a title of its own
    CHECK(navigator.add(onSection(core::BookPlace::Front),
                        {addAs({.title = "For Anna"})}) == 1);
    CHECK(titles(novel.book().frontElements) == "For Anna");

    // A window closed without adding adds nothing
    CHECK(navigator.add(onSection(core::BookPlace::Main)) == 1);
    CHECK(titles(novel.book().mainElements) == "Prologue, Part I");

    // The project is saved at once, and the navigator shows it
    CHECK(titles(novel.savedBook().mainElements) == "Prologue, Part I");
    CHECK(titles(novel.savedBook().mainElements.last().elements) == "Chapter 1, Chapter 2");
    CHECK(titles(novel.savedBook().frontElements) == "For Anna");
    CHECK(navigator.itemsInSection(QStringLiteral("section_body")) ==
          "Prologue [Revision], Part I");
    CHECK(navigator.itemsIn(part.id) == "Chapter 1 [Draft], Chapter 2 [Draft]");
    CHECK(navigator.itemsInSection(QStringLiteral("section_frontmatter")) == "For Anna [Draft]");
}

TEST_CASE("Navigator commands: Add Element on the Workshop adds working notes and materials, "
          "saved at once", "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    TestFileOpener opener;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString picture = QDir(files.path()).filePath(QStringLiteral("map.png"));
    {
        QFile file(picture);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write("PNG") == 3);
    }
    TestFileChooser chooser(picture);

    // The Workshop has the groups of the kinds the program makes, also empty
    CHECK(navigator.itemsInSection(QStringLiteral("workshop")) == "Working notes, Materials");

    // A working note has no status; it opens in the editor
    CHECK(navigator.add(onWorkshop(), {addAs({.kind = "work_note"})}) == 1);
    REQUIRE(pm.project()->workshop.elements.size() == 1);
    const core::ProjectElement note = pm.project()->workshop.elements.first();
    CHECK(note.title == QStringLiteral("Note 1"));
    CHECK(note.kind.toString() == QStringLiteral("kalahari.base:work_note"));
    CHECK(QFileInfo::exists(pm.filePathOf(note)));
    CHECK(navigator.current() == note.id);
    REQUIRE(navigator.tabs->count() == 1);
    CHECK(navigator.tabs->currentWidget()->property("elementId").toString() == note.id);
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Working note added: Note 1"));

    // With the note in the editor, Add offers its kind only
    const gui::AddTarget here = navigator.coordinator.addTargetHere();
    CHECK(here.scope == gui::AddTarget::Scope::Workshop);
    REQUIRE(here.kinds.size() == 1);
    CHECK(here.kinds.first().reference() == QStringLiteral("kalahari.base:work_note"));
    CHECK(answering([&navigator] { navigator.coordinator.onRequestAddHere(); },
                    {addAs({.title = "Ideas"})}) == 1);
    CHECK(titles(pm.project()->workshop.elements) == "Note 1, Ideas");

    // A material is a copy of the file chosen, named after it; it has no text to open, and its
    // card opens in the middle of the window, ready for its description
    CHECK(navigator.add(onWorkshop(), {addAs({.kind = "material"})}) == 1);
    REQUIRE(pm.project()->workshop.elements.size() == 3);
    const core::ProjectElement map = pm.project()->workshop.elements.last();
    CHECK(map.title == QStringLiteral("map"));
    CHECK(map.kind.toString() == QStringLiteral("kalahari.base:material"));
    CHECK(map.status.isEmpty());
    CHECK(QFileInfo(pm.filePathOf(map)).fileName() == QStringLiteral("map.png"));
    CHECK(QFileInfo::exists(pm.filePathOf(map)));
    CHECK(QFileInfo::exists(picture));
    CHECK(navigator.current() == map.id);
    REQUIRE(navigator.tabs->count() == 3);
    auto* card = qobject_cast<gui::MaterialCardPanel*>(navigator.tabs->currentWidget());
    REQUIRE(card != nullptr);
    CHECK(card->elementId() == map.id);
    CHECK(navigator.tabs->tabText(2) == QStringLiteral("map"));
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Material added: map"));

    // Saved at once, and shown in the groups of their kinds
    CHECK(titles(novel.savedProject().workshop.elements) == "Note 1, Ideas, map");
    REQUIRE(navigator.item(note.id) != nullptr);
    CHECK(navigator.item(note.id)->text(0) == QStringLiteral("Note 1"));
    CHECK(navigator.item(note.id)->parent()->text(0) == QStringLiteral("Working notes"));
    REQUIRE(navigator.item(map.id) != nullptr);
    CHECK(navigator.item(map.id)->parent()->text(0) == QStringLiteral("Materials"));

    // A picture opens on its preview tab, which says why it shows none of this file; opened
    // again, its tab comes to the front
    auto& settings = core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(gui::NavigatorPanel::OPEN_MATERIALS_SETTING, "card");
    settings.set<std::string>(gui::NavigatorPanel::OPEN_MATERIALS_SETTING, "card");
    navigator.coordinator.onRequestOpenMaterial(map.id);
    CHECK(opener.opened.isEmpty());
    REQUIRE(navigator.tabs->count() == 4);
    auto* preview = qobject_cast<gui::ImagePreviewPanel*>(navigator.tabs->currentWidget());
    REQUIRE(preview != nullptr);
    CHECK(preview->elementId() == map.id);
    CHECK(preview->property("elementId").toString() == map.id);
    CHECK(navigator.tabs->tabText(3) == QStringLiteral("map"));
    CHECK_FALSE(preview->hasPicture());
    CHECK(preview->problem().startsWith(QStringLiteral("The picture cannot be read (")));
    navigator.tabs->setCurrentIndex(0);
    navigator.coordinator.onRequestOpenMaterial(map.id);
    CHECK(navigator.tabs->count() == 4);
    CHECK(navigator.tabs->currentWidget() == preview);

    // As the settings say, it opens in the system's program for its file
    settings.set<std::string>(gui::NavigatorPanel::OPEN_MATERIALS_SETTING, "system");
    navigator.coordinator.onRequestOpenMaterial(map.id);
    CHECK(opener.opened == QStringList{pm.filePathOf(map)});
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Opened: map"));

    // A file the system cannot open: the writer is told which
    opener.opens = false;
    QString said;
    CHECK(answering([&navigator, &map] { navigator.coordinator.onRequestOpenMaterial(map.id); },
                    {answerMessage(&said)}) == 1);
    CHECK(said == QStringLiteral("Open Material Failed\n\nThe system could not open the file:\n") +
                      QDir::toNativeSeparators(pm.filePathOf(map)));
    settings.set<std::string>(gui::NavigatorPanel::OPEN_MATERIALS_SETTING, before);

    // Deleted from the book, the material takes its preview tab and its card with it
    CHECK(answering([&navigator, &map] { navigator.coordinator.onRequestDelete(map.id); },
                    {answerDelete(true, false)}) == 1);
    CHECK(pm.findElement(map.id) == nullptr);
    CHECK(navigator.tabs->count() == 2);
}

TEST_CASE("Navigator commands: Add and New Chapter start from the element in the editor",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;

    // Nothing in the editor: the whole book, with a new chapter at the end of the body
    CHECK(navigator.coordinator.addTargetHere().scope == gui::AddTarget::Scope::Book);
    CHECK(answering([&navigator] { navigator.coordinator.onRequestNewChapter(); },
                    {addAs()}) == 1);
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString first = novel.book().mainElements.first().id;
    CHECK(novel.book().mainElements.first().kind.toString() ==
          QStringLiteral("kalahari.base:chapter"));
    REQUIRE(navigator.tabs->count() == 1);
    CHECK(navigator.tabs->currentWidget()->property("elementId").toString() == first);

    // The chapter in the editor: right after it
    const gui::AddTarget here = navigator.coordinator.addTargetHere();
    CHECK(here.scope == gui::AddTarget::Scope::Element);
    CHECK(here.id == first);
    const QString last = navigator.added(QStringLiteral("chapter"));
    CHECK(answering([&navigator] { navigator.coordinator.onRequestNewChapter(); },
                    {addAs()}) == 1);
    REQUIRE(novel.book().mainElements.size() == 3);
    const QString second = novel.book().mainElements.at(1).id;
    CHECK(novel.book().mainElements.first().id == first);
    CHECK(novel.book().mainElements.last().id == last);
    CHECK(navigator.tabs->currentWidget()->property("elementId").toString() == second);

    // Add proposes the kind of the element in the editor, right after it
    CHECK(answering([&navigator] { navigator.coordinator.onRequestAddHere(); },
                    {addAs({.title = "Interlude"})}) == 1);
    REQUIRE(novel.book().mainElements.size() == 4);
    CHECK(novel.book().mainElements.at(2).title == QStringLiteral("Interlude"));
    CHECK(novel.book().mainElements.at(2).kind.toString() ==
          QStringLiteral("kalahari.base:chapter"));

    // An item of the front section in the editor: a chapter cannot follow it there, so the
    // new chapter goes to the end of the body
    const QString dedication =
        navigator.added(QStringLiteral("dedication"), core::BookPlace::Front);
    navigator.coordinator.onElementSelected(dedication, QStringLiteral("Dedication"));
    CHECK(answering([&navigator] { navigator.coordinator.onRequestNewChapter(); },
                    {addAs()}) == 1);
    REQUIRE(novel.book().mainElements.size() == 5);
    CHECK(novel.book().mainElements.at(3).id == last);
    CHECK(navigator.tabs->currentWidget()->property("elementId").toString() ==
          novel.book().mainElements.last().id);
    CHECK(titles(novel.book().frontElements) == titles(novel.savedBook().frontElements));
    CHECK(titles(novel.book().mainElements) == titles(novel.savedBook().mainElements));
}

TEST_CASE("Navigator commands: the book and the Workshop are named and the Workshop grouped, "
          "saved at once", "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& pm = novel.pm;
    const auto renameNode = [&navigator](const QString& node, QList<Answer> answers) {
        return answering([&navigator, &node] { navigator.coordinator.onRequestRenameNode(node); },
                         std::move(answers));
    };
    const auto workshopItem = [&navigator] {
        QTreeWidgetItem* item = navigator.section(QStringLiteral("workshop"));
        REQUIRE(item != nullptr);
        return item;
    };

    // Names of the writer's own, shown in the Navigator
    CHECK(renameNode(QStringLiteral("book"), {acceptWith(QStringLiteral("The Manuscript"))}) == 1);
    CHECK(novel.book().name == QStringLiteral("The Manuscript"));
    CHECK(novel.savedBook().name == QStringLiteral("The Manuscript"));
    CHECK(navigator.bookItem()->text(0) == QStringLiteral("The Manuscript"));
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Renamed to 'The Manuscript'"));
    CHECK(renameNode(QStringLiteral("workshop"), {acceptWith(QStringLiteral("Atelier"))}) == 1);
    CHECK(pm.project()->workshop.name == QStringLiteral("Atelier"));
    CHECK(novel.savedProject().workshop.name == QStringLiteral("Atelier"));
    CHECK(workshopItem()->text(0) == QStringLiteral("Atelier"));

    // A window closed without a change changes nothing
    CHECK(renameNode(QStringLiteral("book"), {}) == 1);
    CHECK(novel.savedBook().name == QStringLiteral("The Manuscript"));

    // The usual name is no name of the writer's own: it follows the language of the program
    CHECK(renameNode(QStringLiteral("book"), {acceptWith(QStringLiteral("Book"))}) == 1);
    CHECK(novel.book().name.isEmpty());
    CHECK(novel.savedBook().name.isEmpty());
    CHECK(navigator.bookItem()->text(0) == QStringLiteral("Book"));
    CHECK(renameNode(QStringLiteral("workshop"), {acceptWith(QStringLiteral("Workshop"))}) == 1);
    CHECK(pm.project()->workshop.name.isEmpty());
    CHECK(novel.savedProject().workshop.name.isEmpty());
    CHECK(workshopItem()->text(0) == QStringLiteral("Workshop"));

    // Grouped, the materials are in Resources
    navigator.coordinator.onRequestWorkshopGrouped(true);
    CHECK(pm.project()->workshop.grouped);
    CHECK(novel.savedProject().workshop.grouped);
    CHECK(navigator.itemsInSection(QStringLiteral("workshop")) == "Working notes, Resources");
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Workshop grouped"));
    navigator.coordinator.onRequestWorkshopGrouped(false);
    CHECK_FALSE(pm.project()->workshop.grouped);
    CHECK_FALSE(novel.savedProject().workshop.grouped);
    CHECK(navigator.itemsInSection(QStringLiteral("workshop")) == "Working notes, Materials");
    CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Workshop not grouped"));
}

TEST_CASE("Navigator: the keys and the \"+\" button add, rename and delete on the current "
          "node",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    const QString chapter = navigator.added(QStringLiteral("chapter"));
    QList<gui::AddTarget> added;
    QStringList renamed;
    QStringList deleted;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAdd,
                     [&added](const gui::AddTarget& target) { added << target; });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestRename,
                     [&renamed](const QString& id, const QString&) { renamed << id; });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestRenameNode,
                     [&renamed](const QString& node) { renamed << node; });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestRenameSection,
                     [&renamed](const QString& section) { renamed << section; });
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestDelete,
                     [&deleted](const QString& id) { deleted << id; });

    // On an element: Insert and Ctrl+Shift+Enter add right after it, F2 renames it, Delete
    // deletes it; the tree takes the keys before the window's commands with them
    navigator.select(navigator.item(chapter));
    CHECK(navigator.press(Qt::Key_Insert));
    CHECK(navigator.press(Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier));
    CHECK(navigator.press(Qt::Key_Enter, Qt::ControlModifier | Qt::ShiftModifier));
    REQUIRE(added.size() == 3);
    for (const gui::AddTarget& target : added) {
        CHECK(target.scope == gui::AddTarget::Scope::Element);
        CHECK(target.id == chapter);
    }
    CHECK(navigator.press(Qt::Key_F2));
    CHECK(navigator.press(Qt::Key_Delete));
    CHECK(renamed == QStringList{chapter});
    CHECK(deleted == QStringList{chapter});
    // Other keys stay the tree's and the window's
    CHECK_FALSE(navigator.press(Qt::Key_Delete, Qt::ShiftModifier));
    CHECK_FALSE(navigator.press(Qt::Key_Insert, Qt::ControlModifier));

    // The "+" button adds on the current node
    QToolButton* plus = navigator.addButton();
    REQUIRE(plus != nullptr);
    CHECK(plus->isEnabled());
    plus->click();
    REQUIRE(added.size() == 4);
    CHECK(added.last().scope == gui::AddTarget::Scope::Element);

    // On the book, a section and the Workshop: they are added to and renamed, not deleted
    added.clear();
    renamed.clear();
    navigator.select(navigator.bookItem());
    navigator.press(Qt::Key_Insert);
    navigator.press(Qt::Key_F2);
    navigator.press(Qt::Key_Delete);
    navigator.select(navigator.section(QStringLiteral("section_frontmatter")));
    navigator.press(Qt::Key_Insert);
    navigator.press(Qt::Key_F2);
    navigator.select(navigator.section(QStringLiteral("workshop")));
    navigator.press(Qt::Key_Insert);
    navigator.press(Qt::Key_F2);
    navigator.press(Qt::Key_Delete);
    REQUIRE(added.size() == 3);
    CHECK(added.at(0).scope == gui::AddTarget::Scope::Book);
    CHECK(added.at(1).scope == gui::AddTarget::Scope::Section);
    CHECK(added.at(1).place == core::BookPlace::Front);
    CHECK(added.at(2).scope == gui::AddTarget::Scope::Workshop);
    CHECK(renamed == QStringList{QStringLiteral("book"), QStringLiteral("section_frontmatter"),
                                 QStringLiteral("workshop")});
    CHECK(deleted == QStringList{chapter});

    // On the project's node the window starts at the book, as on the book's node
    added.clear();
    navigator.select(navigator.projectItem());
    CHECK(plus->isEnabled());
    navigator.press(Qt::Key_Insert);
    plus->click();
    REQUIRE(added.size() == 2);
    CHECK(added.at(0).scope == gui::AddTarget::Scope::Book);
    CHECK(added.at(1).scope == gui::AddTarget::Scope::Book);
    CHECK(navigator.inMenuOf(navigator.projectItem(), QStringLiteral("Add...")));
    REQUIRE(added.size() == 3);
    CHECK(added.at(2).scope == gui::AddTarget::Scope::Book);

    // Without the choice at the top of the window nothing is added there: the button is grey,
    // Insert does nothing and the menu has no Add...
    auto& settings = core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(gui::NavigatorPanel::ADD_WINDOW_SETTING, "sections");
    settings.set<std::string>(gui::NavigatorPanel::ADD_WINDOW_SETTING, "place");
    navigator.panel->onSettingsChanged();
    added.clear();
    CHECK_FALSE(plus->isEnabled());
    navigator.press(Qt::Key_Insert);
    CHECK(added.isEmpty());
    CHECK_FALSE(navigator.inMenuOf(navigator.projectItem(), QStringLiteral("Add...")));
    navigator.select(navigator.section(QStringLiteral("workshop")));
    CHECK(plus->isEnabled());
    settings.set<std::string>(gui::NavigatorPanel::ADD_WINDOW_SETTING, before);
}

TEST_CASE("Navigator menus: Add... and the commands named after kinds, or Add... only",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;
    auto& settings = core::SettingsManager::getInstance();
    const std::string before =
        settings.get<std::string>(gui::NavigatorPanel::ADD_COMMANDS_SETTING, "named");
    settings.set<std::string>(gui::NavigatorPanel::ADD_COMMANDS_SETTING, "named");
    const QString part = navigator.added(QStringLiteral("part"));
    const QString chapter = navigator.added(QStringLiteral("chapter"), core::BookPlace::Main, part);
    const QString dedication =
        navigator.added(QStringLiteral("dedication"), core::BookPlace::Front);

    // Add... is the first command of the menus that add, in bold
    QStringList bold;
    const QStringList chapterMenu = navigator.menuTexts(navigator.item(chapter), &bold);
    CHECK(chapterMenu.value(1) == QStringLiteral("Add..."));
    CHECK(bold == QStringList{QStringLiteral("Add...")});

    // By default also the kinds most added there, by name
    CHECK(navigator.addCommandsOf(navigator.bookItem()) ==
          QStringList{"Add...", "Add Chapter...", "Add Part..."});
    CHECK(navigator.addCommandsOf(navigator.section(QStringLiteral("section_body"))) ==
          QStringList{"Add...", "Add Part...", "Add Chapter..."});
    CHECK(navigator.addCommandsOf(navigator.section(QStringLiteral("section_frontmatter"))) ==
          QStringList{"Add..."});
    CHECK(navigator.addCommandsOf(navigator.item(part)) ==
          QStringList{"Add...", "Add Chapter..."});
    CHECK(navigator.addCommandsOf(navigator.item(chapter)) ==
          QStringList{"Add...", "Add Chapter..."});
    CHECK(navigator.addCommandsOf(navigator.item(dedication)) == QStringList{"Add..."});
    CHECK(navigator.addCommandsOf(navigator.section(QStringLiteral("workshop"))) ==
          QStringList{"Add...", "Add Working note...", "Add Material..."});

    // A command named after a kind opens the window with that kind chosen
    QList<gui::AddTarget> requested;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAdd,
                     [&requested](const gui::AddTarget& target) { requested << target; });
    CHECK(navigator.inMenuOf(navigator.item(part), QStringLiteral("Add Chapter...")));
    REQUIRE(requested.size() == 1);
    CHECK(requested.first().scope == gui::AddTarget::Scope::Group);
    CHECK(requested.first().id == part);
    REQUIRE(requested.first().kind);
    CHECK(requested.first().kind.kind->id == QStringLiteral("chapter"));

    // Add... only: the kind is chosen in the window
    settings.set<std::string>(gui::NavigatorPanel::ADD_COMMANDS_SETTING, "single");
    for (QTreeWidgetItem* node :
         {navigator.bookItem(), navigator.section(QStringLiteral("section_body")),
          navigator.item(part), navigator.item(chapter),
          navigator.section(QStringLiteral("workshop"))}) {
        INFO(node->text(0).toStdString());
        CHECK(navigator.addCommandsOf(node) == QStringList{"Add..."});
    }
    settings.set<std::string>(gui::NavigatorPanel::ADD_COMMANDS_SETTING, before);
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
    const QString partId = navigator.added(QStringLiteral("part"));
    navigator.added(QStringLiteral("chapter"), core::BookPlace::Main, partId);
    navigator.added(QStringLiteral("chapter"));
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

    const QString partId = navigator.added(QStringLiteral("part"));
    navigator.added(QStringLiteral("chapter"), core::BookPlace::Main, partId);
    navigator.added(QStringLiteral("chapter"));
    REQUIRE(novel.book().mainElements.size() == 2);
    REQUIRE(novel.book().mainElements.first().elements.size() == 1);
    const core::ProjectElement chapter = novel.book().mainElements.first().elements.first();
    const QString bodyChapterId = novel.book().mainElements.last().id;
    CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");

    // The chapter is open, with unsaved changes
    navigator.coordinator.onElementSelected(chapter.id, chapter.title);
    REQUIRE(navigator.tabs->count() == 1);
    write(*navigator.tabs, 0, QStringLiteral("The storm came."));
    REQUIRE(navigator.coordinator.isChapterDirty(chapter.id));

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

        // The Navigator lists the file at the end of the project, after the book and the
        // Workshop, open
        QTreeWidgetItem* unassigned = navigator.section(QStringLiteral("unassigned_files"));
        REQUIRE(unassigned != nullptr);
        CHECK(unassigned->text(0) == QStringLiteral("Unassigned Files (1)"));
        CHECK(unassigned->isExpanded());
        CHECK(unassigned->parent() == navigator.projectItem());
        CHECK(navigator.projectItem()->indexOfChild(unassigned) ==
              navigator.projectItem()->childCount() - 1);
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
        write(*navigator.tabs, 0, QStringLiteral("The storm came at night."));
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
    const QString partId = navigator.added(QStringLiteral("part"));
    navigator.added(QStringLiteral("chapter"), core::BookPlace::Main, partId);
    navigator.added(QStringLiteral("chapter"), core::BookPlace::Main, partId);
    const QList<core::ProjectElement> chapters = novel.book().mainElements.first().elements;
    REQUIRE(titles(chapters) == "Chapter 1, Chapter 2");
    const QStringList files{pm.filePathOf(chapters.at(0)), pm.filePathOf(chapters.at(1))};
    navigator.coordinator.onElementSelected(chapters.at(0).id, chapters.at(0).title);
    REQUIRE(navigator.tabs->count() == 1);
    write(*navigator.tabs, 0, QStringLiteral("The storm came at night."));
    REQUIRE(navigator.coordinator.isChapterDirty(chapters.at(0).id));

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
        navigator.added(QStringLiteral("chapter"));
    }
    navigator.added(QStringLiteral("prologue"));
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

    // Two parts, with a chapter each
    CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs({.kind = "part"})}) == 1);
    CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs({.kind = "part"})}) == 1);
    REQUIRE(novel.book().mainElements.size() == 2);
    const QString partOne = novel.book().mainElements.at(0).id;
    const QString partTwo = novel.book().mainElements.at(1).id;
    CHECK(navigator.current() == partTwo);
    CHECK(navigator.add(onGroup(partOne), {addAs()}) == 1);
    navigator.item(partTwo)->setExpanded(false);
    CHECK(navigator.add(onGroup(partTwo), {addAs()}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2");

    // The navigator shows the new element, also in a part that was collapsed
    REQUIRE(navigator.item(partTwo) != nullptr);
    CHECK(navigator.item(partTwo)->isExpanded());
    CHECK(navigator.current() == novel.book().mainElements.at(1).elements.first().id);

    // The prologue first in the first part
    CHECK(navigator.add(onSection(core::BookPlace::Main),
                        {addAs({.kind = "prologue",
                                .place = core::ElementPlace{core::BookPlace::Main, partOne, 0}})}) ==
          1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II");
    CHECK(titles(novel.book().mainElements.at(0).elements) == "Prologue, Chapter 1");
    CHECK(titles(novel.savedBook().mainElements.at(0).elements) == "Prologue, Chapter 1");
    CHECK(navigator.itemsIn(partOne) == "Prologue [Draft], Chapter 1 [Draft]");

    // The epilogue, from the menu of the second part, closes that part
    CHECK(navigator.add(onGroup(partTwo), {addAs({.kind = "epilogue"})}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Epilogue");

    // A new chapter of that part goes before the epilogue
    CHECK(navigator.add(onGroup(partTwo), {addAs()}) == 1);
    CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Chapter 3, Epilogue");

    // Put at the end of the body
    const QString epilogueId = novel.book().mainElements.at(1).elements.last().id;
    REQUIRE(pm.removeElement(epilogueId).has_value());
    CHECK(navigator.add(onGroup(partTwo),
                        {addAs({.kind = "epilogue",
                                .place = core::ElementPlace{core::BookPlace::Main, QString(),
                                                            2}})}) == 1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II, Epilogue");

    // A new chapter of the body goes before it
    CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs()}) == 1);
    CHECK(titles(novel.book().mainElements) == "Part I, Part II, Chapter 4, Epilogue");
}

TEST_CASE("Navigator commands: the epilogue at the end of the last part stays the last element",
          "[gui][navigator]") {
    OpenNovel novel;
    Navigator navigator;

    // A part with a chapter and the epilogue
    CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs({.kind = "part"})}) == 1);
    REQUIRE(novel.book().mainElements.size() == 1);
    const QString partOne = novel.book().mainElements.at(0).id;
    CHECK(navigator.add(onGroup(partOne), {addAs()}) == 1);
    CHECK(navigator.add(onGroup(partOne), {addAs({.kind = "epilogue"})}) == 1);
    REQUIRE(titles(novel.book().mainElements.at(0).elements) == "Chapter 1, Epilogue");

    SECTION("A new chapter of the body goes before it, at the end of the part") {
        CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs()}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I");
        CHECK(titles(novel.book().mainElements.at(0).elements) ==
              "Chapter 1, Chapter 2, Epilogue");
        CHECK(titles(novel.savedBook().mainElements.at(0).elements) ==
              "Chapter 1, Chapter 2, Epilogue");
    }

    SECTION("The writer can put it after the part") {
        CHECK(navigator.add(onSection(core::BookPlace::Main),
                            {addAs({.place = core::ElementPlace{core::BookPlace::Main, QString(),
                                                                1}})}) == 1);
        CHECK(titles(novel.book().mainElements) == "Part I, Chapter 2");
        CHECK(titles(novel.book().mainElements.at(0).elements) == "Chapter 1, Epilogue");
    }

    SECTION("A new part takes it to its end, and its chapters go before it") {
        CHECK(navigator.add(onSection(core::BookPlace::Main), {addAs({.kind = "part"})}) == 1);
        REQUIRE(titles(novel.book().mainElements) == "Part I, Part II");
        const QString partTwo = novel.book().mainElements.at(1).id;
        CHECK(titles(novel.book().mainElements.at(0).elements) == "Chapter 1");
        CHECK(titles(novel.book().mainElements.at(1).elements) == "Epilogue");
        CHECK(titles(novel.savedBook().mainElements.at(1).elements) == "Epilogue");
        CHECK(navigator.itemsIn(partTwo) == "Epilogue [Draft]");

        CHECK(navigator.add(onGroup(partTwo), {addAs()}) == 1);
        CHECK(titles(novel.book().mainElements.at(1).elements) == "Chapter 2, Epilogue");
    }

    SECTION("A new part leaves it where it is when the writer wants") {
        CHECK(navigator.add(onSection(core::BookPlace::Main),
                            {addAs({.kind = "part", .take = false})}) == 1);
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
        navigator.added(QStringLiteral("chapter"));
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

TEST_CASE("Navigator menu: Move Up and Move Down take an element out of its part and its "
          "section, and say why when it has no other place", "[gui][navigator]") {
    // Test reports of 17 October, part 11, and of 19 October, part 3: no command moved the
    // only chapter of a new novel; a chapter can go before a part, and to the front and the
    // back section
    OpenNovel novel;
    Navigator navigator;
    core::ProjectManager& pm = novel.pm;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestMoveElement,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onRequestMove);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::elementMovedTo,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onElementMovedTo);
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();

    // The only chapter of a book without parts goes up to the front section and down to the
    // back one; in its own section it is first and last
    navigator.added(QStringLiteral("chapter"));
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1");
    const QString chapterId = novel.book().mainElements.first().id;
    CHECK(navigator.inMenuOf(navigator.item(chapterId), QStringLiteral("Move..."), false));
    CHECK_FALSE(navigator.chooseInMenu(chapterId, QStringLiteral("Move to Start")));
    CHECK_FALSE(navigator.chooseInMenu(chapterId, QStringLiteral("Move to End")));
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().frontElements) == "Chapter 1");
    CHECK(novel.book().mainElements.isEmpty());
    CHECK(titles(novel.savedBook().frontElements) == "Chapter 1");
    CHECK(navigator.current() == chapterId);
    CHECK_FALSE(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().mainElements) == "Chapter 1");
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().backElements) == "Chapter 1");
    CHECK(novel.book().mainElements.isEmpty());
    CHECK_FALSE(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1");

    // The only part of a book, with the chapter in it: each command is greyed out and says why
    const QString partId =
        pm.addElement(pm.partKind(), QStringLiteral("Part I"), core::BookPlace::Main);
    REQUIRE_FALSE(partId.isEmpty());
    navigator.coordinator.refreshNavigator();
    emit navigator.panel->elementMovedTo(chapterId, {core::BookPlace::Main, partId, 0});
    REQUIRE(titles(novel.book().mainElements) == "Part I");
    for (const char* command :
         {"Move...", "Move to Start", "Move Up", "Move Down", "Move to End"}) {
        QString why;
        CHECK_FALSE(navigator.inMenuOf(navigator.item(partId), QString::fromUtf8(command),
                                       false, &why));
        CHECK(why == QStringLiteral("The book has no other place for the part."));
    }

    // The first chapter of a part goes up before it, the last one down after it
    QString why;
    CHECK(navigator.inMenuOf(navigator.item(chapterId), QStringLiteral("Move Up"), false, &why));
    CHECK_FALSE(why.contains(QStringLiteral("no other place")));
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().mainElements) == "Chapter 1, Part I");
    CHECK(novel.book().mainElements.at(1).elements.isEmpty());
    CHECK(titles(novel.savedBook().mainElements) == "Chapter 1, Part I");
    CHECK(navigator.current() == chapterId);

    emit navigator.panel->elementMovedTo(chapterId, {core::BookPlace::Main, partId, 0});
    REQUIRE(titles(novel.book().mainElements) == "Part I");
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().mainElements) == "Part I, Chapter 1");
    CHECK(novel.book().mainElements.at(0).elements.isEmpty());

    // Out of the main section and back
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().mainElements) == "Part I");
    CHECK(titles(novel.book().backElements) == "Chapter 1");
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().mainElements) == "Part I, Chapter 1");
    CHECK(novel.book().backElements.isEmpty());

    // The table of contents goes from the back section up to the front one, over the main
    // section where it cannot be, and back down
    core::ProjectElement toc;
    toc.id = QStringLiteral("toc");
    toc.kind = {QStringLiteral("kalahari.base"), QStringLiteral("toc")};
    toc.title = QStringLiteral("Contents");
    pm.project()->elementsIn(core::BookPlace::Back).append(toc);
    navigator.coordinator.refreshNavigator();
    const QString tocId = toc.id;
    CHECK_FALSE(navigator.chooseInMenu(tocId, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(tocId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().frontElements) == "Contents");
    CHECK(novel.book().backElements.isEmpty());
    CHECK_FALSE(navigator.chooseInMenu(tocId, QStringLiteral("Move Up")));
    CHECK(navigator.chooseInMenu(tocId, QStringLiteral("Move Down")));
    CHECK(novel.book().frontElements.isEmpty());
    CHECK(titles(novel.book().backElements) == "Contents");
}

TEST_CASE("Navigator menu: nothing is moved before the title page", "[gui][navigator]") {
    // Test report of 19 October, part 1: the only chapter could be moved before the title
    // page, which opens the book
    OpenNovel novel;
    Navigator navigator;
    core::ProjectManager& pm = novel.pm;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestMoveElement,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onRequestMove);
    QObject::connect(navigator.panel, &gui::NavigatorPanel::elementMovedTo,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onElementMovedTo);
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();

    // A title page and a dedication in the front section, a chapter in the main one
    const core::KindRef titlePage =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("title_page"));
    const core::KindRef dedication =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("dedication"));
    REQUIRE(titlePage);
    REQUIRE(dedication);
    const QString titleId =
        pm.addElement(titlePage, QStringLiteral("Title Page"), core::BookPlace::Front);
    REQUIRE_FALSE(titleId.isEmpty());
    REQUIRE_FALSE(
        pm.addElement(dedication, QStringLiteral("For Anna"), core::BookPlace::Front).isEmpty());
    navigator.added(QStringLiteral("chapter"));
    REQUIRE(titles(novel.book().frontElements) == "Title Page, For Anna");
    REQUIRE(titles(novel.book().mainElements) == "Chapter 1");
    const QString chapterId = novel.book().mainElements.first().id;

    // The chapter goes up to the end of the front section, and one step more, but not before
    // the title page: there Move Up and Move to Start are greyed out
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().frontElements) == "Title Page, For Anna, Chapter 1");
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().frontElements) == "Title Page, Chapter 1, For Anna");
    CHECK(titles(novel.savedBook().frontElements) == "Title Page, Chapter 1, For Anna");
    CHECK_FALSE(navigator.inMenuOf(navigator.item(chapterId), QStringLiteral("Move Up"), false));
    CHECK_FALSE(
        navigator.inMenuOf(navigator.item(chapterId), QStringLiteral("Move to Start"), false));
    CHECK(navigator.inMenuOf(navigator.item(chapterId), QStringLiteral("Move Down"), false));

    // The title page has no other place, and each move command says why
    for (const char* command :
         {"Move...", "Move to Start", "Move Up", "Move Down", "Move to End"}) {
        QString why;
        CHECK_FALSE(navigator.inMenuOf(navigator.item(titleId), QString::fromUtf8(command),
                                       false, &why));
        CHECK(why == QStringLiteral("The book has no other place for the title page."));
    }

    // ProjectManager keeps everything after the title page, also when asked directly
    CHECK_FALSE(pm.moveElement(chapterId, 0));
    CHECK(pm.startIndexOf(chapterId) == 1);
    CHECK(pm.endIndexOf(titleId) == 0);
    CHECK_FALSE(pm.canMoveTo(chapterId, {core::BookPlace::Front, QString(), 0}));
    const core::KindRef motto =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("motto"));
    REQUIRE(motto);
    CHECK(pm.addElement(motto, QStringLiteral("Motto"), core::BookPlace::Front, QString(), 0)
              .isEmpty());
    CHECK(titles(novel.book().frontElements) == "Title Page, Chapter 1, For Anna");
    const QString mottoId =
        pm.addElement(motto, QStringLiteral("Motto"), core::BookPlace::Front, QString(), 1);
    CHECK_FALSE(mottoId.isEmpty());
    CHECK(titles(novel.book().frontElements) == "Title Page, Motto, Chapter 1, For Anna");
    REQUIRE(pm.removeElement(mottoId).has_value());
    navigator.coordinator.refreshNavigator();

    // Back down: past the dedication, and into the main section
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(chapterId, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().frontElements) == "Title Page, For Anna");
    CHECK(titles(novel.book().mainElements) == "Chapter 1");
}

TEST_CASE("Navigator menu: a file outside the book is added to an open book only",
          "[gui][navigator]") {
    // Round 3 report 19.2: without a book, Add to Book could only say that one must be opened
    // first. It is greyed out, and its tooltip says why.
    const QString path = QDir::temp().absoluteFilePath(QStringLiteral("notes.txt"));
    QString toolTip;
    {
        Navigator navigator;
        navigator.panel->resize(400, 600);
        navigator.window.resize(400, 600);
        navigator.window.show();
        navigator.panel->addStandaloneFile(path);
        CHECK_FALSE(navigator.inMenuOf(navigator.item(path), QStringLiteral("Add to Book..."),
                                       false, &toolTip));
        CHECK(toolTip == QStringLiteral("No book is open"));
        CHECK(navigator.inMenuOf(navigator.item(path), QStringLiteral("Open"), false));
    }

    OpenNovel novel;
    Navigator navigator;
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();
    navigator.panel->addStandaloneFile(path);
    CHECK(navigator.inMenuOf(navigator.item(path), QStringLiteral("Add to Book..."), false));
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
    QObject::connect(navigator.panel, &gui::NavigatorPanel::elementMovedTo, &coordinator,
                     &gui::NavigatorCoordinator::onElementMovedTo);
    navigator.panel->resize(400, 600);
    navigator.window.resize(400, 600);
    navigator.window.show();

    // Two chapters, and an item at each end of the book: a dedication, as a chapter goes
    // after the title page
    navigator.added(QStringLiteral("chapter"));
    navigator.added(QStringLiteral("chapter"));
    REQUIRE_FALSE(novel.pm.addElement(novelKind(QStringLiteral("dedication")),
                                      QStringLiteral("For Anna"), core::BookPlace::Front)
                      .isEmpty());
    REQUIRE_FALSE(novel.pm.addElement(novelKind(QStringLiteral("afterword")),
                                      QStringLiteral("Thanks"), core::BookPlace::Back)
                      .isEmpty());
    coordinator.refreshNavigator();
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

    // An element moves one step in the list, also into the section before or after its own
    // where its kind can be: a chapter into the front and the back section too (test report
    // of 19 October, part 3)
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().frontElements) == "Chapter 1, For Anna");
    CHECK(navigator.itemsInBook() ==
          "Chapter 1 [Draft], For Anna [Draft], Chapter 2 [Draft], Thanks [Draft]");
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move Up")));
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().frontElements) == "For Anna");
    CHECK(titles(novel.book().mainElements) == "Chapter 2, Chapter 1");
    CHECK(navigator.itemsInBook() ==
          "For Anna [Draft], Chapter 2 [Draft], Chapter 1 [Draft], Thanks [Draft]");
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Down")));
    CHECK(titles(novel.book().backElements) == "Thanks, Chapter 1");
    CHECK_FALSE(navigator.chooseInMenu(first, QStringLiteral("Move Down")));
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().backElements) == "Chapter 1, Thanks");

    // Up past the last element of the main section: into the main section, not to the end of
    // the front one, which is the same place in the list
    CHECK(navigator.chooseInMenu(first, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().mainElements) == "Chapter 1, Chapter 2");
    CHECK(titles(novel.book().frontElements) == "For Anna");
    CHECK(titles(novel.book().backElements) == "Thanks");
    CHECK(navigator.chooseInMenu(second, QStringLiteral("Move Up")));
    CHECK(titles(novel.book().mainElements) == "Chapter 2, Chapter 1");
    CHECK(navigator.itemsInBook() ==
          "For Anna [Draft], Chapter 2 [Draft], Chapter 1 [Draft], Thanks [Draft]");

    // The book's menu adds anywhere in the book: Add... and the kinds most added there
    QList<gui::AddTarget> requested;
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAdd,
                     [&requested](const gui::AddTarget& target) { requested << target; });
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add...")));
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Chapter...")));
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part...")));
    REQUIRE(requested.size() == 3);
    for (const gui::AddTarget& target : requested) {
        CHECK(target.scope == gui::AddTarget::Scope::Book);
    }
    CHECK_FALSE(requested.at(0).kind);
    CHECK(requested.at(1).kind.kind->id == QStringLiteral("chapter"));
    CHECK(requested.at(2).kind.kind->id == QStringLiteral("part"));

    // An item added on the book goes to the end of its section, though the Navigator shows
    // no sections
    CHECK(navigator.add(requested.at(0), {addAs({.kind = "acknowledgments"})}) == 1);
    CHECK(titles(novel.book().backElements) == "Thanks, Acknowledgments");

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
    CHECK(navigator.inMenuOf(navigator.bookItem(), QStringLiteral("Add Part..."), false));

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

    // The Navigator names the project after the new title of the book
    rename(title, QStringLiteral("Renamed Book"));
    CHECK(novel.book().title == QStringLiteral("Renamed Book"));
    CHECK(navigator.projectItem()->text(0) == QStringLiteral("Renamed Book"));
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

namespace {

/// @brief The settings of materials as a test sets them, given back as they were when it ends
struct KeptMaterialSettings {
    std::map<std::string, std::string> values;

    KeptMaterialSettings() {
        for (const auto& [key, usual] : keys()) {
            values[key] = settings().get<std::string>(key, usual);
            settings().set<std::string>(key, usual);
        }
    }
    ~KeptMaterialSettings() {
        try {
            for (const auto& [key, value] : values) {
                settings().set<std::string>(key, value);
            }
        } catch (...) {
            // A destructor may not throw: the later tests then find the settings of this one
            std::fputs("KeptMaterialSettings: the settings of the test stay\n", stderr);
        }
    }
    KeptMaterialSettings(const KeptMaterialSettings&) = delete;
    KeptMaterialSettings& operator=(const KeptMaterialSettings&) = delete;

    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }

    /// The settings with their usual values
    static std::vector<std::pair<std::string, std::string>> keys() {
        return {{gui::NavigatorPanel::QUICK_MATERIALS_SETTING, "link"},
                {gui::NavigatorPanel::OPEN_PAGES_SETTING, "browser"},
                {gui::NavigatorPanel::OPEN_MATERIALS_SETTING, "card"},
                {gui::MaterialDownloader::SETTING, "ask"},
                {gui::dialogs::NewFileDialog::SOURCE_SETTING, "file"}};
    }
};

/// @brief The browser of a test in place of the system's until the test ends
struct TestBrowser {
    QList<QUrl> opened;  ///< The addresses the program asked to open

    TestBrowser() {
        gui::setBrowserForTests([this](const QUrl& url) {
            opened << url;
            return true;
        });
    }
    ~TestBrowser() { gui::setBrowserForTests({}); }
    TestBrowser(const TestBrowser&) = delete;
    TestBrowser& operator=(const TestBrowser&) = delete;
};

/// @brief The material of the Workshop added last
const core::ProjectElement& lastMaterial() {
    const core::BookProject* project = core::ProjectManager::getInstance().project();
    REQUIRE(project != nullptr);
    REQUIRE_FALSE(project->workshop.elements.isEmpty());
    return project->workshop.elements.constLast();
}

}  // namespace

TEST_CASE("Navigator: files, web addresses and pictures dropped or pasted are materials, "
          "linked with the chapter they were dropped on",
          "[gui][navigator][materials]") {
    KeptMaterialSettings kept;
    OpenNovel novel;
    Navigator navigator;
    auto& pm = core::ProjectManager::getInstance();
    QObject::connect(navigator.panel, &gui::NavigatorPanel::requestAddMaterials,
                     &navigator.coordinator, &gui::NavigatorCoordinator::onRequestAddMaterials);
    const QString chapter = navigator.added(QStringLiteral("chapter"));
    navigator.window.resize(600, 800);
    navigator.window.show();
    QCoreApplication::processEvents();

    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString picture = QDir(files.path()).filePath(QStringLiteral("map.png"));
    QImage image(30, 20, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);
    REQUIRE(image.save(picture));

    auto* tree = navigator.panel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    auto* pasting = dynamic_cast<gui::PasteTarget*>(tree);
    REQUIRE(pasting != nullptr);

    SECTION("a file dropped on a chapter: its copy, linked with the chapter, nothing opens") {
        QTreeWidgetItem* item = navigator.item(chapter);
        REQUIRE(item != nullptr);
        tree->scrollToItem(item);
        const QPoint at = tree->visualItemRect(item).center();
        QMimeData dropped;
        dropped.setUrls({QUrl::fromLocalFile(picture)});
        QDragEnterEvent enter(at, Qt::CopyAction | Qt::MoveAction, &dropped, Qt::LeftButton,
                              Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &enter);
        CHECK(enter.isAccepted());
        CHECK(enter.dropAction() == Qt::CopyAction);
        QDropEvent drop(QPointF(at), Qt::CopyAction | Qt::MoveAction, &dropped, Qt::LeftButton,
                        Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &drop);
        QCoreApplication::processEvents();

        const core::ProjectElement& map = lastMaterial();
        CHECK(map.title == QStringLiteral("map"));
        CHECK(map.links == QStringList{chapter});
        CHECK(QFileInfo(pm.filePathOf(map)).fileName() == QStringLiteral("map.png"));
        CHECK(QFileInfo::exists(pm.filePathOf(map)));
        CHECK(navigator.current() == map.id);
        CHECK(navigator.tabs->count() == 0);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Material added: map – a copy of the file in the book, linked with "
                             "\"Chapter 1\""));
        CHECK(titles(novel.savedProject().workshop.elements) == "map");
    }

    SECTION("several files dropped on a chapter: all linked with it") {
        const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
        QFile file(notes);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write("Dunes") == 5);
        file.close();
        QTreeWidgetItem* item = navigator.item(chapter);
        REQUIRE(item != nullptr);
        tree->scrollToItem(item);
        const QPoint at = tree->visualItemRect(item).center();
        QMimeData dropped;
        dropped.setUrls({QUrl::fromLocalFile(picture), QUrl::fromLocalFile(notes)});
        QDragEnterEvent enter(at, Qt::CopyAction | Qt::MoveAction, &dropped, Qt::LeftButton,
                              Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &enter);
        REQUIRE(enter.isAccepted());
        QDropEvent drop(QPointF(at), Qt::CopyAction | Qt::MoveAction, &dropped, Qt::LeftButton,
                        Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &drop);
        QCoreApplication::processEvents();

        CHECK(titles(novel.savedProject().workshop.elements) == "map, notes");
        for (const core::ProjectElement& material : pm.project()->workshop.elements) {
            CHECK(material.links == QStringList{chapter});
        }
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Added to \"Materials\": map, notes; all linked with \"Chapter 1\""));
        CHECK(navigator.tabs->count() == 0);
    }

    SECTION("pasted: a web address, a picture without a file; a text adds nothing") {
        tree->setCurrentItem(navigator.item(chapter));
        QGuiApplication::clipboard()->setText(
            QStringLiteral("https://pl.wikipedia.org/wiki/Kalahari"));
        CHECK(pasting->canPasteHere());
        pasting->pasteHere();
        // A copy: the next materials move the list
        core::ProjectElement page = lastMaterial();
        CHECK(page.title == QStringLiteral("Kalahari"));
        CHECK(page.url == QStringLiteral("https://pl.wikipedia.org/wiki/Kalahari"));
        CHECK(page.file.isEmpty());
        CHECK(page.links == QStringList{chapter});
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Material added: Kalahari – the address of a page, linked with "
                             "\"Chapter 1\""));

        // A screenshot: a PNG file in the book
        QGuiApplication::clipboard()->setImage(image);
        tree->setCurrentItem(navigator.item(page.id));  // A material links with nothing
        pasting->pasteHere();
        const core::ProjectElement& shot = lastMaterial();
        CHECK(shot.title.startsWith(QStringLiteral("Picture from the clipboard ")));
        CHECK(shot.file.startsWith(QStringLiteral("workshop/clipboard_")));
        CHECK(shot.file.endsWith(QStringLiteral(".png")));
        CHECK(shot.links.isEmpty());
        CHECK(QImage(pm.filePathOf(shot)).size() == QSize(30, 20));

        // A text that is no file and no address
        const qsizetype count = pm.project()->workshop.elements.size();
        QGuiApplication::clipboard()->setText(QStringLiteral("a caravan of words"));
        CHECK(pasting->canPasteHere());
        pasting->pasteHere();
        CHECK(pm.project()->workshop.elements.size() == count);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("The clipboard has no file, web address or picture, so nothing was "
                             "added."));
        QGuiApplication::clipboard()->clear();
    }

    SECTION("as the settings say: without links, or in the Add Element window") {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<std::string>(gui::NavigatorPanel::QUICK_MATERIALS_SETTING, "add");
        gui::IncomingMaterials incoming;
        incoming.files << picture;
        emit navigator.panel->requestAddMaterials(incoming, QString());
        CHECK(lastMaterial().links.isEmpty());
        CHECK(navigator.tabs->count() == 0);

        // The window, filled with the address; Add opens the card of the material
        settings.set<std::string>(gui::NavigatorPanel::QUICK_MATERIALS_SETTING, "window");
        gui::IncomingMaterials address;
        address.addresses << QUrl(QStringLiteral("https://example.com/oasis"));
        QString filled;
        CHECK(answering(
                  [&navigator, &address] {
                      navigator.coordinator.onRequestAddMaterials(address, QString());
                  },
                  {[&filled](QDialog& dialog) {
                      auto* window = qobject_cast<gui::dialogs::NewFileDialog*>(&dialog);
                      REQUIRE(window != nullptr);
                      filled = window->addressEdit()->text();
                      window->createButton()->click();
                  }}) == 1);
        CHECK(filled == QStringLiteral("https://example.com/oasis"));
        CHECK(lastMaterial().url == QStringLiteral("https://example.com/oasis"));
        CHECK(lastMaterial().title == QStringLiteral("oasis"));
        REQUIRE(navigator.tabs->count() == 1);
        auto* card = qobject_cast<gui::MaterialCardPanel*>(navigator.tabs->widget(0));
        REQUIRE(card != nullptr);
        CHECK(card->elementId() == lastMaterial().id);
        CHECK(card->page()->isFull());
    }
}

TEST_CASE("Navigator: Expand All and Collapse All of a menu have the icons of the panel's "
          "buttons",
          "[gui][navigator]") {
    gui::registerAllIcons();
    OpenNovel novel;
    Navigator navigator;
    navigator.window.setAttribute(Qt::WA_DontShowOnScreen);
    navigator.window.resize(400, 900);
    navigator.panel->setGeometry(0, 0, 400, 900);
    navigator.window.show();
    auto* tree = navigator.panel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* book = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole + 1).toString() == QStringLiteral("book")) {
            book = *it;
        }
    }
    REQUIRE(book != nullptr);

    test::ContextMenuReader reader(navigator.panel);
    emit tree->customContextMenuRequested(tree->visualItemRect(book).center());
    REQUIRE(reader.menusRead() == 1);
    // A null icon where there is no such item or button
    const auto iconOf = [&reader](const QString& text) {
        for (const test::MenuItem& item : reader.items()) {
            if (item.text == text) {
                return item.icon;
            }
        }
        return QIcon();
    };
    const auto buttonIcon = [&navigator](const QString& toolTip) {
        for (const QToolButton* button : navigator.panel->findChildren<QToolButton*>()) {
            if (button->toolTip() == toolTip) {
                return button->icon();
            }
        }
        return QIcon();
    };
    const auto picture = [](const QIcon& icon) { return icon.pixmap(QSize(16, 16)).toImage(); };

    const QIcon expand = iconOf(QStringLiteral("&Expand All"));
    const QIcon collapse = iconOf(QStringLiteral("&Collapse All"));
    REQUIRE_FALSE(expand.isNull());
    REQUIRE_FALSE(collapse.isNull());
    REQUIRE_FALSE(buttonIcon(QStringLiteral("Expand All")).isNull());
    CHECK(picture(expand) == picture(buttonIcon(QStringLiteral("Expand All"))));
    CHECK(picture(collapse) == picture(buttonIcon(QStringLiteral("Collapse All"))));
    CHECK(picture(expand) != picture(collapse));
}

TEST_CASE("Navigator: Rename and Properties of a menu have the icons of their commands",
          "[gui][navigator]") {
    gui::registerAllIcons();
    OpenNovel novel;
    auto& pm = novel.pm;
    const QString partId =
        pm.addElement(pm.partKind(), QStringLiteral("Part I"), core::BookPlace::Main);
    REQUIRE_FALSE(partId.isEmpty());
    const QString chapterId = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                            QStringLiteral("The Storm"), core::BookPlace::Main,
                                            partId);
    REQUIRE_FALSE(chapterId.isEmpty());
    Navigator navigator;
    navigator.window.setAttribute(Qt::WA_DontShowOnScreen);
    navigator.window.resize(400, 900);
    navigator.panel->setGeometry(0, 0, 400, 900);
    navigator.window.show();
    auto* tree = navigator.panel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);

    auto& artProvider = core::ArtProvider::getInstance();
    const QIcon rename = artProvider.getIcon(QStringLiteral("edit.rename"), core::IconContext::Menu);
    const QIcon panel =
        artProvider.getIcon(QStringLiteral("view.properties"), core::IconContext::Menu);
    const QIcon book =
        artProvider.getIcon(QStringLiteral("book.properties"), core::IconContext::Menu);
    REQUIRE_FALSE(rename.isNull());
    REQUIRE_FALSE(panel.isNull());
    REQUIRE_FALSE(book.isNull());
    const auto picture = [](const QIcon& icon) { return icon.pixmap(QSize(16, 16)).toImage(); };

    test::ContextMenuReader reader(navigator.panel);
    struct Command {
        QTreeWidgetItem* node;
        QString text;  ///< Without its access key and keys
        QIcon icon;
    };
    const QList<Command> commands{
        {navigator.item(chapterId), QStringLiteral("Rename..."), rename},
        {navigator.item(chapterId), QStringLiteral("Properties..."), panel},
        {navigator.item(partId), QStringLiteral("Rename..."), rename},
        {navigator.item(partId), QStringLiteral("Properties..."), panel},
        {navigator.section(QStringLiteral("section_body")), QStringLiteral("Rename..."), rename},
        {navigator.section(QStringLiteral("workshop")), QStringLiteral("Rename..."), rename},
        {navigator.bookItem(), QStringLiteral("Rename..."), rename},
        {navigator.bookItem(), QStringLiteral("Book Properties..."), book},
        {navigator.projectItem(), QStringLiteral("Project Properties..."), panel},
    };
    for (const Command& command : commands) {
        REQUIRE(command.node != nullptr);
        INFO(command.text.toStdString() << " of " << command.node->text(0).toStdString());
        tree->scrollToItem(command.node);
        const int read = reader.menusRead();
        emit tree->customContextMenuRequested(tree->visualItemRect(command.node).center());
        REQUIRE(reader.menusRead() == read + 1);
        QHash<QString, QIcon> icons;
        for (const test::MenuItem& item : reader.items()) {
            icons.insert(gui::withoutAccessKey(item.text.section(QLatin1Char('\t'), 0, 0)),
                         item.icon);
        }
        REQUIRE(icons.contains(command.text));
        REQUIRE_FALSE(icons.value(command.text).isNull());
        CHECK(picture(icons.value(command.text)) == picture(command.icon));
    }
    CHECK(picture(rename) != picture(panel));
}

TEST_CASE("Navigator: a material opens as its source says; the title of its page and the copy "
          "of its picture come after it was added",
          "[gui][navigator][materials]") {
    KeptMaterialSettings kept;
    OpenNovel novel;
    Navigator navigator;
    TestBrowser browser;
    auto& pm = core::ProjectManager::getInstance();
    const core::KindRef kind = novelKind(QStringLiteral("material"));
    REQUIRE(kind);

    core::NewMaterial page;
    page.title = QStringLiteral("Kalahari");
    page.url = QStringLiteral("https://pl.wikipedia.org/wiki/Kalahari");
    const QString pageId = pm.addMaterial(page, kind);
    core::NewMaterial camel;
    camel.title = QStringLiteral("Camel on a dune");
    camel.url = QStringLiteral("https://upload.wikimedia.org/a/b/Camel_on_a_dune.jpg");
    const QString camelId = pm.addMaterial(camel, kind);
    core::NewMaterial book;
    book.title = QStringLiteral("Caravan Trails");
    const QString bookId = pm.addMaterial(book, kind);
    REQUIRE_FALSE(pageId.isEmpty());
    REQUIRE_FALSE(camelId.isEmpty());
    REQUIRE_FALSE(bookId.isEmpty());
    navigator.coordinator.refreshNavigator();

    SECTION("a page and a picture on the web in the browser, or on the card; neither on the "
            "card") {
        navigator.coordinator.onRequestOpenMaterial(pageId);
        navigator.coordinator.onRequestOpenMaterial(camelId);
        CHECK(browser.opened == QList<QUrl>{QUrl(page.url), QUrl(camel.url)});
        CHECK(navigator.statusBar->currentMessage() == QStringLiteral("Opened: Camel on a dune"));
        CHECK(navigator.tabs->count() == 0);

        navigator.coordinator.onRequestOpenMaterial(bookId);
        REQUIRE(navigator.tabs->count() == 1);
        auto* card = qobject_cast<gui::MaterialCardPanel*>(navigator.tabs->currentWidget());
        REQUIRE(card != nullptr);
        CHECK(card->elementId() == bookId);
        CHECK(navigator.tabs->tabText(0) == QStringLiteral("Caravan Trails"));

        core::SettingsManager::getInstance().set<std::string>(
            gui::NavigatorPanel::OPEN_PAGES_SETTING, "card");
        navigator.coordinator.onRequestOpenMaterial(pageId);
        CHECK(navigator.tabs->count() == 2);
        CHECK(browser.opened.size() == 2);
        // Opened again, its tab comes to the front
        navigator.coordinator.onRequestOpenMaterialCard(bookId);
        CHECK(navigator.tabs->count() == 2);
        CHECK(navigator.tabs->currentWidget() == card);

        // A change on the card shows on its tab and in the Navigator
        card->page()->titleEdit()->setText(QStringLiteral("Trails of the Caravans"));
        emit card->page()->titleEdit()->editingFinished();
        CHECK(navigator.tabs->tabText(0) == QStringLiteral("Trails of the Caravans"));
        REQUIRE(navigator.item(bookId) != nullptr);
        CHECK(navigator.item(bookId)->text(0) == QStringLiteral("Trails of the Caravans"));

        // Deleted, the material takes its card with it
        CHECK(answering([&navigator, &bookId] { navigator.coordinator.onRequestDelete(bookId); },
                        {answerDelete(true, false)}) == 1);
        CHECK(navigator.tabs->count() == 1);
    }

    SECTION("the title of the page replaces the one its address gave, not the writer's own") {
        // The writer is at a chapter, typing its notes, when the download ends
        const QString chapter = navigator.added(QStringLiteral("chapter"));
        auto* tree = navigator.panel->findChild<QTreeWidget*>();
        REQUIRE(tree != nullptr);
        tree->setCurrentItem(navigator.item(chapter));
        navigator.properties->showChapterProperties(chapter);
        auto* notes =
            navigator.properties->findChild<QTextEdit*>(QStringLiteral("chapterNotesEdit"));
        REQUIRE(notes != nullptr);
        notes->setPlainText(QStringLiteral("Not saved yet"));
        int asked = 0;
        QObject::connect(navigator.panel, &gui::NavigatorPanel::requestProperties,
                         [&asked](const QString&) { ++asked; });

        gui::MaterialDownloader::Result result;
        result.elementId = pageId;
        result.url = QUrl(page.url);
        result.title = QStringLiteral("Basin of the Kalahari – Wikipedia");
        emit navigator.coordinator.materialDownloader()->finished(result);
        CHECK(pm.findElement(pageId)->title == result.title);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Title from the page: Basin of the Kalahari – Wikipedia"));
        CHECK(novel.savedProject().workshop.elements.size() == 3);
        // In the Workshop at the place of its new title; the chapter stays current and its
        // notes as typed
        const QTreeWidgetItem* group = navigator.item(pageId)->parent();
        REQUIRE(group != nullptr);
        QStringList order;
        for (int i = 0; i < group->childCount(); ++i) {
            order << group->child(i)->text(0);
        }
        CHECK(order.join(QStringLiteral(", ")) ==
              QStringLiteral("Basin of the Kalahari – Wikipedia, Camel on a dune, Caravan Trails"));
        CHECK(navigator.current() == chapter);
        CHECK(asked == 0);
        CHECK(notes->toPlainText() == QStringLiteral("Not saved yet"));

        REQUIRE(pm.renameElement(pageId, QStringLiteral("The desert")));
        emit navigator.coordinator.materialDownloader()->finished(result);
        CHECK(pm.findElement(pageId)->title == QStringLiteral("The desert"));

        // A failure nobody asked about goes to the status bar
        result.title.clear();
        result.error = QStringLiteral("The server did not answer in time.");
        emit navigator.coordinator.materialDownloader()->finished(result);
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Nothing was downloaded for \"The desert\": The server did not "
                             "answer in time."));
    }

    SECTION("the picture downloaded is the copy, which opens on its preview tab") {
        QImage image(16, 12, QImage::Format_RGB32);
        image.fill(Qt::yellow);
        QByteArray data;
        QBuffer buffer(&data);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        REQUIRE(image.save(&buffer, "JPG"));
        gui::MaterialDownloader::Result result;
        result.elementId = camelId;
        result.url = QUrl(camel.url);
        result.picture = true;
        result.data = data;
        result.fileName = QStringLiteral("Camel_on_a_dune.jpg");
        emit navigator.coordinator.materialDownloader()->finished(result);
        const core::ProjectElement* copied = pm.findElement(camelId);
        REQUIRE(copied != nullptr);
        CHECK(copied->file == QStringLiteral("workshop/Camel_on_a_dune.jpg"));
        CHECK(QImage(pm.filePathOf(*copied)).size() == QSize(16, 12));
        CHECK(navigator.statusBar->currentMessage() ==
              QStringLiteral("Copy of the picture saved: Camel on a dune"));

        navigator.coordinator.onRequestOpenMaterial(camelId);
        CHECK(browser.opened.isEmpty());
        REQUIRE(navigator.tabs->count() == 1);
        CHECK(qobject_cast<gui::ImagePreviewPanel*>(navigator.tabs->currentWidget()) != nullptr);
    }
}
