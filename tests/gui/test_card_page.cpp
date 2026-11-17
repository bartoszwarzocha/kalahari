// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_page.cpp
/// @brief The cards of characters, locations and items: short in the Properties panel, full
/// on a tab beside the chapters, their portraits and colors, and Settings > Cards

#include "../editor/editor_test_utils.h"
#include "document_window.h"
#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/polish.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/relation_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/card_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/cursor_card_box.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/widgets/card_chapters_box.h"
#include "kalahari/gui/widgets/card_name_box.h"
#include "kalahari/gui/widgets/name_forms_table.h"
#include "kalahari/gui/widgets/own_fields_box.h"
#include "kalahari/gui/widgets/palette_edit.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFocusEvent>
#include <QGroupBox>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <nlohmann/json_fwd.hpp>

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kalahari {

namespace {

constexpr const char* BUILT_IN = KALAHARI_SOURCE_DIR "/resources/booktypes";

/// The tests of the keys need an active window; the offscreen platform has one without a
/// window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

/// The kind @p id of the cards of the Base
core::KindRef cardKind(const core::ProjectManager& pm, const char* id) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(id));
    REQUIRE(kind);
    REQUIRE(kind.kind->isCard());
    return kind;
}

/// A new card @p title of the kind @p kindId in the Workshop of the open book
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title) {
    const QString id = pm.addElement(cardKind(pm, kindId), title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// A picture of @p size in folder @p dir: red in its top left quarter, blue elsewhere
QString writePicture(const QString& dir, const QString& name, const QSize& size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::blue);
    for (int y = 0; y < size.height() / 2; ++y) {
        for (int x = 0; x < size.width() / 2; ++x) {
            image.setPixelColor(x, y, Qt::red);
        }
    }
    const QString path = QDir(dir).filePath(name);
    REQUIRE(image.save(path, "PNG"));
    return path;
}

/// A copy of picture @p path in the Workshop of the open book, as material @p title
QString addPicture(core::ProjectManager& pm, const QString& path, const QString& title) {
    const QString id = pm.addMaterial(
        path,
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                core::MaterialKinds::ofFile(path)),
        title);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// Whether the pixel at @p x, @p y of @p image is red, and not blue
bool isRed(const QImage& image, int x, int y) {
    const QRgb pixel = image.pixel(x, y);
    return qRed(pixel) > 200 && qBlue(pixel) < 50;
}

/// Whether the pixel at @p x, @p y of @p image is blue, and not red
bool isBlue(const QImage& image, int x, int y) {
    const QRgb pixel = image.pixel(x, y);
    return qBlue(pixel) > 200 && qRed(pixel) < 50;
}

/// The portrait that @p page shows, as a picture of @p side pixels
QImage shownPortrait(const gui::CardPage& page, int side) {
    return page.portraitButton()->icon().pixmap(QSize(side, side), 1.0).toImage();
}

/// The .klh file of the open book, as it is on the disk
QByteArray manifestFile(const core::ProjectManager& pm) {
    QFile file(pm.getManifestPath());
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// Key @p key with @p modifiers sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// @p widget loses the keys
void loseKeys(QWidget* widget) {
    QFocusEvent event(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(widget, &event);
}

/// @p widget gets the keys
void getKeys(QWidget* widget) {
    QFocusEvent event(QEvent::FocusIn, Qt::TabFocusReason);
    QCoreApplication::sendEvent(widget, &event);
}

/// No field of @p page has the keys: the test gives them the keys and the focus events
/// itself. A window that becomes active gives the keys to its first field when the system
/// activates it (macOS to the name, the first field that takes them with Tab there), and a
/// field with the keys keeps what it shows while the card changes elsewhere
void takeKeysOff(const QWidget& page) {
    if (QWidget* focused = QApplication::focusWidget();
        focused != nullptr && page.isAncestorOf(focused)) {
        focused->clearFocus();
    }
}

/// The names that the line of a field of the type "card" suggests now
QStringList suggestions(const QLineEdit* line) {
    REQUIRE(line->completer() != nullptr);
    const auto* model = qobject_cast<const QStringListModel*>(line->completer()->model());
    REQUIRE(model != nullptr);
    return model->stringList();
}

/// The widgets that Tab goes through from @p first, in order, until it comes back to it
QList<QWidget*> tabChain(QWidget* first) {
    QList<QWidget*> chain{first};
    const QWidget* window = first->window();
    for (QWidget* widget = first->nextInFocusChain(); widget != first;
         widget = widget->nextInFocusChain()) {
        const Qt::FocusPolicy policy = widget->focusPolicy();
        const bool tabs =
            policy == Qt::TabFocus || policy == Qt::StrongFocus || policy == Qt::WheelFocus;
        if (tabs && widget->isVisibleTo(window) &&
            widget->isEnabled() && widget->focusProxy() == nullptr) {
            chain << widget;
        }
    }
    return chain;
}

/// The texts of the labels of @p widget
QStringList labelsOf(const QWidget& widget) {
    QStringList texts;
    for (const QLabel* label : widget.findChildren<QLabel*>()) {
        texts << label->text();
    }
    return texts;
}

/// Run @p command; each message window that it shows is closed
/// @return The title and the texts of each of them
QStringList messagesOf(const std::function<void()>& command) {
    QStringList messages;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&messages]() {
        QDialog* dialog = test::shownDialog();
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        messages << test::textsOf(*dialog);
        dialog->reject();
    });
    timer.start();
    command();
    return messages;
}

/// Run @p command; in each color window that it opens, @p color is chosen
/// @return The number of color windows
int choosing(const std::function<void()>& command, const QColor& color) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &color]() {
        auto* dialog = qobject_cast<gui::dialogs::ColorDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        dialog->setColor(color);
        dialog->accept();
    });
    timer.start();
    command();
    return shown;
}

/// Run @p command, which may open a menu; the menu is closed
/// @return The number of menus it opened
int closingMenus(const std::function<void()>& command) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown]() {
        if (QWidget* popup = QApplication::activePopupWidget()) {
            ++shown;
            popup->close();
        }
    });
    timer.start();
    command();
    return shown;
}

/// The action of @p menu with object name @p name
QAction* actionOf(const QMenu* menu, const char* name) {
    for (QAction* action : menu->actions()) {
        if (action->objectName() == QLatin1String(name)) {
            return action;
        }
    }
    return nullptr;
}

/// The texts of the actions of @p menu, a separator as "-"
QStringList textsOf(const QMenu* menu) {
    QStringList texts;
    for (const QAction* action : menu->actions()) {
        texts << (action->isSeparator() ? QStringLiteral("-") : action->text());
    }
    return texts;
}

/// The item of the Navigator with the element @p id
QTreeWidgetItem* navigatorItem(QTreeWidget* tree, const QString& id) {
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id) {
            return *it;
        }
    }
    return nullptr;
}

using test::ChangedSetting;

/// The recent colors of the color window, as they were before the test
class RecentColors {
public:
    RecentColors()
        : m_saved(core::SettingsManager::getInstance().get<std::vector<std::string>>(
              "ui.recentColors", {})) {}
    ~RecentColors() {
        try {
            core::SettingsManager::getInstance().set<std::vector<std::string>>("ui.recentColors",
                                                                               m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the colors of this one
            static_cast<void>(
                std::fputs("RecentColors: the recent colors of the test stay\n", stderr));
        }
    }
    RecentColors(const RecentColors&) = delete;
    RecentColors& operator=(const RecentColors&) = delete;
    RecentColors(RecentColors&&) = delete;
    RecentColors& operator=(RecentColors&&) = delete;

private:
    std::vector<std::string> m_saved;
};

using test::Polish;

/// Folder @p from copied to @p to, with what it has
void copyFolder(const QString& from, const QString& to) {
    QDirIterator it(from, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = QDir(to).filePath(QDir(from).relativeFilePath(source));
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(source, target));
    }
}

/// A novel "Rain" with card "Anna", made where the program had the novel and open where it
/// does not have it: read-only, until the test ends
struct ReadOnlyNovel {
    QTemporaryDir booksDir;
    QTemporaryDir typesDir;
    core::ProjectManager& pm = test::book();
    QString anna;
    QString jan;

