/// @file test_views_toolbar.cpp
/// @brief The Views toolbar: the view modes, the switches, the paper, the text frame border,
///        the formatting marks and the zoom of the View menu

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/toolbar_manager_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/toolbar_manager.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDialogButtonBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>

using namespace kalahari::gui;

namespace {

/// The toolbar of @p window named @p id
QToolBar* toolbarOf(const QMainWindow& window, const QString& id) {
    return window.findChild<QToolBar*>(id);
}

const QColor PRIMARY(QStringLiteral("#333333"));
const QColor SECONDARY(QStringLiteral("#999999"));

/// The icon @p name of the icon theme @p theme drawn at its size, in the colors above
QImage renderIcon(const char* theme, const char* name) {
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/icons/%1/%2.svg")
                   .arg(QLatin1String(theme), QLatin1String(name)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("{COLOR_PRIMARY}"), PRIMARY.name());
    svg.replace(QStringLiteral("{COLOR_SECONDARY}"), SECONDARY.name());
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return {};
    }
    QImage image(24, 24, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return image;
}

/// The layout of the toolbars that a window saved before the Views toolbar existed
QByteArray layoutWithoutViewsToolbar() {
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("text")));
    const auto add = [&window](const char* id, bool visible) {
        auto* toolbar = new QToolBar(QString::fromLatin1(id), &window);
        toolbar->setObjectName(QString::fromLatin1(id));
        toolbar->addAction(QString::fromLatin1(id));
        window.addToolBar(Qt::TopToolBarArea, toolbar);
        toolbar->setVisible(visible);
    };
    for (const char* id : {"quickActions", "edit", "format", "insert"}) {
        add(id, true);
    }
    window.addToolBarBreak(Qt::TopToolBarArea);
    for (const char* id : {"book", "styles", "tools", "help"}) {
        add(id, true);
    }
    add("file", false);
    add("view", false);
    return window.saveState();
}

}  // namespace

TEST_CASE("Views toolbar: the view modes, switches, paper, text frame border, formatting marks "
          "and zoom of the View menu",
          "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    CommandRegistry& registry = CommandRegistry::getInstance();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(registry);

    QToolBar* toolbar = manager.getToolbar("views");
    REQUIRE(toolbar != nullptr);
    CHECK(window.toolBarArea(toolbar) == Qt::TopToolBarArea);
    CHECK_FALSE(toolbar->isHidden());

    // The very actions of the View menu, so a button is checked with its menu item
    const QList<QAction*> actions = toolbar->actions();
    REQUIRE(actions.size() == 15);
    CHECK(actions[0] == registry.getAction(std::string("view.mode.continuous")));
    CHECK(actions[1] == registry.getAction(std::string("view.mode.page")));
    CHECK(actions[2]->isSeparator());
    CHECK(actions[3] == registry.getAction(std::string("view.mode.distraction-free")));
    CHECK(actions[4] == registry.getAction(std::string("view.focus")));
    CHECK(actions[5] == registry.getAction(std::string("view.typewriter")));
    CHECK(actions[6] == registry.getAction(std::string("view.darkPaper")));
    CHECK(actions[7] == registry.getAction(std::string("view.textFrameBorder")));
    CHECK(actions[8] == registry.getAction(std::string("view.showFormattingMarks")));
    CHECK(actions[9]->isSeparator());
    CHECK(actions[10] == registry.getAction(std::string("view.zoomOut")));
    CHECK(actions[11] == registry.getAction(std::string("view.zoomIn")));
    CHECK(actions[12] == registry.getAction(std::string("view.resetZoom")));
    CHECK(actions[13] == registry.getAction(std::string("view.zoomPageWidth")));
    CHECK(actions[14] == registry.getAction(std::string("view.zoomWholePage")));

    // In the View menu the text frame border follows the paper, before the separator
    const Command* paper = registry.getCommand("view.darkPaper");
    const Command* frame = registry.getCommand("view.textFrameBorder");
    REQUIRE(paper != nullptr);
    REQUIRE(frame != nullptr);
    CHECK(frame->menuPath == "VIEW/Text Frame Border");
    CHECK(frame->menuOrder == paper->menuOrder + 1);
    CHECK_FALSE(paper->addSeparatorAfter);
    CHECK(frame->addSeparatorAfter);

    // Every button has an icon of the icon theme
    auto& icons = kalahari::core::IconRegistry::getInstance();
    for (const char* id : {"view.mode.continuous", "view.mode.page", "view.mode.distraction-free",
                           "view.focus", "view.typewriter", "view.darkPaper",
                           "view.textFrameBorder", "view.showFormattingMarks", "view.zoomOut",
                           "view.zoomIn",
                           "view.resetZoom", "view.zoomPageWidth", "view.zoomWholePage"}) {
        INFO(id);
        CHECK(icons.hasIcon(QString::fromLatin1(id)));
    }
}

