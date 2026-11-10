/// @file test_style_dialogs.cpp
/// @brief New Style and Modify Style in both of their variants, Format > Font and Format >
///        Paragraph for a style, and Manage Styles

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/dialogs/font_dialog.h"
#include "kalahari/gui/dialogs/manage_styles_dialog.h"
#include "kalahari/gui/dialogs/paragraph_dialog.h"
#include "kalahari/gui/dialogs/style_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/paragraph_panel.h"
#include "kalahari/gui/style_coordinator.h"
#include "kalahari/gui/style_draft.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/gui/widgets/style_tag_delegate.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <functional>
#include <string>

using namespace kalahari;
using gui::StyleCoordinator;
using gui::StyleDraft;
using gui::dialogs::FontDialog;
using gui::dialogs::ManageStylesDialog;
using gui::dialogs::ParagraphDialog;
using gui::dialogs::StyleDialog;
using Variant = gui::dialogs::StyleDialog::Variant;

namespace {

constexpr const char* VARIANT_SETTING = "ui.styleWindow";

/// Key @p key pressed in @p widget
void pressKey(QWidget* widget, Qt::Key key, const QString& text = QString()) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(widget, &release);
}

/// @p text typed in @p widget, a key for each character
void typeIn(QWidget* widget, const QString& text) {
    for (const QChar character : text) {
        pressKey(widget, static_cast<Qt::Key>(character.toUpper().unicode()), QString(character));
    }
}

/// The way the window of a style shows it, as it was before the test
class KeptVariant {
public:
    KeptVariant()
        : m_saved(core::SettingsManager::getInstance().get<std::string>(VARIANT_SETTING)) {}
    ~KeptVariant() {
        try {
            core::SettingsManager::getInstance().set<std::string>(VARIANT_SETTING, m_saved);
        } catch (...) {
            std::fputs("The way of the window of a style could not be given back\n", stderr);
        }
    }
    KeptVariant(const KeptVariant&) = delete;
    KeptVariant& operator=(const KeptVariant&) = delete;

    /// Show the next windows this way
    static void set(const char* value) {
        core::SettingsManager::getInstance().set<std::string>(VARIANT_SETTING, value);
    }

private:
    std::string m_saved;
};

/// A style of the book's type, named in English
core::PackageParagraphStyle typeStyle(const QString& id, const QString& name,
                                      const QString& base, const QVariantMap& properties) {
    core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), name);
    style.baseStyle = base;
    style.properties = properties;
    return style;
}

/// The styles of the type: Normal and Quote in italics with indents
QList<core::PackageParagraphStyle> typeStyles() {
    return {typeStyle(QStringLiteral("normal"), QStringLiteral("Normal"), QString(),
                      {{QStringLiteral("fontSize"), 12}, {QStringLiteral("lineHeight"), 1.5}}),
            typeStyle(QStringLiteral("quote"), QStringLiteral("Quote"), QStringLiteral("normal"),
                      {{QStringLiteral("italic"), true},
                       {QStringLiteral("leftMargin"), 36},
                       {QStringLiteral("rightMargin"), 36}})};
}

editor::ParagraphStyleSet bookStyles() {
    return editor::ParagraphStyleSet::forBook(typeStyles(), {}, QStringLiteral("en"));
}

/// Run @p command, answering each window it shows with @p answer once
/// @return The number of windows the command showed
int answeringWith(const std::function<void()>& command,
                  const std::function<void(QDialog&)>& answer) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QDialog* dialog = test::shownDialog();
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        answer(*dialog);
    });
    timer.start();
    command();
    return shown;
}

/// Press the key @p keys of the window, as the writer does
void press(QWidget& window, const QKeySequence& keys) {
    for (QShortcut* shortcut : window.findChildren<QShortcut*>()) {
        if (shortcut->key() == keys) {
            emit shortcut->activated();
            return;
        }
    }
    FAIL("No key " << keys.toString().toStdString());
}