    ReadOnlyNovel() {
        REQUIRE(booksDir.isValid());
        REQUIRE(typesDir.isValid());
        core::ProjectManager::loadBookTypes({QString::fromUtf8(BUILT_IN)});
        REQUIRE(pm.createProject(booksDir.path(), QStringLiteral("Rain"),
                                 QStringLiteral("Author"), QStringLiteral("en"), false,
                                 QStringLiteral("kalahari.novel")));
        anna = addCard(pm, "character", QStringLiteral("Anna"));
        REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("29")));
        REQUIRE(pm.setDescription(anna, QStringLiteral("A cartographer.")));
        jan = addCard(pm, "character", QStringLiteral("Jan"));
        REQUIRE(pm.addRelation(anna, jan, QStringLiteral("friend"), QStringLiteral("old")));
        // The name of Anna in the book's dictionary, with another name
        core::DictionaryName name = core::nameOfCard(
            pm.bookDictionary(), anna, QStringLiteral("Anna"), core::NameKind::Character,
            core::NameDeclension(QStringLiteral("en")));
        core::NameVariant ania;
        ania.text = QStringLiteral("Ania");
        name.variants.append(ania);
        REQUIRE(pm.changeBookDictionary(
            [&name](core::BookDictionary& dictionary) { dictionary.putName(name); }));
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.closeProject(false));

        // Another computer, without the novel
        const QString types = typesDir.filePath(QStringLiteral("booktypes"));
        copyFolder(QString::fromUtf8(BUILT_IN), types);
        for (const char* id : {"kalahari.novel", "kalahari.short_stories"}) {
            REQUIRE(QDir(QDir(types).filePath(QLatin1String(id))).removeRecursively());
        }
        core::ProjectManager::loadBookTypes({types});
        REQUIRE(pm.openProject(manifest));
        REQUIRE(pm.isReadOnly());
    }
    ~ReadOnlyNovel() {
        try {
            if (pm.isProjectOpen()) {
                pm.closeProject(false);
            }
            core::ProjectManager::loadBookTypes({QString::fromUtf8(BUILT_IN)});
        } catch (...) {
            // A destructor may not throw: the later tests then find this book or its types
            static_cast<void>(std::fputs(
                "ReadOnlyNovel: the book or the book types of the test stay\n", stderr));
        }
    }
    ReadOnlyNovel(const ReadOnlyNovel&) = delete;
    ReadOnlyNovel& operator=(const ReadOnlyNovel&) = delete;
    ReadOnlyNovel(ReadOnlyNovel&&) = delete;
    ReadOnlyNovel& operator=(ReadOnlyNovel&&) = delete;
};

}  // namespace

TEST_CASE("Card: the initials of a portrait are the first letters of the first two words",
          "[gui][cards]") {
    CHECK(gui::CardPage::initialsOf(QStringLiteral("Anna Wolska")) == QStringLiteral("AW"));
    CHECK(gui::CardPage::initialsOf(QStringLiteral("  anna   maria wolska ")) ==
          QStringLiteral("AM"));
    CHECK(gui::CardPage::initialsOf(QStringLiteral("„Kruk” z Doliny")) == QStringLiteral("KZ"));
    CHECK(gui::CardPage::initialsOf(QStringLiteral("Łódź")) == QStringLiteral("Ł"));
    CHECK(gui::CardPage::initialsOf(QStringLiteral("1410")) == QStringLiteral("1"));
    CHECK(gui::CardPage::initialsOf(QStringLiteral("– … –")).isEmpty());
    CHECK(gui::CardPage::initialsOf(QString()).isEmpty());
}