TEST_CASE("Views toolbar: the paper icon is a square halved corner to corner in every icon theme",
          "[gui][toolbar]") {
    for (const char* theme : {"twotone", "filled", "outlined", "rounded"}) {
        INFO(theme);
        const QImage image = renderIcon(theme, "contrast_square");
        REQUIRE_FALSE(image.isNull());

        // A frame, the lower right half filled and the upper left half empty (in two tones:
        // the second color)
        CHECK(image.pixelColor(4, 12) == PRIMARY);
        CHECK(image.pixelColor(12, 4) == PRIMARY);
        CHECK(image.pixelColor(16, 16) == PRIMARY);
        if (QLatin1String(theme) == QLatin1String("twotone")) {
            CHECK(image.pixelColor(8, 8) == SECONDARY);
        } else {
            CHECK(image.pixelColor(8, 8).alpha() == 0);
        }
    }
}

TEST_CASE("Views toolbar: the text frame border icon is a page with lines of text in a frame in "
          "every icon theme",
          "[gui][toolbar]") {
    for (const char* theme : {"twotone", "filled", "outlined", "rounded"}) {
        INFO(theme);
        const QImage image = renderIcon(theme, "text_frame");
        REQUIRE_FALSE(image.isNull());

        // The left side of the page and of the frame, with room between them, and a line of
        // the text
        CHECK(image.pixelColor(5, 12) == PRIMARY);
        CHECK(image.pixelColor(8, 12).alpha() == 0);
        CHECK(image.pixelColor(9, 12) == PRIMARY);
        CHECK(image.pixelColor(12, 10) == PRIMARY);

        // Between the lines, inside the frame: empty (in two tones: the second color)
        if (QLatin1String(theme) == QLatin1String("twotone")) {
            CHECK(image.pixelColor(12, 11) == SECONDARY);
        } else {
            CHECK(image.pixelColor(12, 11).alpha() == 0);
        }
    }
}

TEST_CASE("Views toolbar: the Page Width and Whole Page icons are a page with a double arrow "
          "across or down it in every icon theme",
          "[gui][toolbar]") {
    for (const char* theme : {"twotone", "filled", "outlined", "rounded"}) {
        INFO(theme);
        const QImage width = renderIcon(theme, "fit_page_width");
        const QImage whole = renderIcon(theme, "fit_page");
        REQUIRE_FALSE(width.isNull());
        REQUIRE_FALSE(whole.isNull());

        // The left side of the page, and the middle of the arrow
        for (const QImage& image : {width, whole}) {
            CHECK(image.pixelColor(5, 12) == PRIMARY);
            CHECK(image.pixelColor(12, 12) == PRIMARY);
        }
        // Page Width: the arrow goes to the sides; Whole Page: to the top and bottom
        CHECK(width.pixelColor(14, 12) == PRIMARY);
        CHECK(width.pixelColor(12, 15) != PRIMARY);
        CHECK(whole.pixelColor(12, 15) == PRIMARY);
        CHECK(whole.pixelColor(14, 12) != PRIMARY);

        // Inside the page, away from the arrow (above it, beside its shaft): empty (in two
        // tones: the second color)
        if (QLatin1String(theme) == QLatin1String("twotone")) {
            CHECK(width.pixelColor(8, 6) == SECONDARY);
            CHECK(whole.pixelColor(8, 11) == SECONDARY);
        } else {
            CHECK(width.pixelColor(8, 6).alpha() == 0);
            CHECK(whole.pixelColor(8, 11).alpha() == 0);
        }
    }
}

TEST_CASE("Views toolbar: the formatting marks icon is a pilcrow in every icon theme",
          "[gui][toolbar]") {
    for (const char* theme : {"twotone", "filled", "outlined", "rounded"}) {
        INFO(theme);
        const QImage image = renderIcon(theme, "format_paragraph");
        REQUIRE_FALSE(image.isNull());

        // The bar on top, the two stems with room between them and the edge of the bowl
        CHECK(image.pixelColor(16, 5) == PRIMARY);
        CHECK(image.pixelColor(11, 16) == PRIMARY);
        CHECK(image.pixelColor(15, 16) == PRIMARY);
        CHECK(image.pixelColor(13, 16).alpha() == 0);
        CHECK(image.pixelColor(7, 6) == PRIMARY);

        // The middle of the bowl: full, empty in outline, the second color in two tones
        const QLatin1String name(theme);
        if (name == QLatin1String("twotone")) {
            CHECK(image.pixelColor(9, 8) == SECONDARY);
        } else if (name == QLatin1String("outlined")) {
            CHECK(image.pixelColor(9, 8).alpha() == 0);
        } else {
            CHECK(image.pixelColor(9, 8) == PRIMARY);
        }
    }
}