/// The items of a menu: their texts, "-" for a line
QStringList itemsOf(const QMenu& menu) {
    QStringList items;
    for (const QAction* action : menu.actions()) {
        items.append(action->isSeparator() ? QStringLiteral("-") : action->text());
    }
    return items;
}

/// The item of a menu with this text
QAction* itemOf(const QMenu& menu, const QString& text) {
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

/// The row of a list with this text
int rowOf(const QListWidget& list, const QString& text) {
    for (int row = 0; row < list.count(); ++row) {
        if (list.item(row)->text() == text) {
            return row;
        }
    }
    return -1;
}

}  // namespace

// =============================================================================
// The window of a style
// =============================================================================

TEST_CASE("Style window: on tabs at first, short after the switch, remembered",
          "[gui][styles][dialogs]") {
    KeptVariant kept;
    KeptVariant::set("tabs");
    StyleDialog dialog(StyleDraft::newStyle(bookStyles(), QStringLiteral("quote")),
                       QStringLiteral("Novel"), true);
    CHECK(dialog.windowTitle() == QStringLiteral("New Style"));
    CHECK(dialog.variant() == Variant::Tabs);
    CHECK(dialog.tabs()->count() == 4);
    CHECK(dialog.nameField() == dialog.nameField(Variant::Tabs));
    CHECK(dialog.variantButton()->text() == QStringLiteral("S&horter Window"));
    CHECK(dialog.applyBox()->isVisibleTo(&dialog));
    CHECK(dialog.applyToText());

    dialog.variantButton()->click();
    CHECK(dialog.variant() == Variant::Short);
    CHECK(dialog.nameField() == dialog.nameField(Variant::Short));
    CHECK(dialog.variantButton()->text() == QStringLiteral("Window with &Tabs"));
    CHECK(core::SettingsManager::getInstance().get<std::string>(VARIANT_SETTING) == "short");
    CHECK(StyleDialog::savedVariant() == Variant::Short);
    StyleDialog next(StyleDraft::newStyle(bookStyles(), QStringLiteral("quote")),
                     QStringLiteral("Novel"), true);
    CHECK(next.variant() == Variant::Short);

    // Only a new style goes to the paragraph at the cursor
    StyleDialog modify(StyleDraft::existing(bookStyles(), typeStyles(), QStringLiteral("quote")),
                       QStringLiteral("Novel"), true);
    CHECK(modify.windowTitle() == QStringLiteral("Modify the Style \"Quote\""));
    CHECK_FALSE(modify.applyBox()->isVisibleTo(&modify));
    CHECK_FALSE(modify.applyToText());
}