TEST_CASE("Card: the short card shows the fields of its kind marked for the Properties panel "
          "and saves each change at once",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));

    gui::CardPage page(kalahari::test::book(), false);
    QStringList opened;
    QStringList titles;
    int settings = 0;
    QObject::connect(&page, &gui::CardPage::requestOpenCard,
                     [&opened](const QString& id) { opened << id; });
    QObject::connect(&page, &gui::CardPage::titleChanged,
                     [&titles](const QString& id) { titles << id; });
    QObject::connect(&page, &gui::CardPage::requestSettings, [&settings]() { ++settings; });
    page.resize(320, 900);
    page.showCard(anna);
    page.show();
    QApplication::processEvents();

    // Its group names its kind; the gear stands at the right of its title, above the name
    CHECK_FALSE(page.isFull());
    CHECK(page.elementId() == anna);
    REQUIRE(page.infoGroup() != nullptr);
    CHECK(page.infoGroup()->title() == QStringLiteral("Character Information"));
    CHECK(page.settingsButton()->parentWidget() == page.infoGroup());
    const QRect gear = page.settingsButton()->geometry();
    CHECK(gear.right() < page.infoGroup()->width());
    CHECK(gear.left() > page.infoGroup()->width() / 2);
    CHECK(gear.bottom() < page.nameEdit()->mapTo(page.infoGroup(), QPoint()).y());
    CHECK(page.nameEdit()->text() == QStringLiteral("Anna Wolska"));

    // The parts of the full card are not on it
    CHECK(page.subtitleLabel() == nullptr);
    CHECK(page.colorButton() == nullptr);
    CHECK(page.ownFields() == nullptr);
    CHECK(page.descriptionGroup() == nullptr);
    REQUIRE(page.openButton() != nullptr);

    // The role, chosen from a list, right under the name; the other fields with their
    // captions under In short
    auto* role = qobject_cast<QComboBox*>(page.fieldEditor(QStringLiteral("role")));
    REQUIRE(role != nullptr);
    CHECK(role->accessibleName() == QStringLiteral("Role"));
    REQUIRE(role->count() == 5);
    CHECK(role->itemText(0).isEmpty());
    CHECK(role->itemText(1) == QStringLiteral("main"));
    CHECK(role->itemData(1).toString() == QStringLiteral("main"));
    CHECK(role->currentIndex() == 0);
    const int shortTop = page.shortEdit()->mapTo(&page, QPoint()).y();
    CHECK(role->mapTo(&page, QPoint()).y() < shortTop);
    auto* age = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("age")));
    auto* occupation = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("occupation")));
    auto* goal = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("goal")));
    REQUIRE(age != nullptr);
    REQUIRE(occupation != nullptr);
    REQUIRE(goal != nullptr);
    CHECK(age->mapTo(&page, QPoint()).y() > shortTop);
    for (const char* hidden : {"gender", "appearance", "personality", "origin", "speech"}) {
        INFO(hidden);
        CHECK(page.fieldEditor(QLatin1String(hidden)) == nullptr);
    }
    const QStringList captions = labelsOf(page);
    CHECK(captions.contains(QStringLiteral("Age:")));
    CHECK(captions.contains(QStringLiteral("Occupation:")));
    CHECK(captions.contains(QStringLiteral("Goal:")));
    CHECK_FALSE(captions.contains(QStringLiteral("Role:")));

    // The keys go through the card as it reads
    CHECK(tabChain(page.settingsButton()) ==
          QList<QWidget*>{page.settingsButton(), page.portraitButton(), page.nameEdit(), role,
                          page.shortEdit(), age, occupation, goal,
                          page.nameBox()->nameButton(), page.relationsBox()->addButton(),
                          page.openButton()});

    // A value chosen is saved at once in the .klh file
    role->setCurrentIndex(1);
    emit role->activated(1);
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("role")) == QStringLiteral("main"));
    CHECK(manifestFile(pm).contains("\"main\""));

    // A line when Enter is pressed, without the spaces around it
    age->setText(QStringLiteral(" 29 "));
    press(age, Qt::Key_Return);
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("age")) == QStringLiteral("29"));
    occupation->setText(QStringLiteral("cartographer"));
    press(occupation, Qt::Key_Enter);
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("occupation")) ==
          QStringLiteral("cartographer"));
    // An empty line takes the value out
    occupation->clear();
    press(occupation, Qt::Key_Return);
    CHECK_FALSE(pm.findElement(anna)->card.values.contains(QStringLiteral("occupation")));

    // In short when it loses the keys
    page.shortEdit()->setPlainText(QStringLiteral("A cartographer from Warsaw."));
    loseKeys(page.shortEdit());
    CHECK(pm.findElement(anna)->description == QStringLiteral("A cartographer from Warsaw."));
    CHECK(manifestFile(pm).contains("A cartographer from Warsaw."));

    // Enter in the name renames the card; an empty name is not taken
    page.nameEdit()->setText(QStringLiteral("Anna Nowak"));
    press(page.nameEdit(), Qt::Key_Return);
    CHECK(pm.findElement(anna)->title == QStringLiteral("Anna Nowak"));
    CHECK(titles == QStringList{anna});
    page.nameEdit()->setText(QStringLiteral("   "));
    press(page.nameEdit(), Qt::Key_Return);
    CHECK(pm.findElement(anna)->title == QStringLiteral("Anna Nowak"));
    CHECK(page.nameEdit()->text() == QStringLiteral("Anna Nowak"));
    CHECK(titles.size() == 1);

    // A change made elsewhere shows at once
    takeKeysOff(page);
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("30")));
    CHECK(age->text() == QStringLiteral("30"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("minor")));
    CHECK(role->currentData().toString() == QStringLiteral("minor"));
    REQUIRE(pm.setDescription(anna, QStringLiteral("A painter.")));
    CHECK(page.shortEdit()->toPlainText() == QStringLiteral("A painter."));
    REQUIRE(pm.renameElement(anna, QStringLiteral("Anka")));
    CHECK(page.nameEdit()->text() == QStringLiteral("Anka"));
    // A value the list does not have stays, shown as it is saved
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("hero")));
    CHECK(role->currentText() == QStringLiteral("hero"));
    CHECK(role->count() == 6);

    // Open Card asks for the full card, the gear for Settings > Cards
    page.openButton()->click();
    CHECK(opened == QStringList{anna});
    page.settingsButton()->click();
    CHECK(settings == 1);

    // Opened again, the book has it all
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    takeKeysOff(page);
    page.showCard(anna);
    CHECK(page.nameEdit()->text() == QStringLiteral("Anka"));
    CHECK(page.shortEdit()->toPlainText() == QStringLiteral("A painter."));
    auto* reopenedAge = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("age")));
    REQUIRE(reopenedAge != nullptr);
    CHECK(reopenedAge->text() == QStringLiteral("30"));

    // Another kind has its own fields: the type of an item under its name, its owner among
    // the characters of the book, its place among the locations
    const QString jan = addCard(pm, "character", QStringLiteral("jan Kos"));
    const QString warsaw = addCard(pm, "location", QStringLiteral("Warsaw"));
    const QString lamp = addCard(pm, "item", QStringLiteral("Lamp"));
    page.showCard(lamp);
    CHECK(page.infoGroup()->title() == QStringLiteral("Item Information"));
    CHECK(page.fieldEditor(QStringLiteral("role")) == nullptr);
    auto* type = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("type")));
    REQUIRE(type != nullptr);
    CHECK(type->mapTo(&page, QPoint()).y() < page.shortEdit()->mapTo(&page, QPoint()).y());
    auto* owner = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("owner")));
    auto* place = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("place")));
    REQUIRE(owner != nullptr);
    REQUIRE(place != nullptr);
    CHECK(owner->placeholderText() == QStringLiteral("Type or choose a card..."));
    getKeys(owner);
    CHECK(suggestions(owner) == QStringList{QStringLiteral("Anka"), QStringLiteral("jan Kos")});
    getKeys(place);
    CHECK(suggestions(place) == QStringList{QStringLiteral("Warsaw")});
    // The names as they are when the line gets the keys
    REQUIRE(pm.renameElement(jan, QStringLiteral("Jan Kos")));
    getKeys(owner);
    CHECK(suggestions(owner) == QStringList{QStringLiteral("Anka"), QStringLiteral("Jan Kos")});
    // The name of a card: the relation with it, which both cards show
    owner->setText(QStringLiteral("Jan Kos"));
    press(owner, Qt::Key_Return);
    CHECK(pm.findElement(lamp)->card.value(QStringLiteral("owner")).isEmpty());
    CHECK(pm.cardFieldText(lamp, QStringLiteral("owner")) == QStringLiteral("Jan Kos"));
    REQUIRE(page.relationsBox()->list()->topLevelItemCount() == 1);
    CHECK(page.relationsBox()->list()->topLevelItem(0)->text(1) == QStringLiteral("Jan Kos"));
    // Another text: its value
    owner->setText(QStringLiteral("Somebody"));
    press(owner, Qt::Key_Return);
    CHECK(pm.findElement(lamp)->card.value(QStringLiteral("owner")) == QStringLiteral("Somebody"));
    CHECK(page.relationsBox()->list()->topLevelItemCount() == 0);
    CHECK(page.relationsBox()->emptyLabel()->isVisibleTo(&page));

    // A location is not a part of itself
    const QString mokotow = addCard(pm, "location", QStringLiteral("Mokotów"));
    page.showCard(mokotow);
    auto* parent = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("parent")));
    REQUIRE(parent != nullptr);
    getKeys(parent);
    CHECK(suggestions(parent) == QStringList{QStringLiteral("Warsaw")});
    CHECK(pm.findElement(warsaw) != nullptr);
}

TEST_CASE("Card: a field with the keys keeps what is typed in it while the card changes",
          "[gui][cards]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));

    gui::CardPage page(kalahari::test::book(), true);
    page.resize(900, 700);
    page.showCard(anna);
    page.show();
    page.activateWindow();
    REQUIRE(test::waitUntil([&page] { return QApplication::activeWindow() == &page; }));

    // Typed in the age, not saved yet: another field changed elsewhere leaves it
    auto* age = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("age")));
    REQUIRE(age != nullptr);
    age->setFocus(Qt::OtherFocusReason);
    REQUIRE(test::waitUntil([age] { return age->hasFocus(); }));
    age->setText(QStringLiteral("31"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("goal"), QStringLiteral("A map of the sea")));
    CHECK(age->text() == QStringLiteral("31"));
    auto* goal = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("goal")));
    REQUIRE(goal != nullptr);
    CHECK(goal->text() == QStringLiteral("A map of the sea"));

    // So does the name
    page.focusName();
    REQUIRE(test::waitUntil([&page] { return page.nameEdit()->hasFocus(); }));
    CHECK(page.nameEdit()->selectedText() == QStringLiteral("Anna"));
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("age")) == QStringLiteral("31"));
    page.nameEdit()->setText(QStringLiteral("Anna W"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("32")));
    CHECK(page.nameEdit()->text() == QStringLiteral("Anna W"));
    CHECK(age->text() == QStringLiteral("32"));

    // And an own field being edited: the table is not filled anew
    REQUIRE(pm.setFields(anna, {core::ElementField{QStringLiteral("Eyes"),
                                                   QStringLiteral("green"), {}}}));
    QTableWidget* table = page.ownFields()->table();
    REQUIRE(table->rowCount() == 1);
    table->editItem(table->item(0, 1));
    auto* cell = qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(0, 1)));
    REQUIRE(cell != nullptr);
    cell->setText(QStringLiteral("grey"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("goal"), QStringLiteral("A map of the land")));
    CHECK(table->indexWidget(table->model()->index(0, 1)) == cell);
    CHECK(cell->text() == QStringLiteral("grey"));
    // Its fields changed elsewhere fill it anew
    REQUIRE(pm.setFields(anna, {core::ElementField{QStringLiteral("Eyes"),
                                                   QStringLiteral("blue"), {}}}));
    CHECK(table->item(0, 1)->text() == QStringLiteral("blue"));
}