TEST_CASE("Views toolbar: one customized before gets the formatting marks after the text frame "
          "border, once",
          "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string savedToolbars = settings.get<std::string>("toolbars.configurations", "");
    const int savedVersion = settings.get<int>("toolbars.addedCommands", 0);
    const QString views = QStringLiteral("views");

    // Customized before the button came: the paper moved to the end
    settings.set<std::string>(
        "toolbars.configurations",
        R"({"builtIn":{"views":["view.mode.continuous","view.mode.page","view.textFrameBorder",)"
        R"("view.zoomIn","view.darkPaper"]}})");
    settings.set<int>("toolbars.addedCommands", 2);
    QMainWindow window;
    {
        ToolbarManager manager(&window);
        CHECK(manager.getToolbarCommands(views).join(QStringLiteral(" | ")).toStdString() ==
              "view.mode.continuous | view.mode.page | view.textFrameBorder | "
              "view.showFormattingMarks | view.zoomIn | view.darkPaper");

        // Taken away by the writer after that, it stays away
        QStringList without = manager.getToolbarCommands(views);
        without.removeAll(QStringLiteral("view.showFormattingMarks"));
        manager.setToolbarCommands(views, without);
    }
    ToolbarManager next(&window);
    CHECK_FALSE(
        next.getToolbarCommands(views).contains(QStringLiteral("view.showFormattingMarks")));

    settings.set<std::string>("toolbars.configurations", savedToolbars);
    settings.set<int>("toolbars.addedCommands", savedVersion);
}

TEST_CASE("Views toolbar: shown in the second row of a layout saved before it existed",
          "[gui][toolbar]") {
    // A window keeps its toolbar layout between sessions; the layout saved by an earlier
    // version has no Views toolbar, which still has to show, next to the other toolbars
    // of its row, without the layout being reset
    const QByteArray savedLayout = layoutWithoutViewsToolbar();

    registerAllCommands(CommandCallbacks{});
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("text")));
    ToolbarManager manager(&window);
    manager.createToolbars(CommandRegistry::getInstance());
    REQUIRE(window.restoreState(savedLayout));

    window.resize(1600, 900);
    window.show();
    QApplication::processEvents();

    QToolBar* views = toolbarOf(window, QStringLiteral("views"));
    QToolBar* book = toolbarOf(window, QStringLiteral("book"));
    QToolBar* edit = toolbarOf(window, QStringLiteral("edit"));
    REQUIRE(views != nullptr);
    REQUIRE(book != nullptr);
    REQUIRE(edit != nullptr);
    CHECK(window.toolBarArea(views) == Qt::TopToolBarArea);
    CHECK(views->isVisible());
    CHECK(views->y() == book->y());
    CHECK(views->y() > edit->y());
}

TEST_CASE("Toolbar manager: the View menu items follow the toolbars and outlive no menu",
          "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(CommandRegistry::getInstance());
    QToolBar* toolbar = manager.getToolbar("views");
    REQUIRE(toolbar != nullptr);

    auto* viewMenu = new QMenu(&window);
    manager.createViewMenuActions(viewMenu);
    QAction* item = nullptr;
    for (QAction* action : viewMenu->findChildren<QAction*>()) {
        if (action->data().toString() == QStringLiteral("views")) {
            item = action;
        }
    }
    REQUIRE(item != nullptr);

    window.show();
    toolbar->setVisible(false);
    CHECK_FALSE(item->isChecked());
    toolbar->setVisible(true);
    CHECK(item->isChecked());

    // A toolbar shown or hidden after its menu is gone no longer reaches the deleted item
    delete viewMenu;
    toolbar->setVisible(false);
    toolbar->setVisible(true);
    CHECK(toolbar->isVisible());
}

TEST_CASE("Panels toolbar: a button for each panel, the Annotations panel too", "[gui][toolbar]") {
    // The user's test: the Annotations panel (F9) had no button on the Panels toolbar
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    CommandRegistry& registry = CommandRegistry::getInstance();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(registry);
    QToolBar* toolbar = manager.getToolbar("view");
    REQUIRE(toolbar != nullptr);

    // The very actions of View > Panels, so a button is checked with its menu item
    const QList<QAction*> actions = toolbar->actions();
    for (const char* id : {"view.navigator", "view.properties", "view.search", "view.assistant",
                           "view.annotations", "view.log"}) {
        INFO(id);
        CHECK(actions.contains(registry.getAction(std::string(id))));
        CHECK(kalahari::core::IconRegistry::getInstance().hasIcon(QString::fromLatin1(id)));
    }
    // Next to the Assistant panel, with which it shares the tabs on the right
    CHECK(actions.indexOf(registry.getAction(std::string("view.annotations"))) ==
          actions.indexOf(registry.getAction(std::string("view.assistant"))) + 1);
}