TEST_CASE("Style window: both variants change one style, bold labels mark its own",
          "[gui][styles][dialogs]") {
    KeptVariant kept;
    KeptVariant::set("tabs");
    StyleDialog dialog(StyleDraft::newStyle(bookStyles(), QStringLiteral("quote")),
                       QStringLiteral("Novel"), false);

    // On tabs: the alignment and the left indent; the short window shows them too
    const int center = dialog.alignmentBox()->findData(QStringLiteral("center"));
    REQUIRE(center >= 0);
    dialog.alignmentBox()->setCurrentIndex(center);
    emit dialog.alignmentBox()->activated(center);
    CHECK(dialog.draft().value(QStringLiteral("alignment")) == QStringLiteral("center"));
    CHECK(dialog.isMarkedOwn(QStringLiteral("alignment")));
    CHECK(dialog.alignmentButton(Qt::AlignHCenter)->isChecked());
    CHECK_FALSE(dialog.isMarkedOwn(QStringLiteral("leftMargin")));
    dialog.leftField()->setStoredValue(18.0);
    CHECK(dialog.draft().value(QStringLiteral("leftMargin")).toDouble() == 18.0);
    CHECK(dialog.isMarkedOwn(QStringLiteral("leftMargin")));
    dialog.leftField()->setStoredValue(36.0);  // Quote's: not its own
    CHECK_FALSE(dialog.draft().isOwn(QStringLiteral("leftMargin")));
    CHECK_FALSE(dialog.isMarkedOwn(QStringLiteral("leftMargin")));

    // Short: bold, which the tab Font shows with Quote's italics
    dialog.setVariant(Variant::Short);
    dialog.boldButton()->click();
    CHECK(dialog.draft().value(QStringLiteral("bold")).toBool());
    CHECK(dialog.typefaceList()->currentRow() == 3);  // Bold Italic
    CHECK(dialog.isMarkedOwn(QStringLiteral("bold")));
    emit dialog.shortLineSpacingBox()->activated(StyleDialog::Double);
    CHECK(dialog.draft().value(QStringLiteral("lineHeight")).toDouble() == 2.0);
    CHECK(dialog.lineSpacingBox()->currentIndex() == StyleDialog::Double);
    CHECK_FALSE(dialog.lineSpacingField()->isEnabled());

    // A multiple of the tab shows in the short list as one more line
    dialog.setVariant(Variant::Tabs);
    emit dialog.lineSpacingBox()->activated(StyleDialog::Multiple);
    CHECK(dialog.draft().value(QStringLiteral("lineHeight")).toDouble() == 1.15);
    CHECK(dialog.lineSpacingField()->isEnabled());
    CHECK(dialog.shortLineSpacingBox()->count() == 4);
    CHECK(dialog.shortLineSpacingBox()->currentIndex() == 3);

    // The keys of the text
    press(dialog, QKeySequence(Qt::CTRL | Qt::Key_R));
    CHECK(dialog.draft().value(QStringLiteral("alignment")) == QStringLiteral("right"));
    press(dialog, QKeySequence(Qt::CTRL | Qt::Key_I));
    CHECK_FALSE(dialog.draft().value(QStringLiteral("italic")).toBool());
    CHECK(dialog.typefaceList()->currentRow() == 2);  // Bold

    // The sentence of what it is, in both; Clear Own Settings
    CHECK(dialog.summaryLabel(Variant::Tabs)->text() == dialog.draft().summary());
    CHECK(dialog.summaryLabel(Variant::Short)->text() == dialog.draft().summary());
    CHECK(dialog.resetButton()->text() == QStringLiteral("&Clear Own Settings"));
    REQUIRE(dialog.resetButton()->isEnabled());
    dialog.resetButton()->click();
    CHECK(dialog.draft().ownProperties().isEmpty());
    CHECK_FALSE(dialog.resetButton(Variant::Short)->isEnabled());
    CHECK_FALSE(dialog.isMarkedOwn(QStringLiteral("bold")));
    CHECK(dialog.alignmentButton(Qt::AlignJustify)->isChecked());
}

TEST_CASE("Style window: a name the book has, or none, cannot be saved",
          "[gui][styles][dialogs]") {
    KeptVariant kept;
    KeptVariant::set("tabs");
    StyleDialog dialog(StyleDraft::newStyle(bookStyles(), QStringLiteral("normal")),
                       QStringLiteral("Novel"), false);
    QLineEdit* name = dialog.nameField();
    CHECK(name->text() == QStringLiteral("Style 1"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.problemLabel()->isHidden());

    name->selectAll();
    typeIn(name, QStringLiteral("quote"));
    CHECK(dialog.problemLabel()->text() ==
          QStringLiteral("The book has a style \"Quote\" already."));
    CHECK_FALSE(dialog.problemLabel()->isHidden());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(dialog.nameField(Variant::Short)->text() == QStringLiteral("quote"));

    name->selectAll();
    pressKey(name, Qt::Key_Delete);
    CHECK(dialog.problemLabel()->text() == QStringLiteral("Give the style a name."));
    typeIn(name, QStringLiteral("Dream"));
    CHECK(dialog.problemLabel()->isHidden());
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.draft().name() == QStringLiteral("Dream"));
}