TEST_CASE("Card: the full card has the kind with the role, the color, the data, the own fields "
          "and the description",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const RecentColors recentColors;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));

    gui::CardPage page(kalahari::test::book(), true);
    int settings = 0;
    QObject::connect(&page, &gui::CardPage::requestSettings, [&settings]() { ++settings; });
    auto* description = new QPlainTextEdit();
    page.setDescriptionWidget(description);
    page.resize(900, 700);
    page.showCard(anna);
    page.show();
    QApplication::processEvents();

    // Its header: the name larger than the text, the kind with the role beside it
    CHECK(page.isFull());
    CHECK(page.infoGroup() == nullptr);
    CHECK(page.openButton() == nullptr);
    CHECK(page.nameEdit()->font().bold());
    CHECK(page.nameEdit()->fontInfo().pixelSize() > page.fontInfo().pixelSize());
    REQUIRE(page.subtitleLabel() != nullptr);
    CHECK(page.subtitleLabel()->text() == QStringLiteral("Character"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("main")));
    CHECK(page.subtitleLabel()->text() == QStringLiteral("Character · main"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("hero")));
    CHECK(page.subtitleLabel()->text() == QStringLiteral("Character · hero"));

    // The gear on the bar over the data, which scroll
    CHECK(page.settingsButton()->parentWidget()->objectName() == QStringLiteral("cardBar"));
    page.settingsButton()->click();
    CHECK(settings == 1);

    // The fields shown on the card, each with its caption, in the order of the kind
    for (const char* shown :
         {"role", "gender", "age", "occupation", "appearance", "personality", "goal"}) {
        INFO(shown);
        CHECK(page.fieldEditor(QLatin1String(shown)) != nullptr);
    }
    for (const char* hidden : {"origin", "motivation", "secret", "change", "speech"}) {
        INFO(hidden);
        CHECK(page.fieldEditor(QLatin1String(hidden)) == nullptr);
    }
    CHECK(labelsOf(page).contains(QStringLiteral("Role:")));
    QWidget* role = page.fieldEditor(QStringLiteral("role"));
    QWidget* goal = page.fieldEditor(QStringLiteral("goal"));
    CHECK(role->mapTo(&page, QPoint()).y() < goal->mapTo(&page, QPoint()).y());
    CHECK(role->mapTo(&page, QPoint()).y() >
          page.shortEdit()->mapTo(&page, QPoint()).y());

    // A longer text has a few lines, saved when it loses the keys
    auto* appearance =
        qobject_cast<QPlainTextEdit*>(page.fieldEditor(QStringLiteral("appearance")));
    REQUIRE(appearance != nullptr);
    CHECK(appearance->tabChangesFocus());
    CHECK(appearance->height() > appearance->fontMetrics().lineSpacing() * 2);
    appearance->setPlainText(QStringLiteral("Tall, with grey eyes.\nA scar on her hand."));
    loseKeys(appearance);
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("appearance")) ==
          QStringLiteral("Tall, with grey eyes.\nA scar on her hand."));
    auto* gender = qobject_cast<QComboBox*>(page.fieldEditor(QStringLiteral("gender")));
    REQUIRE(gender != nullptr);
    gender->setCurrentIndex(gender->findData(QStringLiteral("woman")));
    emit gender->activated(gender->currentIndex());
    CHECK(pm.findElement(anna)->card.value(QStringLiteral("gender")) == QStringLiteral("woman"));

    // The data beside the description, the text of the card
    REQUIRE(page.descriptionGroup() != nullptr);
    CHECK(page.descriptionGroup()->isAncestorOf(description));
    const auto* data = page.findChild<QGroupBox*>(QStringLiteral("cardDataGroup"));
    REQUIRE(data != nullptr);
    CHECK(data->mapTo(&page, QPoint()).x() <
          page.descriptionGroup()->mapTo(&page, QPoint()).x());
    CHECK(page.descriptionGroup()->width() > data->width());
    CHECK(page.descriptionGroup()->height() >= 320);

    // The own fields under the data
    REQUIRE(page.ownFields() != nullptr);
    CHECK(page.ownFields()->table()->rowCount() == 0);
    REQUIRE(pm.setFields(anna, {core::ElementField{QStringLiteral("Eyes"),
                                                   QStringLiteral("grey"), {}}}));
    REQUIRE(page.ownFields()->table()->rowCount() == 1);
    CHECK(page.ownFields()->table()->item(0, 0)->text() == QStringLiteral("Eyes"));
    CHECK(page.ownFields()->mapTo(&page, QPoint()).y() > data->mapTo(&page, QPoint()).y());

    // The color of the card on its button; a click opens the color window
    const QString first = pm.findElement(anna)->card.color;
    REQUIRE(QColor(first).isValid());
    REQUIRE(page.colorButton() != nullptr);
    CHECK(page.colorButton()->accessibleDescription() == first);
    const QImage swatch = page.colorButton()->icon().pixmap(QSize(16, 16), 1.0).toImage();
    CHECK(swatch.pixelColor(8, 8) == QColor(first));
    CHECK(choosing([&page]() { page.colorButton()->click(); }, QColor(QStringLiteral("#123456"))) ==
          1);
    CHECK(pm.findElement(anna)->card.color == QStringLiteral("#123456"));
    CHECK(manifestFile(pm).contains("#123456"));
    CHECK(page.colorButton()->accessibleDescription() == QStringLiteral("#123456"));
    // A color closed without a change changes nothing
    CHECK(choosing([&page]() { page.chooseColor(); }, QColor(QStringLiteral("#123456"))) == 1);
    CHECK(pm.findElement(anna)->card.color == QStringLiteral("#123456"));

    // The keys go through the bar and the header, then the data, the declension, the own
    // fields, the description and the relations
    const QList<QWidget*> chain = tabChain(page.settingsButton());
    REQUIRE(chain.size() > 6);
    CHECK(chain.mid(0, 6) == QList<QWidget*>{page.settingsButton(), page.portraitButton(),
                                             page.nameEdit(), page.colorButton(),
                                             page.shortEdit(), role});
    CHECK(chain.indexOf(goal) < chain.indexOf(page.nameBox()->nameButton()));
    CHECK(chain.indexOf(page.nameBox()->nameButton()) <
          chain.indexOf(page.ownFields()->addButton()));
    CHECK(chain.indexOf(page.ownFields()->table()) < chain.indexOf(description));
    CHECK(chain.indexOf(description) < chain.indexOf(page.relationsBox()->addButton()));
    REQUIRE(chain.size() > 2);
    CHECK(chain.at(chain.size() - 2) == page.relationButton());
    CHECK(chain.last() == page.fieldsButton());
}