TEST_CASE("Toolbar manager: a renamed toolbar stays selected", "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(CommandRegistry::getInstance());
    const QString id = manager.createUserToolbar(QStringLiteral("Mine"));

    dialogs::ToolbarManagerDialog dialog(&manager, &window);
    // The list of toolbars is the one that holds the new toolbar
    QListWidget* list = nullptr;
    for (QListWidget* candidate : dialog.findChildren<QListWidget*>()) {
        for (int i = 0; i < candidate->count(); ++i) {
            if (candidate->item(i)->data(Qt::UserRole).toString() == id) {
                list = candidate;
                list->setCurrentRow(i);
            }
        }
    }
    REQUIRE(list != nullptr);
    QPushButton* rename = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Rename...")) {
            rename = button;
        }
    }
    REQUIRE(rename != nullptr);
    REQUIRE(rename->isEnabled());

    QTimer::singleShot(0, []() {
        auto* input = qobject_cast<dialogs::TextInputDialog*>(QApplication::activeModalWidget());
        REQUIRE(input != nullptr);
        input->findChild<QLineEdit*>()->setText(QStringLiteral("Second"));
        input->accept();
    });
    rename->click();

    REQUIRE(list->currentItem() != nullptr);
    CHECK(list->currentItem()->data(Qt::UserRole).toString() == id);
    CHECK(list->currentItem()->text().endsWith(QStringLiteral("Second")));
    CHECK(rename->isEnabled());

    manager.deleteUserToolbar(id);
}

TEST_CASE("Toolbar manager: Apply and OK save a changed toolbar, Reset to Defaults stands apart",
          "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(CommandRegistry::getInstance());
    const QString id = manager.createUserToolbar(QStringLiteral("Mine"));

    dialogs::ToolbarManagerDialog dialog(&manager, &window);
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Customize Toolbars"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("OK"));
    REQUIRE(dialog.applyButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.applyButton()->isEnabled());

    // Reset to Defaults is no button that closes the dialog
    QPushButton* reset = nullptr;
    QPushButton* add = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Reset to Defaults")) {
            reset = button;
        } else if (button->text() == QStringLiteral("Add to Toolbar >>")) {
            add = button;
        }
    }
    REQUIRE(reset != nullptr);
    REQUIRE(add != nullptr);
    CHECK(qobject_cast<QDialogButtonBox*>(reset->parentWidget()) == nullptr);
    CHECK(reset->mapTo(&dialog, QPoint(0, 0)).x() <
          dialog.acceptButton()->mapTo(&dialog, QPoint(0, 0)).x());

    // The new toolbar, and the first command of the first category on it
    for (QListWidget* list : dialog.findChildren<QListWidget*>()) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == id) {
                list->setCurrentRow(i);
            }
        }
    }
    auto* commands = dialog.findChild<QTreeWidget*>();
    REQUIRE(commands != nullptr);
    REQUIRE(commands->topLevelItemCount() > 0);
    QTreeWidgetItem* command = commands->topLevelItem(0)->child(0);
    REQUIRE(command != nullptr);
    commands->setCurrentItem(command);
    REQUIRE(add->isEnabled());
    add->click();
    const QString commandId = command->data(0, Qt::UserRole).toString();
    CHECK(dialog.applyButton()->isEnabled());
    CHECK(dialog.windowTitle() == QStringLiteral("Customize Toolbars *"));
    CHECK(manager.getToolbarCommands(id).isEmpty());

    SECTION("Apply saves the toolbar and keeps the dialog open") {
        dialog.applyButton()->click();
        CHECK(manager.getToolbarCommands(id) == QStringList{commandId});
        CHECK_FALSE(dialog.applyButton()->isEnabled());
        CHECK(dialog.isVisible());
    }

    SECTION("OK saves the toolbar and closes the dialog") {
        dialog.acceptButton()->click();
        CHECK(manager.getToolbarCommands(id) == QStringList{commandId});
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK_FALSE(dialog.isVisible());
    }

    SECTION("Cancel leaves the toolbar as it was") {
        dialog.cancelButton()->click();
        CHECK(manager.getToolbarCommands(id).isEmpty());
        CHECK(dialog.result() == QDialog::Rejected);
    }

    manager.deleteUserToolbar(id);
}