TEST_CASE("Style window: a style of the type keeps its name and base", "[gui][styles][dialogs]") {
    KeptVariant kept;
    KeptVariant::set("tabs");
    StyleDialog quote(StyleDraft::existing(bookStyles(), typeStyles(), QStringLiteral("quote")),
                      QStringLiteral("Novel"), false);
    for (const Variant variant : {Variant::Tabs, Variant::Short}) {
        CHECK_FALSE(quote.nameField(variant)->isEnabled());
        CHECK_FALSE(quote.baseBox(variant)->isEnabled());
        CHECK(quote.baseBox(variant)->currentText() == QStringLiteral("Normal"));
        CHECK(quote.resetButton(variant)->text() == QStringLiteral("&Reset to the Type's Style"));
        CHECK_FALSE(quote.resetButton(variant)->isEnabled());
        // Enter goes on in it, or in another style
        CHECK(quote.nextBox(variant)->itemText(0) == QStringLiteral("(the same style)"));
        CHECK(quote.nextBox(variant)->findData(QStringLiteral("quote")) == -1);
    }
    CHECK(quote.screenNote()->isHidden());

    quote.underlineBox()->setChecked(true);
    CHECK(quote.resetButton()->isEnabled());
    CHECK(quote.summaryLabel()->text().endsWith(
        QStringLiteral(" Changed in this book: underlined.")));
    quote.resetButton()->click();
    CHECK_FALSE(quote.draft().value(QStringLiteral("underline")).toBool());
    CHECK_FALSE(quote.underlineBox()->isChecked());

    // Normal has no base; the tab Font says where the screen takes its font from
    StyleDialog normal(StyleDraft::existing(bookStyles(), typeStyles(), QStringLiteral("normal")),
                       QStringLiteral("Novel"), false);
    CHECK(normal.baseBox()->currentText() == QStringLiteral("(none – the base style)"));
    CHECK_FALSE(normal.screenNote()->isHidden());
    CHECK(normal.screenNote()->text().startsWith(
        QStringLiteral("On the screen the text in the style \"Normal\"")));
}

TEST_CASE("Style window: Font... and Paragraph... for the style", "[gui][styles][dialogs]") {
    KeptVariant kept;
    KeptVariant::set("short");
    StyleDialog dialog(StyleDraft::newStyle(bookStyles(), QStringLiteral("normal")),
                       QStringLiteral("Novel"), false);
    REQUIRE(dialog.variant() == Variant::Short);

    // Format > Font with only what a style sets
    CHECK(answeringWith([&]() { dialog.fontButton()->click(); },
                        [](QDialog& shown) {
                            auto* font = qobject_cast<FontDialog*>(&shown);
                            REQUIRE(font != nullptr);
                            CHECK(font->windowTitle() ==
                                  QStringLiteral("Font of the Style \"Style 1\""));
                            CHECK(font->tabs()->count() == 1);
                            CHECK(font->colorButton()->isHidden());
                            CHECK(font->strikethroughBox()->isHidden());
                            CHECK_FALSE(font->underlineBox()->isHidden());
                            font->underlineBox()->setChecked(true);
                            font->acceptButton()->click();
                        }) == 1);
    CHECK(dialog.draft().value(QStringLiteral("underline")).toBool());
    CHECK(dialog.draft().isOwn(QStringLiteral("underline")));

    // Format > Paragraph: a style has its line spacing in lines
    CHECK(answeringWith([&]() { dialog.paragraphButton()->click(); },
                        [](QDialog& shown) {
                            auto* paragraph = qobject_cast<ParagraphDialog*>(&shown);
                            REQUIRE(paragraph != nullptr);
                            CHECK(paragraph->windowTitle() ==
                                  QStringLiteral("Paragraph of the Style \"Style 1\""));
                            auto* rows = qobject_cast<QStandardItemModel*>(
                                paragraph->lineSpacingBox()->model());
                            REQUIRE(rows != nullptr);
                            CHECK_FALSE(rows->item(ParagraphDialog::AtLeast)->isEnabled());
                            CHECK_FALSE(rows->item(ParagraphDialog::Exactly)->isEnabled());
                            CHECK(rows->item(ParagraphDialog::Multiple)->isEnabled());
                            paragraph->rightField()->setStoredValue(12.0);
                            QComboBox* alignment = paragraph->alignmentBox();
                            alignment->setCurrentIndex(
                                alignment->findData(static_cast<int>(Qt::AlignRight)));
                            paragraph->acceptButton()->click();
                        }) == 1);
    CHECK(dialog.draft().value(QStringLiteral("rightMargin")).toDouble() == 12.0);
    CHECK(dialog.draft().value(QStringLiteral("alignment")) == QStringLiteral("right"));
    CHECK(dialog.alignmentButton(Qt::AlignRight)->isChecked());
    CHECK(dialog.draft().value(QStringLiteral("lineHeight")).toDouble() == 1.5);  // as it was
}