TEST_CASE("Card: the portrait is a picture of the materials, from a file, or the initials on "
          "the color",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    core::CardData colored = pm.findElement(anna)->card;
    colored.color = QStringLiteral("#2e9e6a");
    REQUIRE(pm.setCard(anna, colored));

    gui::CardPage page(kalahari::test::book(), false);
    QStringList added;
    QObject::connect(&page, &gui::CardPage::materialAdded,
                     [&added](const QString& id) { added << id; });
    page.resize(320, 900);
    page.showCard(anna);
    page.show();
    QApplication::processEvents();

    // Without a picture: the initials on the color of the card
    const int side = gui::CardPage::SHORT_PORTRAIT;
    CHECK(page.portraitButton()->iconSize() == QSize(side, side));
    QImage portrait = shownPortrait(page, side);
    REQUIRE(portrait.size() == QSize(side, side));
    CHECK(portrait.pixelColor(2, 2) == QColor(QStringLiteral("#2e9e6a")));
    bool lettered = false;
    for (int x = 0; x < side && !lettered; ++x) {
        lettered = portrait.pixelColor(x, side / 2) != QColor(QStringLiteral("#2e9e6a"));
    }
    CHECK(lettered);

    // The menu of the portrait: the pictures of the materials, a file, no portrait
    QMenu* menu = page.portraitMenu();
    emit menu->aboutToShow();
    CHECK(textsOf(menu) == QStringList{QStringLiteral("Picture from the &Materials"),
                                       QStringLiteral("Picture from a &File..."),
                                       QStringLiteral("-"), QStringLiteral("&Remove Portrait")});
    REQUIRE(actionOf(menu, "cardPortraitFile") != nullptr);
    CHECK(actionOf(menu, "cardPortraitFile")->isEnabled());
    REQUIRE(actionOf(menu, "cardPortraitRemove") != nullptr);
    CHECK_FALSE(actionOf(menu, "cardPortraitRemove")->isEnabled());
    // No pictures yet
    CHECK_FALSE(menu->actions().constFirst()->isEnabled());

    // A picture of the materials: the portrait shows a square of it, near its top, and the
    // picture is linked with the card
    const QString sea =
        addPicture(pm, writePicture(files.path(), QStringLiteral("sea.png"), QSize(400, 200)),
                   QStringLiteral("Sea & Sky"));
    const QString face =
        addPicture(pm, writePicture(files.path(), QStringLiteral("face.png"), QSize(200, 400)),
                   QStringLiteral("Face"));
    page.setPortrait(sea);
    CHECK(pm.findElement(anna)->card.portraitImage() == sea);
    CHECK(pm.findElement(sea)->linkTo(anna) != nullptr);
    portrait = shownPortrait(page, side);
    CHECK(isRed(portrait, 10, 10));
    CHECK(isBlue(portrait, side - 10, 10));
    CHECK(isBlue(portrait, 10, side - 10));
    // A tall picture: the square near its top, where a face is
    const QPixmap tall = gui::CardPage::portraitOf(
        pm, [&pm, &face]() {
            core::ProjectElement element = *pm.findElement(face);
            element.card.portraits = {core::CardPortrait{face, QString(), {}}};
            return element;
        }(),
        side, 1.0);
    const QImage tallImage = tall.toImage();
    CHECK(tallImage.size() == QSize(side, side));
    // The picture is red in its top left quarter: the top half of the square is half red
    CHECK(isRed(tallImage, 10, 10));
    CHECK(isBlue(tallImage, side - 10, 10));

    // The linked pictures first in the menu, the portrait checked
    emit menu->aboutToShow();
    auto* pictures = menu->findChild<QMenu*>(QStringLiteral("cardPortraitPictures"));
    REQUIRE(pictures != nullptr);
    CHECK(pictures->isEnabled());
    CHECK(textsOf(pictures) ==
          QStringList{QStringLiteral("Sea && Sky"), QStringLiteral("-"), QStringLiteral("Face")});
    CHECK(pictures->actions().constFirst()->isChecked());
    CHECK_FALSE(pictures->actions().constLast()->isChecked());
    CHECK(actionOf(menu, "cardPortraitRemove")->isEnabled());

    // Another picture chosen comes first; the one before stays among the portraits
    pictures->actions().constLast()->trigger();
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 2);
    CHECK(pm.findElement(anna)->card.portraits.at(0).image == face);
    CHECK(pm.findElement(anna)->card.portraits.at(1).image == sea);
    CHECK(pm.findElement(face)->linkTo(anna) != nullptr);
    // Chosen again, it is not there twice
    page.setPortrait(sea);
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 2);
    CHECK(pm.findElement(anna)->card.portraits.at(0).image == sea);
    // A card or a chapter is no picture
    page.setPortrait(novel.chapterId);
    CHECK(pm.findElement(anna)->card.portraitImage() == sea);

    // Remove Portrait takes out the one shown; its picture stays in the materials
    actionOf(menu, "cardPortraitRemove")->trigger();
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 1);
    CHECK(pm.findElement(anna)->card.portraitImage() == face);
    page.removePortrait();
    CHECK(pm.findElement(anna)->card.portraits.isEmpty());
    CHECK(pm.findElement(sea) != nullptr);
    CHECK(pm.findElement(face) != nullptr);
    CHECK(shownPortrait(page, side).pixelColor(2, 2) == QColor(QStringLiteral("#2e9e6a")));

    // A picture from a file becomes a material linked with the card, and the portrait
    const QString file = writePicture(files.path(), QStringLiteral("anna.png"), QSize(64, 64));
    page.addPortraitFile(file);
    REQUIRE(added.size() == 1);
    const core::ProjectElement* material = pm.findElement(added.first());
    REQUIRE(material != nullptr);
    CHECK(pm.isMaterial(*material));
    CHECK(material->title == QStringLiteral("anna"));
    CHECK(material->linkTo(anna) != nullptr);
    CHECK(QFileInfo::exists(pm.filePathOf(*material)));
    CHECK(pm.findElement(anna)->card.portraitImage() == added.first());
    CHECK(isRed(shownPortrait(page, side), 10, 10));

    // A file that is no picture is not taken, and the writer is told why
    const QString text = QDir(files.path()).filePath(QStringLiteral("notes.png"));
    {
        QFile notes(text);
        REQUIRE(notes.open(QIODevice::WriteOnly));
        REQUIRE(notes.write("not a picture") == 13);
    }
    const QStringList messages = messagesOf([&page, &text]() { page.addPortraitFile(text); });
    REQUIRE(messages.size() == 1);
    CHECK(messages.first().contains(QStringLiteral("Portrait Not Added")));
    CHECK(messages.first().contains(
        QStringLiteral("The file \"notes.png\" is not a picture Kalahari can show.")));
    CHECK(added.size() == 1);

    // A picture deleted takes its portrait with it: the initials again
    REQUIRE(pm.removeElement(added.first()).has_value());
    CHECK(pm.findElement(anna)->card.portraits.isEmpty());
    CHECK(shownPortrait(page, side).pixelColor(2, 2) == QColor(QStringLiteral("#2e9e6a")));
}

TEST_CASE("Card: the menu of the portrait opens from the keys and with a click",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const QString anna = addCard(novel.pm, "character", QStringLiteral("Anna"));
    gui::CardPage page(kalahari::test::book(), true);
    page.showCard(anna);
    page.show();
    QApplication::processEvents();
    int filled = 0;
    QObject::connect(page.portraitMenu(), &QMenu::aboutToShow, [&filled]() { ++filled; });

    // Enter, the menu key and a click; another key does nothing
    CHECK(closingMenus([&page]() { press(page.portraitButton(), Qt::Key_Return); }) == 1);
    CHECK(closingMenus([&page]() {
              const QPoint place(4, 4);
              QContextMenuEvent event(QContextMenuEvent::Keyboard, place,
                                      page.portraitButton()->mapToGlobal(place));
              QCoreApplication::sendEvent(page.portraitButton(), &event);
          }) == 1);
    CHECK(closingMenus([&page]() { page.portraitButton()->click(); }) == 1);
    CHECK(filled == 3);
    CHECK(closingMenus([&page]() { press(page.portraitButton(), Qt::Key_A); }) == 0);
    CHECK(filled == 3);
}

TEST_CASE("Card: in Polish, the kind declined in the title of the group, the values of the "
          "lists and the keys of the menu of the portrait",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Polish polish;
    REQUIRE(polish.loaded());
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("main")));
    const QString lamp = addCard(pm, "item", QStringLiteral("Lampa"));

    gui::CardPage brief(kalahari::test::book(), false);
    brief.showCard(anna);
    CHECK(brief.infoGroup()->title() == QStringLiteral("Informacje o postaci"));
    auto* role = qobject_cast<QComboBox*>(brief.fieldEditor(QStringLiteral("role")));
    REQUIRE(role != nullptr);
    CHECK(role->currentText() == QStringLiteral("główna"));
    CHECK(labelsOf(brief).contains(QStringLiteral("Wiek:")));
    CHECK(labelsOf(brief).contains(QStringLiteral("W skrócie:")));
    CHECK(brief.openButton()->text() == QStringLiteral("Otwórz kartę"));
    brief.showCard(lamp);
    CHECK(brief.infoGroup()->title() == QStringLiteral("Informacje o przedmiocie"));
    auto* owner = qobject_cast<QLineEdit*>(brief.fieldEditor(QStringLiteral("owner")));
    REQUIRE(owner != nullptr);
    CHECK(owner->placeholderText() == QStringLiteral("Wpisz albo wybierz kartę..."));

    gui::CardPage full(kalahari::test::book(), true);
    full.showCard(anna);
    CHECK(full.subtitleLabel()->text() == QStringLiteral("Postać · główna"));
    CHECK(labelsOf(full).contains(QStringLiteral("Kolor karty:")));
    CHECK(full.descriptionGroup()->title() == QStringLiteral("Opis"));
    CHECK(full.ownFields()->title() == QStringLiteral("Własne pola"));

    // Each command of the menu of the portrait has a key of its own
    emit full.portraitMenu()->aboutToShow();
    CHECK(textsOf(full.portraitMenu()) ==
          QStringList{QStringLiteral("Obraz z &materiałów"), QStringLiteral("Obraz z &pliku..."),
                      QStringLiteral("-"), QStringLiteral("&Usuń portret")});
}

TEST_CASE("Card: a book open read-only shows its cards and changes nothing", "[gui][cards]") {
    gui::registerAllIcons();
    const ReadOnlyNovel novel;
    auto& pm = novel.pm;

    gui::CardPage full(kalahari::test::book(), true);
    gui::CardPage brief(kalahari::test::book(), false);
    for (gui::CardPage* page : {&full, &brief}) {
        INFO((page->isFull() ? "full card" : "short card"));
        page->showCard(novel.anna);
        CHECK(page->nameEdit()->text() == QStringLiteral("Anna"));
        CHECK(page->nameEdit()->isReadOnly());
        CHECK(page->shortEdit()->toPlainText() == QStringLiteral("A cartographer."));
        CHECK(page->shortEdit()->isReadOnly());
        auto* age = qobject_cast<QLineEdit*>(page->fieldEditor(QStringLiteral("age")));
        REQUIRE(age != nullptr);
        CHECK(age->text() == QStringLiteral("29"));
        CHECK(age->isReadOnly());
        CHECK_FALSE(page->fieldEditor(QStringLiteral("role"))->isEnabled());

        // Nothing typed is saved
        page->nameEdit()->setText(QStringLiteral("Anka"));
        press(page->nameEdit(), Qt::Key_Return);
        age->setText(QStringLiteral("30"));
        press(age, Qt::Key_Return);
        page->shortEdit()->setPlainText(QStringLiteral("A painter."));
        loseKeys(page->shortEdit());
        CHECK(pm.findElement(novel.anna)->title == QStringLiteral("Anna"));
        CHECK(pm.findElement(novel.anna)->card.value(QStringLiteral("age")) ==
              QStringLiteral("29"));
        CHECK(pm.findElement(novel.anna)->description == QStringLiteral("A cartographer."));

        // Nor is the menu of the portrait of use
        emit page->portraitMenu()->aboutToShow();
        for (const QAction* action : page->portraitMenu()->actions()) {
            INFO(action->text().toStdString());
            CHECK((action->isSeparator() || !action->isEnabled()));
        }
        page->removePortrait();
        CHECK(pm.findElement(novel.anna)->card.portraits.isEmpty());

        // The relations are shown, and the keys and the buttons change nothing
        gui::RelationsBox* relations = page->relationsBox();
        QTreeWidget* list = relations->list();
        REQUIRE(list->topLevelItemCount() == 1);
        QTreeWidgetItem* row = list->topLevelItem(0);
        if (row->childCount() > 0) {
            row = row->child(0);
        }
        CHECK(row->text(1) == QStringLiteral("Jan"));
        list->setCurrentItem(row);
        CHECK_FALSE(relations->addButton()->isEnabled());
        CHECK_FALSE(relations->removeButton()->isEnabled());
        CHECK(messagesOf([list]() {
                  press(list, Qt::Key_Delete);
                  press(list, Qt::Key_Insert);
                  press(list, Qt::Key_F2);
              }).isEmpty());
        CHECK(pm.cardRelations().of(novel.anna).size() == 1);
        emit list->customContextMenuRequested(QPoint(4, 4));
        const QList<QAction*> actions = relations->rowMenu()->actions();
        REQUIRE(actions.size() == 5);
        CHECK(actions.at(0)->isEnabled());
        for (const QAction* action : actions.mid(1)) {
            CHECK((action->isSeparator() || !action->isEnabled()));
        }
        relations->rowMenu()->close();
    }
    REQUIRE(full.colorButton() != nullptr);
    CHECK_FALSE(full.colorButton()->isEnabled());
    REQUIRE(full.relationButton() != nullptr);
    CHECK_FALSE(full.relationButton()->isEnabled());
    // Nor do the chapters and the materials of the card, nor what is dropped on it; the book is
    // searched all the same
    CHECK_FALSE(full.chaptersBox()->addButton()->isEnabled());
    CHECK_FALSE(full.materialsBox()->addButton()->isEnabled());
    gui::IncomingMaterials dropped;
    dropped.picture = QImage(QSize(8, 8), QImage::Format_RGB32);
    CHECK_FALSE(full.dropMaterials(dropped, QPoint(1, 1)));
    CHECK(full.findButton()->isEnabled());

    // The declension is shown, and changes nothing: no window Name, no other name, no form
    CHECK(brief.nameBox()->formsLabel()->text() == QStringLiteral("The name does not change."));
    CHECK(brief.nameBox()->otherNamesLabel()->text() == QStringLiteral("Other names: Ania"));
    CHECK_FALSE(brief.nameBox()->nameButton()->isEnabled());
    gui::CardNameBox* declension = full.nameBox();
    REQUIRE(declension->variantButtons().size() == 2);
    CHECK(declension->variantButtons().at(1)->isEnabled());  // the forms of each are shown
    CHECK_FALSE(declension->nameButton()->isEnabled());
    CHECK_FALSE(declension->addButton()->isEnabled());
    REQUIRE(declension->formsTable() != nullptr);
    CHECK_FALSE(declension->formsTable()->item(0, 1)->flags().testFlag(Qt::ItemIsEditable));
    int others = 0;
    QObject::connect(&full, &gui::CardPage::requestOtherName, [&others]() { ++others; });
    press(declension->formsTable(), Qt::Key_Insert);
    CHECK(others == 0);

    // Nor does the window Relation save or make a card
    const gui::dialogs::RelationDialog dialog(kalahari::test::book(), novel.anna, std::nullopt);
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.filterEdit()->setText(QStringLiteral("Zofia"));
    CHECK(dialog.cardList()->currentItem() == nullptr);
    for (int i = 0; i < dialog.cardList()->topLevelItemCount(); ++i) {
        CHECK(dialog.cardList()->topLevelItem(i)->isHidden());
    }
    CHECK(choosing([&full]() { full.chooseColor(); }, QColor(QStringLiteral("#123456"))) == 0);
}

TEST_CASE("Card: it opens on a tab beside the chapters, with its text as the description, and "
          "shows short in the Properties panel",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    test::Window window;
    int settings = 0;
    QObject::connect(&window.navigator, &gui::NavigatorCoordinator::cardSettingsRequested,
                     [&settings]() { ++settings; });

    // The full card on its tab, the editor of its text in its Description
    window.navigator.onElementSelected(anna, QStringLiteral("Anna Wolska"));
    REQUIRE(window.tabs->count() == 1);
    auto* panel = qobject_cast<gui::CardPanel*>(window.tabs->currentWidget());
    REQUIRE(panel != nullptr);
    CHECK(panel->objectName() == QStringLiteral("cardPanel"));
    CHECK(panel->elementId() == anna);
    REQUIRE(panel->page() != nullptr);
    CHECK(panel->page()->isFull());
    CHECK(panel->page()->elementId() == anna);
    REQUIRE(panel->getBookEditor() != nullptr);
    CHECK(panel->page()->descriptionGroup()->isAncestorOf(panel->getBookEditor()));
    CHECK(panel->getBookEditor()->accessibleName() == QStringLiteral("Description of the card"));
    CHECK_FALSE(panel->getBookEditor()->isReadOnly());
    CHECK(window.tabs->tabText(0).contains(QStringLiteral("Anna Wolska")));

    // Opened again, the same tab
    window.navigator.onElementSelected(anna, QStringLiteral("Anna Wolska"));
    CHECK(window.tabs->count() == 1);

    // A new name on the card: its tab and the Navigator show it
    panel->page()->nameEdit()->setText(QStringLiteral("Anna Nowak"));
    press(panel->page()->nameEdit(), Qt::Key_Return);
    CHECK(pm.findElement(anna)->title == QStringLiteral("Anna Nowak"));
    CHECK(window.tabs->tabText(0).contains(QStringLiteral("Anna Nowak")));
    QTreeWidgetItem* item = navigatorItem(window.navigatorPanel->tree(), anna);
    REQUIRE(item != nullptr);
    CHECK(item->text(0).contains(QStringLiteral("Anna Nowak")));

    // The gear asks for Settings > Cards
    panel->page()->settingsButton()->click();
    CHECK(settings == 1);

    // A chapter opens in the editor of the chapters
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    CHECK(window.tabs->count() == 2);
    CHECK(qobject_cast<gui::CardPanel*>(window.tabs->currentWidget()) == nullptr);

    // In the Properties panel, the short card
    gui::PropertiesPanel& properties = *window.properties;
    QStringList opened;
    int propertiesSettings = 0;
    QObject::connect(&properties, &gui::PropertiesPanel::requestOpenCard,
                     [&opened](const QString& id) { opened << id; });
    QObject::connect(&properties, &gui::PropertiesPanel::requestCardSettings,
                     [&propertiesSettings]() { ++propertiesSettings; });
    properties.showChapterProperties(anna);
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Card);
    REQUIRE(properties.cardPage() != nullptr);
    CHECK_FALSE(properties.cardPage()->isFull());
    CHECK(properties.cardPage()->elementId() == anna);
    CHECK(properties.cardPage()->nameEdit()->text() == QStringLiteral("Anna Nowak"));
    properties.cardPage()->openButton()->click();
    CHECK(opened == QStringList{anna});
    properties.cardPage()->settingsButton()->click();
    CHECK(propertiesSettings == 1);
    // A change on the full card shows on the short one
    auto* age = qobject_cast<QLineEdit*>(panel->page()->fieldEditor(QStringLiteral("age")));
    REQUIRE(age != nullptr);
    age->setText(QStringLiteral("29"));
    press(age, Qt::Key_Return);
    auto* shortAge =
        qobject_cast<QLineEdit*>(properties.cardPage()->fieldEditor(QStringLiteral("age")));
    REQUIRE(shortAge != nullptr);
    CHECK(shortAge->text() == QStringLiteral("29"));
    // Alt+Enter in the Navigator gives the keys to the name
    properties.focusFirstField();
    CHECK(properties.cardPage()->nameEdit()->selectedText() == QStringLiteral("Anna Nowak"));

    // A chapter has the page of the chapters; a card deleted leaves the book's page
    properties.showChapterProperties(novel.chapterId);
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Chapter);
    const QString lamp = addCard(pm, "item", QStringLiteral("Lamp"));
    properties.showChapterProperties(lamp);
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Card);
    REQUIRE(pm.removeElement(lamp).has_value());
    properties.refresh();
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Project);
}