TEST_CASE("Style window: fits a small screen in both variants", "[gui][styles][dialogs]") {
    // 1366 x 768 at 150%: 910 x 512 points, a taskbar and the title of the window under it
    constexpr int MAX_WIDTH = 760;
    constexpr int MAX_HEIGHT = 480;
    KeptVariant kept;
    for (const char* way : {"tabs", "short"}) {
        INFO(way);
        KeptVariant::set(way);
        StyleDialog dialog(StyleDraft::newStyle(bookStyles(), QStringLiteral("quote")),
                           QStringLiteral("Novel"), true);
        const QSize size = dialog.sizeHint();
        INFO(size.width() << " x " << size.height());
        CHECK(size.width() <= MAX_WIDTH);
        CHECK(size.height() <= MAX_HEIGHT);
    }
}

// =============================================================================
// Manage Styles
// =============================================================================

TEST_CASE("Manage Styles: the styles of the type and the book's own", "[gui][styles][dialogs]") {
    test::OpenNovel book(QStringLiteral("Styles"));
    test::Window window;
    StyleCoordinator styles(window.tabs, {}, &window.statusBar, &window.window);
    StyleDraft draft = StyleDraft::newStyle(styles.styles(), QStringLiteral("quote"), {},
                                            QStringLiteral("Memory"));
    const QString memory = styles.saveDraft(draft);
    REQUIRE(memory == QStringLiteral("memory"));

    ManageStylesDialog dialog(styles, memory, &window.window);
    QListWidget* list = dialog.list();
    CHECK(dialog.selectedStyle() == memory);
    CHECK(list->item(0)->text() == QStringLiteral("Styles of the Type \"Novel\""));
    CHECK(list->item(0)->flags() == Qt::NoItemFlags);
    const int ownHeading = rowOf(*list, QStringLiteral("Own Styles of This Book"));
    REQUIRE(ownHeading > 0);
    CHECK(list->item(ownHeading + 1)->text() == QStringLiteral("Memory"));
    CHECK(list->item(ownHeading + 1)->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("own"));

    // The style selected: its kind, settings and use
    CHECK(dialog.nameLabel()->text() == QStringLiteral("Memory"));
    CHECK(dialog.kindLabel()->text() == QStringLiteral("a style of this book's own"));
    CHECK(dialog.settingsLabel()->text().contains(QStringLiteral("Based on")));
    CHECK(dialog.settingsLabel()->text().contains(QStringLiteral("Quote")));
    CHECK(dialog.usageLabel()->text() == QStringLiteral("No paragraph uses it."));
    CHECK(dialog.deleteButton()->isEnabled());
    CHECK_FALSE(dialog.resetButton()->isEnabled());
    dialog.selectStyle(QStringLiteral("normal"));
    CHECK(dialog.kindLabel()->text() == QStringLiteral("a style of the type \"Novel\""));
    CHECK_FALSE(dialog.deleteButton()->isEnabled());
    CHECK_FALSE(dialog.resetButton()->isEnabled());

    // Delete, and Ctrl+Z
    dialog.selectStyle(memory);
    dialog.deleteButton()->click();
    CHECK_FALSE(styles.styles().contains(memory));
    CHECK(rowOf(*list, QStringLiteral("Own Styles of This Book")) == -1);
    CHECK_FALSE(dialog.selectedStyle().isEmpty());
    CHECK(dialog.undoLabel()->text() ==
          QStringLiteral("Ctrl+Z undoes deleting the style \"Memory\"."));
    press(dialog, QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Z)));
    CHECK(styles.styles().contains(memory));
    CHECK(rowOf(*list, QStringLiteral("Memory")) > 0);

    // A style of the type changed in the book: tagged, and reset
    StyleDraft quote = StyleDraft::existing(styles.styles(), gui::bookTypeStyles(),
                                            QStringLiteral("quote"));
    quote.setValue(QStringLiteral("italic"), false);
    REQUIRE(styles.saveDraft(quote) == QStringLiteral("quote"));
    dialog.selectStyle(QStringLiteral("quote"));
    const int quoteRow = list->currentRow();
    CHECK(list->item(quoteRow)->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("changed"));
    CHECK(dialog.kindLabel()->text() ==
          QStringLiteral("a style of the type \"Novel\", changed in this book"));
    REQUIRE(dialog.resetButton()->isEnabled());
    dialog.resetButton()->click();
    CHECK(styles.styles().find(QStringLiteral("quote"))->origin == editor::StyleOrigin::Type);
    CHECK(dialog.selectedStyle() == QStringLiteral("quote"));
    CHECK(list->currentItem()->data(gui::StyleTagDelegate::TAG_ROLE).toString().isEmpty());

    // Enter in the list opens the style, and does not close the window
    CHECK(answeringWith([&]() { pressKey(list, Qt::Key_Return); },
                        [](QDialog& shown) {
                            CHECK(qobject_cast<StyleDialog*>(&shown) != nullptr);
                            shown.reject();
                        }) == 1);
    CHECK(dialog.result() == 0);
}