TEST_CASE("Card: the Navigator shows the color of a card beside its name, as the settings say",
          "[gui][cards]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const ChangedSetting<bool> dots(core::CardColors::NAVIGATOR_DOT_SETTING, true);
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString lamp = addCard(pm, "item", QStringLiteral("Lamp"));
    test::Window window;
    gui::NavigatorPanel& navigator = *window.navigatorPanel;

    // A card has the dot of its color; a chapter has none
    QTreeWidgetItem* annaItem = navigatorItem(navigator.tree(), anna);
    REQUIRE(annaItem != nullptr);
    CHECK(gui::NavigatorPanel::cardColorOf(annaItem) == QColor(pm.findElement(anna)->card.color));
    CHECK(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), lamp)) ==
          QColor(pm.findElement(lamp)->card.color));
    CHECK_FALSE(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), novel.chapterId))
                    .isValid());

    // Another color: the dot follows
    core::CardData card = pm.findElement(anna)->card;
    card.color = QStringLiteral("#123456");
    REQUIRE(pm.setCard(anna, card));
    annaItem = navigatorItem(navigator.tree(), anna);
    REQUIRE(annaItem != nullptr);
    CHECK(gui::NavigatorPanel::cardColorOf(annaItem) == QColor(QStringLiteral("#123456")));

    // Without the dots in the settings: none
    core::SettingsManager::getInstance().set<bool>(core::CardColors::NAVIGATOR_DOT_SETTING,
                                                   false);
    navigator.onSettingsChanged();
    CHECK_FALSE(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), anna)).isValid());
    CHECK_FALSE(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), lamp)).isValid());
    window.navigator.refreshNavigator();
    CHECK_FALSE(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), anna)).isValid());
    core::SettingsManager::getInstance().set<bool>(core::CardColors::NAVIGATOR_DOT_SETTING, true);
    navigator.onSettingsChanged();
    CHECK(gui::NavigatorPanel::cardColorOf(navigatorItem(navigator.tree(), anna)) ==
          QColor(QStringLiteral("#123456")));
}