// =============================================================================
// The palette of the Paragraph and Styles panel
// =============================================================================

TEST_CASE("Paragraph and Styles: the palette tags the styles and changes the one chosen",
          "[gui][styles][panels]") {
    gui::registerAllIcons();
    KeptVariant kept;
    KeptVariant::set("tabs");
    test::OpenNovel book(QStringLiteral("Styles"));
    test::Window window;
    StyleCoordinator styles(window.tabs, {}, &window.statusBar, &window.window);
    StyleDraft draft = StyleDraft::newStyle(styles.styles(), QStringLiteral("quote"), {},
                                            QStringLiteral("Memory"));
    const QString memory = styles.saveDraft(draft);
    REQUIRE(memory == QStringLiteral("memory"));
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    text->replaceWithKml(QStringLiteral("<kml><p>One.</p><p style=\"memory\">Two.</p></kml>"));
    text->setCursorPosition({1, 0});

    // Without the styles of the book: no buttons, and the menu only gives the style
    gui::ParagraphPanel panel;
    panel.setEditor(text);
    auto* bar = panel.findChild<QToolBar*>(QStringLiteral("paragraphStyleBar"));
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("paragraphStyleList"));
    REQUIRE(bar != nullptr);
    REQUIRE(list != nullptr);
    CHECK(bar->isHidden());
    QMenu plain;
    panel.fillStyleMenu(plain);
    CHECK(itemsOf(plain) == QStringList{QStringLiteral("&Apply the Style \"Memory\"")});
    CHECK(plain.actions().first()->font().bold());

    // The writer's own style is tagged; it is the style of the paragraph at the cursor
    panel.setStyleCoordinator(&styles);
    CHECK_FALSE(bar->isHidden());
    CHECK(bar->actions().size() == 5);  // New, Modify, Delete | Manage
    const int memoryRow = rowOf(*list, QStringLiteral("Memory"));
    REQUIRE(memoryRow >= 0);
    CHECK(list->item(memoryRow)->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("own"));
    CHECK(list->item(rowOf(*list, QStringLiteral("Quote")))
              ->data(gui::StyleTagDelegate::TAG_ROLE)
              .toString()
              .isEmpty());
    CHECK(panel.paletteStyle() == memory);
    CHECK(panel.deleteStyleAction()->isEnabled());
    CHECK(panel.deleteStyleAction()->toolTip() == QStringLiteral("Delete the style \"Memory\""));
    CHECK(panel.newStyleAction()->toolTip() ==
          QStringLiteral("New style based on \"Memory\"..."));
    CHECK(panel.manageStylesAction()->isEnabled());

    // Quote, chosen with the arrows: a style of the type is not deleted, nor reset while the
    // book has it as the type does
    list->setCurrentRow(rowOf(*list, QStringLiteral("Quote")));
    CHECK(panel.paletteStyle() == QStringLiteral("quote"));
    CHECK(panel.modifyStyleAction()->isEnabled());
    CHECK(panel.modifyStyleAction()->toolTip() ==
          QStringLiteral("Modify the style \"Quote\"..."));
    CHECK_FALSE(panel.deleteStyleAction()->isEnabled());
    CHECK(panel.deleteStyleAction()->toolTip() ==
          QStringLiteral("\"Quote\" is a style of the type \"Novel\": it can be changed or "
                         "reset, but not deleted"));
    QMenu menu;
    panel.fillStyleMenu(menu);
    CHECK(itemsOf(menu) == QStringList{QStringLiteral("&Apply the Style \"Quote\""),
                                       QStringLiteral("-"),
                                       QStringLiteral("&New Style Based on \"Quote\"..."),
                                       QStringLiteral("&Modify the Style \"Quote\"..."),
                                       QStringLiteral("&Reset to the Type's Style"),
                                       QStringLiteral("&Delete the Style \"Quote\""),
                                       QStringLiteral("-"), QStringLiteral("Manage &Styles...")});
    CHECK_FALSE(itemOf(menu, QStringLiteral("&Reset to the Type's Style"))->isEnabled());
    CHECK_FALSE(itemOf(menu, QStringLiteral("&Delete the Style \"Quote\""))->isEnabled());

    // Modify opens the window of Quote; changed, it is tagged so, and the menu resets it
    CHECK(answeringWith([&]() { panel.modifyStyleAction()->trigger(); },
                        [](QDialog& shown) {
                            auto* style = qobject_cast<StyleDialog*>(&shown);
                            REQUIRE(style != nullptr);
                            CHECK(style->nameField()->text() == QStringLiteral("Quote"));
                            style->underlineBox()->setChecked(true);
                            style->acceptButton()->click();
                        }) == 1);
    CHECK(styles.styles().find(QStringLiteral("quote"))->origin == editor::StyleOrigin::Changed);
    panel.refresh();
    const int quoteRow = rowOf(*list, QStringLiteral("Quote"));
    REQUIRE(quoteRow >= 0);
    CHECK(list->item(quoteRow)->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("changed"));
    list->setCurrentRow(quoteRow);
    QMenu changed;
    panel.fillStyleMenu(changed);
    QAction* reset = itemOf(changed, QStringLiteral("&Reset to the Type's Style"));
    REQUIRE(reset != nullptr);
    REQUIRE(reset->isEnabled());
    reset->trigger();
    CHECK(styles.styles().find(QStringLiteral("quote"))->origin == editor::StyleOrigin::Type);

    // Apply in the menu gives the style chosen to the paragraph at the cursor
    panel.refresh();
    list->setCurrentRow(rowOf(*list, QStringLiteral("Quote")));
    QMenu give;
    panel.fillStyleMenu(give);
    give.actions().first()->trigger();
    CHECK(text->currentParagraphStyle() == QStringLiteral("quote"));

    // Delete in the menu of Memory, which no paragraph uses now: it goes without a question
    list->setCurrentRow(rowOf(*list, QStringLiteral("Memory")));
    QMenu own;
    panel.fillStyleMenu(own);
    CHECK(itemOf(own, QStringLiteral("&Reset to the Type's Style")) == nullptr);
    QAction* remove = itemOf(own, QStringLiteral("&Delete the Style \"Memory\""));
    REQUIRE(remove != nullptr);
    REQUIRE(remove->isEnabled());
    CHECK(answeringWith([&]() { remove->trigger(); }, [](QDialog& shown) { shown.reject(); }) ==
          0);
    CHECK_FALSE(styles.styles().contains(memory));

    // Without a text the buttons of a style say why they are greyed out; Manage Styles...
    // needs only the book
    panel.setEditor(nullptr);
    CHECK(panel.paletteStyle().isEmpty());
    CHECK_FALSE(panel.newStyleAction()->isEnabled());
    CHECK(panel.newStyleAction()->toolTip() ==
          QStringLiteral("No text with a cursor is in front"));
    CHECK(panel.manageStylesAction()->isEnabled());
    QMenu none;
    panel.fillStyleMenu(none);
    CHECK(none.isEmpty());
}