TEST_CASE("Card: the Navigator puts a location under the one it is part of, or all of them in "
          "a list, as the settings say",
          "[gui][cards][navigator]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const ChangedSetting<std::string> trees(gui::NavigatorPanel::CARDS_TREE_SETTING, "tree");
    const ChangedSetting<std::string> atCursor(gui::CursorCardBox::SETTING,
                                               gui::CursorCardBox::NAMES);
    const QString parent = QStringLiteral("parent");
    const QString warsaw = addCard(pm, "location", QStringLiteral("Warsaw"));
    const QString zoliborz = addCard(pm, "location", QStringLiteral("Zoliborz"));
    const QString mokotow = addCard(pm, "location", QStringLiteral("Mokotow"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Krakow"));
    REQUIRE(pm.setCardFieldText(zoliborz, parent, QStringLiteral("Warsaw")));
    REQUIRE(pm.setCardFieldText(mokotow, parent, QStringLiteral("Warsaw")));
    test::Window window;
    QTreeWidget* tree = window.navigatorPanel->tree();
    const auto itemOf = [tree](const QString& id) {
        QTreeWidgetItem* item = navigatorItem(tree, id);
        REQUIRE(item != nullptr);
        return item;
    };
    const auto titlesUnder = [](const QTreeWidgetItem* item) {
        QStringList titles;
        for (int i = 0; i < item->childCount(); ++i) {
            titles << item->child(i)->text(0);
        }
        return titles;
    };

    // The districts under their city, open, in alphabetical order; the cities in the group
    QTreeWidgetItem* group = itemOf(warsaw)->parent();
    REQUIRE(group != nullptr);
    CHECK(group->data(0, Qt::UserRole + 1).toString() == QStringLiteral("workshop_group"));
    CHECK(itemOf(krakow)->parent() == group);
    CHECK(itemOf(zoliborz)->parent() == itemOf(warsaw));
    CHECK(itemOf(warsaw)->isExpanded());
    CHECK(itemOf(warsaw)->indexOfChild(itemOf(mokotow)) == 0);
    CHECK(itemOf(warsaw)->indexOfChild(itemOf(zoliborz)) == 1);

    // Another city in the field: the district goes there at once, still the current item
    tree->setCurrentItem(itemOf(zoliborz));
    REQUIRE(pm.setCardFieldText(zoliborz, parent, QStringLiteral("Krakow")));
    CHECK(itemOf(zoliborz)->parent() == itemOf(krakow));
    CHECK(tree->currentItem() == itemOf(zoliborz));

    // A new name: its place among the others under its city
    const QString wola = addCard(pm, "location", QStringLiteral("Wola"));
    REQUIRE(pm.setCardFieldText(wola, parent, QStringLiteral("Warsaw")));
    window.navigator.refreshNavigator();
    CHECK(itemOf(warsaw)->indexOfChild(itemOf(wola)) == 1);
    REQUIRE(pm.renameElement(wola, QStringLiteral("Bemowo")));
    window.navigatorPanel->refreshItem(wola);
    CHECK(itemOf(warsaw)->indexOfChild(itemOf(wola)) == 0);
    CHECK(itemOf(warsaw)->indexOfChild(itemOf(mokotow)) == 1);

    // The list in the settings: all of them in the group in alphabetical order, and the tree
    // again
    core::SettingsManager::getInstance().set<std::string>(gui::NavigatorPanel::CARDS_TREE_SETTING,
                                                         "list");
    QCoreApplication::processEvents();
    group = itemOf(warsaw)->parent();
    CHECK(itemOf(zoliborz)->parent() == group);
    CHECK(itemOf(wola)->parent() == group);
    CHECK(itemOf(warsaw)->childCount() == 0);
    const QStringList all{QStringLiteral("Bemowo"), QStringLiteral("Krakow"),
                          QStringLiteral("Mokotow"), QStringLiteral("Warsaw"),
                          QStringLiteral("Zoliborz")};
    CHECK(titlesUnder(group) == all);
    core::SettingsManager::getInstance().set<std::string>(gui::NavigatorPanel::CARDS_TREE_SETTING,
                                                         "tree");
    QCoreApplication::processEvents();
    CHECK(titlesUnder(itemOf(warsaw)) ==
          QStringList{QStringLiteral("Bemowo"), QStringLiteral("Mokotow")});
    CHECK(titlesUnder(itemOf(krakow)) == QStringList{QStringLiteral("Zoliborz")});
}

TEST_CASE("Palette: swatches changed with a click, taken out with Delete, added with Insert or "
          "\"+\"",
          "[gui][cards][settings]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    gui::registerAllIcons();
    const RecentColors recentColors;
    gui::PaletteEdit palette(QStringLiteral("A color of the palette of new cards"));
    int changes = 0;
    QObject::connect(&palette, &gui::PaletteEdit::colorsChanged, [&changes]() { ++changes; });
    palette.setColors({QStringLiteral("#c0504d"), QStringLiteral("#7e57c2"),
                       QStringLiteral("#2e9e6a")});
    palette.resize(400, 80);
    palette.show();
    palette.activateWindow();
    REQUIRE(test::waitUntil([&palette] { return QApplication::activeWindow() == &palette; }));

    // A swatch of each color, then "+"
    REQUIRE(palette.swatches().size() == 3);
    CHECK(changes == 0);
    QToolButton* second = palette.swatches().at(1);
    CHECK(second->accessibleName() == QStringLiteral("Color 2: #7E57C2"));
    CHECK(palette.addButton()->isVisible());
    CHECK(palette.addButton()->x() > palette.swatches().last()->x());

    // A click changes the color in the color window
    CHECK(choosing([second]() { second->click(); }, QColor(QStringLiteral("#123456"))) == 1);
    CHECK(palette.colors() == QStringList{QStringLiteral("#c0504d"), QStringLiteral("#123456"),
                                          QStringLiteral("#2e9e6a")});
    CHECK(changes == 1);
    CHECK(palette.swatches().at(1) == second);
    CHECK(second->accessibleName() == QStringLiteral("Color 2: #123456"));

    // The arrows go from a swatch to the next one, past the last to "+"
    palette.activateWindow();
    REQUIRE(test::waitUntil([&palette] { return QApplication::activeWindow() == &palette; }));
    second->setFocus(Qt::OtherFocusReason);
    REQUIRE(test::waitUntil([second] { return second->hasFocus(); }));
    press(second, Qt::Key_Right);
    CHECK(palette.swatches().at(2)->hasFocus());
    press(palette.swatches().at(2), Qt::Key_Left);
    CHECK(second->hasFocus());

    // Delete takes the color out, the keys go to the next one
    press(second, Qt::Key_Delete);
    CHECK(palette.colors() == QStringList{QStringLiteral("#c0504d"), QStringLiteral("#2e9e6a")});
    CHECK(changes == 2);
    REQUIRE(palette.swatches().size() == 2);
    CHECK(palette.swatches().at(1)->hasFocus());

    // Insert adds a color at the end, as "+" does
    CHECK(choosing([&palette]() { press(palette.swatches().at(0), Qt::Key_Insert); },
                   QColor(QStringLiteral("#abcdef"))) == 1);
    CHECK(palette.colors() == QStringList{QStringLiteral("#c0504d"), QStringLiteral("#2e9e6a"),
                                          QStringLiteral("#abcdef")});
    CHECK(changes == 3);
    CHECK(choosing([&palette]() { palette.addButton()->click(); },
                   QColor(QStringLiteral("#000000"))) == 1);
    CHECK(palette.colors().size() == 4);
    CHECK(changes == 4);

    // The last color stays
    palette.setColors({QStringLiteral("#c0504d")});
    palette.removeColor(0);
    CHECK(palette.colors() == QStringList{QStringLiteral("#c0504d")});
}

TEST_CASE("Settings > Cards: the dots in the Navigator and the palette of new cards",
          "[gui][cards][settings]") {
    gui::registerAllIcons();
    using json = nlohmann::json;
    const ChangedSetting<json> palette(core::CardColors::PALETTE_SETTING,
                                       json::array({"#111111", "#222222"}));
    const ChangedSetting<bool> dots(core::CardColors::NAVIGATOR_DOT_SETTING, true);
    const ChangedSetting<std::string> trees(gui::NavigatorPanel::CARDS_TREE_SETTING, "tree");
    const ChangedSetting<std::string> atCursor(gui::CursorCardBox::SETTING,
                                               gui::CursorCardBox::NAMES);
    const ChangedSetting<bool> marks(gui::ParagraphMarksCoordinator::CARD_MARKS_SETTING, true);

    gui::CardsPage page;
    page.load();
    REQUIRE(page.paletteEdit() != nullptr);
    CHECK(page.paletteEdit()->colors() ==
          QStringList{QStringLiteral("#111111"), QStringLiteral("#222222")});
    QCheckBox* dot = nullptr;
    for (QCheckBox* box : page.findChildren<QCheckBox*>()) {
        if (box->text() == QStringLiteral("Color dots of the cards in the Navigator")) {
            dot = box;
        }
    }
    REQUIRE(dot != nullptr);
    CHECK(dot->isChecked());
    CHECK_FALSE(page.isChanged());

    // Restore Defaults shows the palette of the program, written by Apply
    page.restorePaletteButton()->click();
    CHECK(page.paletteEdit()->colors() == core::CardColors::defaultPalette());
    CHECK(page.isChanged());
    dot->setChecked(false);
    page.apply();
    CHECK(core::CardColors::palette() == core::CardColors::defaultPalette());
    CHECK_FALSE(core::SettingsManager::getInstance().get<bool>(
        core::CardColors::NAVIGATOR_DOT_SETTING, true));
    CHECK_FALSE(page.isChanged());

    // A color taken out
    page.paletteEdit()->removeColor(0);
    CHECK(page.isChanged());
    page.apply();
    CHECK(core::CardColors::palette() == core::CardColors::defaultPalette().mid(1));

    // The locations in the Navigator: a tree, or an alphabetical list
    QComboBox* locations = page.locationsList();
    REQUIRE(locations != nullptr);
    CHECK(locations->currentData().toString() == QStringLiteral("tree"));
    locations->setCurrentIndex(locations->findData(QStringLiteral("list")));
    CHECK(page.isChanged());
    page.apply();
    CHECK(core::SettingsManager::getInstance().get<std::string>(
              gui::NavigatorPanel::CARDS_TREE_SETTING, "tree") == "list");
    CHECK_FALSE(gui::NavigatorPanel::cardsInTrees());
    locations->setCurrentIndex(locations->findData(QStringLiteral("tree")));
    page.apply();
    CHECK(gui::NavigatorPanel::cardsInTrees());

    // The cards at the cursor of a text: the names and the paragraphs, only the paragraphs, none
    using gui::CursorCardBox;
    QComboBox* cursorList = page.atCursorList();
    REQUIRE(cursorList != nullptr);
    for (const char* value : {CursorCardBox::NAMES, CursorCardBox::PARAGRAPHS,
                              CursorCardBox::NONE}) {
        REQUIRE(cursorList->findData(QString::fromLatin1(value)) >= 0);
    }
    CHECK(cursorList->count() == 3);
    CHECK(cursorList->currentData().toString() == QLatin1String(CursorCardBox::NAMES));
    for (const char* value : {CursorCardBox::PARAGRAPHS, CursorCardBox::NONE,
                              CursorCardBox::NAMES}) {
        cursorList->setCurrentIndex(cursorList->findData(QString::fromLatin1(value)));
        CHECK(page.isChanged());
        page.apply();
        CHECK(core::SettingsManager::getInstance().get<std::string>(CursorCardBox::SETTING,
                                                                    "") == value);
        CHECK_FALSE(page.isChanged());
    }

    // The signs of the cards beside the linked paragraphs
    auto* signs = page.findChild<QCheckBox*>(QStringLiteral("cardsParagraphMarks"));
    REQUIRE(signs != nullptr);
    CHECK(signs->isChecked());
    signs->setChecked(false);
    CHECK(page.isChanged());
    page.apply();
    CHECK_FALSE(gui::ParagraphMarksCoordinator::cardMarksShown());
    signs->setChecked(true);
    page.apply();
    CHECK(gui::ParagraphMarksCoordinator::cardMarksShown());
}

}  // namespace kalahari