TEST_CASE("Paragraph and Styles: a style the book does not have is made for its paragraphs",
          "[gui][styles][panels]") {
    gui::registerAllIcons();
    KeptVariant kept;
    KeptVariant::set("tabs");
    test::OpenNovel book(QStringLiteral("Styles"));
    test::Window window;
    StyleCoordinator styles(window.tabs, {}, &window.statusBar, &window.window);
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    // As pasted from another book: the paragraph keeps the id of its style
    text->replaceWithKml(
        QStringLiteral("<kml><p>One.</p><p style=\"inner_voice\">Two.</p></kml>"));
    text->setCursorPosition({1, 0});

    gui::ParagraphPanel panel;
    panel.setEditor(text);
    panel.setStyleCoordinator(&styles);
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("paragraphStyleList"));
    REQUIRE(list != nullptr);

    // Last in the list, by its id, tagged; it is the style at the cursor
    QListWidgetItem* last = list->item(list->count() - 1);
    CHECK(last->text() == QStringLiteral("inner_voice"));
    CHECK(last->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("not in this book"));
    CHECK(last->data(gui::StyleTagDelegate::MISSING_ROLE).toBool());
    CHECK(list->currentItem() == last);
    CHECK(panel.newStyleAction()->toolTip() ==
          QStringLiteral("Create the style \"inner_voice\" for its paragraphs..."));
    CHECK_FALSE(panel.modifyStyleAction()->isEnabled());
    CHECK_FALSE(panel.deleteStyleAction()->isEnabled());
    QMenu menu;
    panel.fillStyleMenu(menu);
    CHECK(itemsOf(menu) == QStringList{QStringLiteral("&Apply the Style \"inner_voice\""),
                                       QStringLiteral("-"),
                                       QStringLiteral("&Create the Style \"inner_voice\"..."),
                                       QStringLiteral("-"), QStringLiteral("Manage &Styles...")});

    // Made: named after the id at first, with its id, so the paragraph shows it at once
    CHECK(answeringWith(
              [&]() {
                  itemOf(menu, QStringLiteral("&Create the Style \"inner_voice\"..."))->trigger();
              },
              [](QDialog& shown) {
                  auto* style = qobject_cast<StyleDialog*>(&shown);
                  REQUIRE(style != nullptr);
                  CHECK(style->nameField()->text() == QStringLiteral("Inner voice"));
                  CHECK_FALSE(style->applyBox()->isVisible());
                  style->acceptButton()->click();
              }) == 1);
    const editor::ParagraphStyle* made = styles.styles().find(QStringLiteral("inner_voice"));
    REQUIRE(made != nullptr);
    CHECK(made->name == QStringLiteral("Inner voice"));
    CHECK(made->origin == editor::StyleOrigin::Own);
    CHECK(made->baseStyle == QStringLiteral("normal"));
    CHECK(text->paragraphStyles().contains(QStringLiteral("inner_voice")));
    text->setCursorPosition({1, 0});
    CHECK(text->currentParagraphStyle() == QStringLiteral("inner_voice"));

    // The palette has it as the book's own now
    panel.refresh();
    CHECK(rowOf(*list, QStringLiteral("inner_voice")) == -1);
    const int row = rowOf(*list, QStringLiteral("Inner voice"));
    REQUIRE(row >= 0);
    CHECK(list->item(row)->data(gui::StyleTagDelegate::TAG_ROLE).toString() ==
          QStringLiteral("own"));
}
