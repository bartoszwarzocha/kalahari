// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_table_dialogs.cpp
/// @brief The windows of the Table menu: Split Cells..., Sort..., Text to Table..., Table to
///        Text... and Table Properties..., and the editor panel that shows them

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/table_dialogs.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include "document_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <map>
#include "test_support/window_parts.h"

using namespace kalahari;
using namespace kalahari::editor;
using Catch::Approx;
using kalahari::gui::LengthSpinBox;
using kalahari::gui::dialogs::KalahariDialog;
using kalahari::gui::dialogs::SortDialog;
using kalahari::gui::dialogs::SplitCellsDialog;
using kalahari::gui::dialogs::TablePropertiesDialog;
using kalahari::gui::dialogs::TableToTextDialog;
using kalahari::gui::dialogs::TextToTableDialog;

namespace {

/// The field a label names
template <typename Field>
Field* fieldOf(const QWidget& parent, const QString& label)
{
    for (QLabel* each : parent.findChildren<QLabel*>()) {
        if (each->text() == label) {
            return qobject_cast<Field*>(each->buddy());
        }
    }
    return nullptr;
}

/// The button with the text
template <typename Button>
Button* buttonOf(const QWidget& parent, const QString& text)
{
    for (Button* each : parent.findChildren<Button*>()) {
        if (each->text() == text) {
            return each;
        }
    }
    return nullptr;
}

/// The widget for a screen reader by its name
QWidget* namedWidget(const QWidget& parent, const QString& name)
{
    for (QWidget* each : parent.findChildren<QWidget*>()) {
        if (each->accessibleName() == name) {
            return each;
        }
    }
    return nullptr;
}

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text)
{
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts of the widgets under @p root with an access key that is not a Latin letter or
/// digit (Alt does not reach "ś"), "śr&edników" is right, "&średników" is not
QString foreignKeys(const QWidget& root)
{
    QStringList foreign;
    const auto check = [&foreign](const QString& text) {
        const QChar key = accessKey(text);
        if (!key.isNull() && (key.unicode() >= 0x80 || !key.isLetterOrNumber())) {
            foreign << text;
        }
    };
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        if (const auto* label = qobject_cast<const QLabel*>(widget)) {
            check(label->text());
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            check(button->text());
        } else if (const auto* tabs = qobject_cast<const QTabWidget*>(widget)) {
            for (int tab = 0; tab < tabs->count(); ++tab) {
                check(tabs->tabText(tab));
            }
        }
    }
    return foreign.join(QStringLiteral(" / "));
}

/// The texts of the widgets under @p root (not under @p skipped) with the same access key,
/// "&Top / &Tabs"; empty when each key is the only one
QString sameKeys(const QWidget& root, const QWidget* skipped = nullptr,
                 const QStringList& more = {})
{
    std::map<QChar, QStringList> texts;
    const auto add = [&texts](const QString& text) {
        const QChar key = accessKey(text);
        if (!key.isNull()) {
            texts[key] << text;
        }
    };
    for (const QString& text : more) {
        add(text);
    }
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        bool inSkipped = false;
        for (const QWidget* up = widget; up != nullptr && up != &root; up = up->parentWidget()) {
            inSkipped = inSkipped || up == skipped;
        }
        if (inSkipped) {
            continue;
        }
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            add(label->text());
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            add(button->text());
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            add(group->title());
        }
    }
    QStringList same;
    for (const auto& [key, each] : texts) {
        if (each.size() > 1) {
            same << each.join(QStringLiteral(" / "));
        }
    }
    return same.join(QStringLiteral("; "));
}

/// 1366x768 at 150%: 911 pixels wide, as test_dock_layout.cpp counts, and 460 high without
/// the taskbar and the title bar (a Mac's larger controls need a few pixels more)
void checkFitsSmallScreen(const QWidget& dialog)
{
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

/// The windows in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish()
    {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// The data of a table of 3 columns (100, 100 and 200 points) and 3 rows, the first a header
TablePropertiesData gridData()
{
    TablePropertiesData data;
    data.textWidth = 400.0;
    data.columnWidths = {100.0, 100.0, 200.0};
    data.rows.resize(3);
    data.headerRows = 1;
    data.row = 1;
    data.column = 0;
    return data;
}

/// The data of the window as it was given, the widths of the columns compared apart
void checkSameData(const TablePropertiesData& got, const TablePropertiesData& expected)
{
    REQUIRE(got.columnWidths.size() == expected.columnWidths.size());
    for (size_t column = 0; column < got.columnWidths.size(); ++column) {
        CHECK(got.columnWidths[column] == Approx(expected.columnWidths[column]));
    }
    TablePropertiesData same = got;
    same.columnWidths = expected.columnWidths;
    CHECK(same == expected);
}

/// The editor of a panel with a text
BookEditor& editorOf(gui::EditorPanel& panel, const QString& kml)
{
    BookEditor* editor = panel.getBookEditor();
    editor->fromKml(kml);
    return *editor;
}

/// Answer the window shown next: @p answer gets it, as the type expected
template <typename Dialog>
void answerNext(const std::function<void(Dialog&)>& answer)
{
    QTimer::singleShot(0, [answer]() {
        auto* dialog = qobject_cast<Dialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        answer(*dialog);
    });
}

}  // namespace

// =============================================================================
// Split Cells...
// =============================================================================

TEST_CASE("Split Cells window: the columns, the rows and the paragraphs", "[gui][tables][dialog]") {
    SECTION("a cell of one paragraph") {
        SplitCellsDialog dialog(false, [](bool) { return 1; });
        dialog.show();
        QApplication::processEvents();
        CHECK(dialog.windowTitle() == QStringLiteral("Split Cells"));
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Split"));
        CHECK(dialog.columns() == 2);
        CHECK(dialog.rows() == 1);
        auto* columns = fieldOf<QSpinBox>(dialog, QStringLiteral("&Columns"));
        auto* rows = fieldOf<QSpinBox>(dialog, QStringLiteral("&Rows"));
        REQUIRE(columns != nullptr);
        REQUIRE(rows != nullptr);
        CHECK(dialog.focusWidget() == columns);
        CHECK(columns->maximum() == MAX_SPLIT_CELLS);

        // Nothing to merge, one paragraph for one cell
        auto* merge = buttonOf<QCheckBox>(dialog, QStringLiteral("&Merge the selected cells first"));
        auto* distribute =
            buttonOf<QCheckBox>(dialog, QStringLiteral("&Distribute the cell's paragraphs to the new cells"));
        REQUIRE(merge != nullptr);
        REQUIRE(distribute != nullptr);
        CHECK_FALSE(merge->isEnabled());
        CHECK_FALSE(dialog.mergeFirst());
        CHECK_FALSE(distribute->isEnabled());
        CHECK_FALSE(dialog.distribute());

        // 1 x 1 does not split
        const QWidget* preview = namedWidget(dialog, QStringLiteral("Preview of the table split"));
        REQUIRE(preview != nullptr);
        CHECK(preview->accessibleDescription() ==
              QStringLiteral("The cell split into 2 columns and 1 rows"));
        columns->setValue(1);
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        rows->setValue(3);
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(dialog.rows() == 3);
        CHECK(sameKeys(dialog).isEmpty());
        checkFitsSmallScreen(dialog);
        CHECK_FALSE(dialog.grab().isNull());
    }

    SECTION("cells selected: merged first, their paragraphs one to each new cell") {
        SplitCellsDialog dialog(true, [](bool merged) { return merged ? 4 : 1; });
        auto* merge = buttonOf<QCheckBox>(dialog, QStringLiteral("&Merge the selected cells first"));
        auto* distribute =
            buttonOf<QCheckBox>(dialog, QStringLiteral("&Distribute the cell's paragraphs to the new cells"));
        REQUIRE(merge != nullptr);
        REQUIRE(distribute != nullptr);
        CHECK(dialog.mergeFirst());
        CHECK(dialog.distribute());

        // Without merging, the first cell's one paragraph
        merge->setChecked(false);
        CHECK_FALSE(dialog.mergeFirst());
        CHECK_FALSE(distribute->isEnabled());
        CHECK_FALSE(dialog.distribute());
        merge->setChecked(true);
        CHECK(dialog.distribute());

        // 1 x 1 merges only
        fieldOf<QSpinBox>(dialog, QStringLiteral("&Columns"))->setValue(1);
        CHECK(dialog.acceptButton()->isEnabled());
    }
}

// =============================================================================
// Sort...
// =============================================================================

TEST_CASE("Sort window: the keys of the rows of a table, or of paragraphs", "[gui][tables][dialog]") {
    SECTION("the rows: by the columns of the header row") {
        SortDialog dialog(true, {QStringLiteral("Name"), QString(), QStringLiteral("Date")}, true,
                          QStringLiteral("Polish"));
        dialog.show();
        QApplication::processEvents();
        CHECK(dialog.windowTitle() == QStringLiteral("Sort"));
        auto* first = fieldOf<QComboBox>(dialog, QStringLiteral("&Sort by"));
        auto* second = fieldOf<QComboBox>(dialog, QStringLiteral("&Then by"));
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        CHECK(dialog.focusWidget() == first);
        CHECK(first->count() == 3);
        CHECK(first->itemText(1) == QStringLiteral("Column 2"));
        CHECK(second->count() == 4);
        CHECK(second->itemText(0) == QStringLiteral("(none)"));
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("The order of the alphabet: Polish, the language of the book. "
                           "The header row stays at the top.")));

        // One key at first; the second only with a column
        SortOptions options = dialog.options();
        REQUIRE(options.keys.size() == 1);
        CHECK(options.keys[0].column == 0);
        CHECK(options.keys[0].type == SortType::Text);
        CHECK_FALSE(options.keys[0].descending);
        CHECK_FALSE(options.caseSensitive);
        auto* secondDescending = buttonOf<QRadioButton>(dialog, QStringLiteral("Descending"));
        REQUIRE(secondDescending != nullptr);
        CHECK_FALSE(secondDescending->isEnabled());

        first->setCurrentIndex(2);
        auto* firstType = qobject_cast<QComboBox*>(namedWidget(dialog, QStringLiteral("Type")));
        REQUIRE(firstType != nullptr);
        firstType->setCurrentIndex(2);
        buttonOf<QRadioButton>(dialog, QStringLiteral("&Descending"))->setChecked(true);
        second->setCurrentIndex(1);
        CHECK(secondDescending->isEnabled());
        buttonOf<QCheckBox>(dialog, QStringLiteral("&Match case"))->setChecked(true);
        options = dialog.options();
        REQUIRE(options.keys.size() == 2);
        CHECK(options.keys[0].column == 2);
        CHECK(options.keys[0].type == SortType::Dates);
        CHECK(options.keys[0].descending);
        CHECK(options.keys[1].column == 0);
        CHECK_FALSE(options.keys[1].descending);
        CHECK(options.caseSensitive);
        CHECK(sameKeys(dialog).isEmpty());
        checkFitsSmallScreen(dialog);
    }

    SECTION("paragraphs: their text, with no columns") {
        SortDialog dialog(false, {}, false, QStringLiteral("English"));
        auto* first = fieldOf<QComboBox>(dialog, QStringLiteral("&Sort by"));
        REQUIRE(first != nullptr);
        CHECK(first->currentText() == QStringLiteral("Paragraphs"));
        CHECK_FALSE(first->isEnabled());
        CHECK_FALSE(fieldOf<QComboBox>(dialog, QStringLiteral("&Then by"))->isEnabled());
        CHECK(test::textsOf(dialog).contains(QStringLiteral("List items are sorted with their paragraphs.")));
        CHECK(dialog.options().keys.size() == 1);
    }
}

// =============================================================================
// Text to Table... and Table to Text...
// =============================================================================

TEST_CASE("Text to Table window: the character most paragraphs have, and the size",
          "[gui][tables][dialog]") {
    TextToTableDialog dialog({QStringLiteral("Name; Age"), QStringLiteral("Adam; 2"),
                              QStringLiteral("Ewa; 30; Kraków")});
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Text to Table"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Convert to Table"));
    CHECK(dialog.separator() == QLatin1Char(';'));
    CHECK(dialog.headerRow());

    // The columns of the longest row, the spaces next to the semicolons gone
    auto* columns = fieldOf<QSpinBox>(dialog, QStringLiteral("&Columns"));
    REQUIRE(columns != nullptr);
    CHECK(dialog.columns() == 3);
    CHECK_FALSE(columns->isEnabled());
    CHECK(dialog.cells() == std::vector<QStringList>{
                                {QStringLiteral("Name"), QStringLiteral("Age"), QString()},
                                {QStringLiteral("Adam"), QStringLiteral("2"), QString()},
                                {QStringLiteral("Ewa"), QStringLiteral("30"), QStringLiteral("Kraków")}});
    CHECK(test::textsOf(dialog).contains(QStringLiteral("The table: 3 × 3 (columns × rows)")));

    // Each paragraph a cell: the columns given
    buttonOf<QRadioButton>(dialog, QStringLiteral("&Paragraphs"))->setChecked(true);
    CHECK(dialog.separator().isNull());
    CHECK(columns->isEnabled());
    CHECK(dialog.columns() == 2);
    CHECK(dialog.cells().size() == 2);
    columns->setValue(3);
    CHECK(dialog.cells().size() == 1);
    CHECK(test::textsOf(dialog).contains(QStringLiteral("The table: 3 × 1 (columns × rows)")));

    // Another character: none given, nothing to convert
    buttonOf<QRadioButton>(dialog, QStringLiteral("&Other character:"))->setChecked(true);
    auto* other = qobject_cast<QLineEdit*>(namedWidget(dialog, QStringLiteral("Other character")));
    REQUIRE(other != nullptr);
    CHECK(other->isEnabled());
    CHECK(dialog.separator() == QLatin1Char('|'));
    other->clear();
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    other->setText(QStringLiteral(","));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.columns() == 1);

    buttonOf<QCheckBox>(dialog, QStringLiteral("&Header row"))->setChecked(false);
    CHECK_FALSE(dialog.headerRow());
    CHECK(sameKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
    CHECK_FALSE(dialog.grab().isNull());
}

TEST_CASE("Table to Text window: the cells separated by the character, or each a paragraph",
          "[gui][tables][dialog]") {
    const QString twoParagraphs = QStringLiteral("Narrator") + QChar(QChar::LineSeparator) +
                                  QStringLiteral("Second");
    TableToTextDialog dialog({{QStringLiteral("Anna"), twoParagraphs},
                              {QStringLiteral("Both; all"), QString()}});
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Table to Text"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Convert to Text"));
    CHECK(dialog.separator() == QLatin1Char('\t'));
    CHECK(dialog.paragraphs() == QStringList{QStringLiteral("Anna\tNarrator Second"),
                                             QStringLiteral("Both; all\t")});
    auto* preview = qobject_cast<QPlainTextEdit*>(namedWidget(dialog, QStringLiteral("Preview of the text")));
    REQUIRE(preview != nullptr);
    CHECK(preview->toPlainText().startsWith(QStringLiteral("Anna→\tNarrator Second")));
    CHECK(test::textsOf(dialog).contains(QStringLiteral("The arrow in the preview stands for a tab.")));

    // A semicolon is also in a cell: the note says so
    buttonOf<QRadioButton>(dialog, QStringLiteral("S&emicolons"))->setChecked(true);
    CHECK(dialog.paragraphs() == QStringList{QStringLiteral("Anna; Narrator Second"),
                                             QStringLiteral("Both; all; ")});
    CHECK(test::textsOf(dialog).contains(QStringLiteral("(“Both; all”)")));

    buttonOf<QRadioButton>(dialog, QStringLiteral("&Paragraphs"))->setChecked(true);
    CHECK(dialog.separator().isNull());
    CHECK(dialog.paragraphs() == QStringList{QStringLiteral("Anna"), QStringLiteral("Narrator"),
                                             QStringLiteral("Second"), QStringLiteral("Both; all"),
                                             QString()});
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Each cell becomes a paragraph: 5 paragraphs in all.")));
    CHECK(sameKeys(dialog).isEmpty());
    checkFitsSmallScreen(dialog);
}

// =============================================================================
// Table Properties...
// =============================================================================

TEST_CASE("Table Properties window: the tabs give the data they were given",
          "[gui][tables][dialog]") {
    KalahariDialog::setSettingsOpener([](QWidget*, const QString&) {});
    TablePropertiesDialog dialog(gridData(), QColor(QStringLiteral("#808080")));
    KalahariDialog::setSettingsOpener(nullptr);
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Table Properties"));
    REQUIRE(dialog.tabs()->count() == 5);
    QStringList tabs;
    for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
        tabs << dialog.tabs()->tabText(tab);
    }
    CHECK(tabs == QStringList{QStringLiteral("T&able"), QStringLiteral("Ro&w"),
                              QStringLiteral("Colu&mn"), QStringLiteral("C&ell"),
                              QStringLiteral("&Borders")});
    // The gear: the colors of the lines and the header
    bool gear = false;
    for (const QToolButton* button : dialog.findChildren<QToolButton*>()) {
        gear = gear || button->toolTip() == QStringLiteral("Table Color Settings");
    }
    CHECK(gear);

    checkSameData(dialog.data(), gridData());
    // The row and the column shown are the cursor's
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Row 2 of 3")));
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Column 1 of 3")));

    // Each tab with keys of its own, with the names of the tabs
    for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
        INFO("tab " << tab);
        CHECK(sameKeys(*dialog.tabs()->widget(tab), nullptr, tabs).isEmpty());
    }
    CHECK(sameKeys(dialog, dialog.tabs(), tabs).isEmpty());
    for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
        dialog.tabs()->setCurrentIndex(tab);
        CHECK_FALSE(dialog.grab().isNull());
    }
    checkFitsSmallScreen(dialog);
}

TEST_CASE("Table Properties window: the Table tab", "[gui][tables][dialog]") {
    TablePropertiesDialog dialog(gridData(), QColor());
    QWidget& tab = *dialog.tabs()->widget(0);
    auto* width = qobject_cast<LengthSpinBox*>(namedWidget(tab, QStringLiteral("Width of the table")));
    auto* indent = fieldOf<LengthSpinBox>(tab, QStringLiteral("&Indent from left"));
    REQUIRE(width != nullptr);
    REQUIRE(indent != nullptr);
    CHECK_FALSE(width->isEnabled());
    CHECK(width->storedValue() == Approx(400.0));
    CHECK(indent->isEnabled());

    buttonOf<QRadioButton>(tab, QStringLiteral("&Fixed"))->setChecked(true);
    CHECK(width->isEnabled());
    width->setStoredValue(300.0);
    indent->setStoredValue(20.0);
    TablePropertiesData data = dialog.data();
    REQUIRE(data.table.width.has_value());
    CHECK(*data.table.width == Approx(300.0));
    CHECK(data.table.indent == Approx(20.0));
    CHECK(data.table.alignment == TableAlignment::Left);

    // The indent only of a table at the left
    buttonOf<QRadioButton>(tab, QStringLiteral("&Center"))->setChecked(true);
    CHECK_FALSE(indent->isEnabled());
    data = dialog.data();
    CHECK(data.table.alignment == TableAlignment::Center);
    CHECK(data.table.indent == 0.0);

    buttonOf<QRadioButton>(tab, QStringLiteral("&Text width"))->setChecked(true);
    CHECK_FALSE(dialog.data().table.width.has_value());
}

TEST_CASE("Table Properties window: the Row tab goes through the rows", "[gui][tables][dialog]") {
    TablePropertiesDialog dialog(gridData(), QColor());
    QWidget& tab = *dialog.tabs()->widget(1);
    auto* header = buttonOf<QCheckBox>(tab, QStringLiteral("Repeat as &header row at the top of each page"));
    auto* split = buttonOf<QCheckBox>(tab, QStringLiteral("Allow the row to &split across pages"));
    auto* previous = buttonOf<QPushButton>(tab, QStringLiteral("&Previous"));
    auto* next = buttonOf<QPushButton>(tab, QStringLiteral("&Next"));
    auto* atLeast = buttonOf<QRadioButton>(tab, QStringLiteral("At &least"));
    auto* height = qobject_cast<LengthSpinBox*>(namedWidget(tab, QStringLiteral("Height of the row")));
    REQUIRE(header != nullptr);
    REQUIRE(split != nullptr);
    REQUIRE(previous != nullptr);
    REQUIRE(next != nullptr);
    REQUIRE(atLeast != nullptr);
    REQUIRE(height != nullptr);

    // Row 2, under the header: it can join it
    CHECK_FALSE(header->isChecked());
    CHECK(header->isEnabled());
    CHECK_FALSE(height->isEnabled());
    atLeast->setChecked(true);
    CHECK(height->isEnabled());
    height->setStoredValue(30.0);
    split->setChecked(true);
    TablePropertiesData data = dialog.data();
    CHECK(data.rows[1].minHeight == Approx(30.0));
    CHECK(data.rows[1].allowSplit);
    CHECK(data.rows[0] == TableRowProperties{});

    // Row 3: the rows above it are not all header rows
    next->click();
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Row 3 of 3")));
    CHECK_FALSE(next->isEnabled());
    CHECK_FALSE(header->isEnabled());
    CHECK_FALSE(split->isChecked());
    CHECK_FALSE(atLeast->isChecked());

    // Row 2 joins the header, and then row 3 can
    previous->click();
    CHECK(atLeast->isChecked());
    CHECK(height->storedValue() == Approx(30.0));
    header->setChecked(true);
    CHECK(dialog.data().headerRows == 2);
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Row 2 of 3 (header)")));
    next->click();
    CHECK(header->isEnabled());

    // Row 1 leaves the header with those after it
    dialog.showRow(0);
    CHECK(header->isChecked());
    CHECK_FALSE(previous->isEnabled());
    header->setChecked(false);
    CHECK(dialog.data().headerRows == 0);
}

TEST_CASE("Table Properties window: the Column tab, its neighbour giving the width",
          "[gui][tables][dialog]") {
    TablePropertiesDialog dialog(gridData(), QColor());
    QWidget& tab = *dialog.tabs()->widget(2);
    auto* width = fieldOf<LengthSpinBox>(tab, QStringLiteral("Column w&idth"));
    REQUIRE(width != nullptr);
    CHECK(width->storedValue() == Approx(100.0));

    // The next column gives the width
    width->setStoredValue(150.0);
    std::vector<double> widths = dialog.data().columnWidths;
    CHECK(widths[0] == Approx(150.0));
    CHECK(widths[1] == Approx(50.0));
    CHECK(widths[2] == Approx(200.0));

    // The last column takes it from the one before
    dialog.showColumn(2);
    CHECK(width->storedValue() == Approx(200.0));
    width->setStoredValue(230.0);
    widths = dialog.data().columnWidths;
    CHECK(widths[1] == Approx(20.0));
    CHECK(widths[2] == Approx(230.0));

    // Not more than the neighbour has: it keeps the narrowest width
    width->setStoredValue(400.0);
    widths = dialog.data().columnWidths;
    CHECK(widths[1] == Approx(MIN_COLUMN_WIDTH));
    CHECK(width->storedValue() == Approx(250.0 - MIN_COLUMN_WIDTH));

    buttonOf<QPushButton>(tab, QStringLiteral("&Distribute Columns Evenly"))->click();
    widths = dialog.data().columnWidths;
    for (const double each : widths) {
        CHECK(each == Approx(400.0 / 3.0));
    }

    SECTION("one column: as wide as the table") {
        TablePropertiesData data = gridData();
        data.columnWidths = {400.0};
        TablePropertiesDialog one(data, QColor());
        CHECK_FALSE(fieldOf<LengthSpinBox>(*one.tabs()->widget(2), QStringLiteral("Column w&idth"))->isEnabled());
    }
}

TEST_CASE("Table Properties window: the Cell tab", "[gui][tables][dialog]") {
    TablePropertiesDialog dialog(gridData(), QColor());
    QWidget& tab = *dialog.tabs()->widget(3);
    CHECK_FALSE(dialog.data().cellChanged);
    auto* color = fieldOf<QPushButton>(tab, QStringLiteral("Back&ground color"));
    REQUIRE(color != nullptr);
    CHECK(color->text() == QStringLiteral("No Color"));
    buttonOf<QRadioButton>(tab, QStringLiteral("B&ottom"))->setChecked(true);
    TablePropertiesData data = dialog.data();
    CHECK(data.cellChanged);
    CHECK(data.cell.alignment == CellAlignment::Bottom);
}

TEST_CASE("Table Properties window: the Borders tab, its tiles and lines", "[gui][tables][dialog]") {
    TablePropertiesData given = gridData();
    given.table.borders.width = 1.2;  // a width of its own
    TablePropertiesDialog dialog(given, QColor(QStringLiteral("#808080")));
    QWidget& tab = *dialog.tabs()->widget(4);
    auto* none = buttonOf<QToolButton>(tab, QStringLiteral("&None"));
    auto* box = buttonOf<QToolButton>(tab, QStringLiteral("Bo&x"));
    auto* grid = buttonOf<QToolButton>(tab, QStringLiteral("&Grid"));
    auto* custom = buttonOf<QToolButton>(tab, QStringLiteral("C&ustom"));
    auto* vertical = buttonOf<QCheckBox>(tab, QStringLiteral("Inside &vertical"));
    auto* style = fieldOf<QComboBox>(tab, QStringLiteral("&Style"));
    auto* lineWidth = fieldOf<QComboBox>(tab, QStringLiteral("W&idth"));
    auto* color = fieldOf<QPushButton>(tab, QStringLiteral("&Color"));
    REQUIRE(none != nullptr);
    REQUIRE(box != nullptr);
    REQUIRE(grid != nullptr);
    REQUIRE(custom != nullptr);
    REQUIRE(vertical != nullptr);
    REQUIRE(style != nullptr);
    REQUIRE(lineWidth != nullptr);
    REQUIRE(color != nullptr);

    // The grid; the width of its own among the others
    CHECK(grid->isChecked());
    CHECK(vertical->isChecked());
    CHECK(color->text() == QStringLiteral("Automatic"));
    CHECK(lineWidth->currentData().toDouble() == Approx(1.2));
    CHECK(lineWidth->itemData(lineWidth->currentIndex() - 1).toDouble() == Approx(1.0));
    CHECK(lineWidth->itemData(lineWidth->currentIndex() + 1).toDouble() == Approx(1.5));

    // A line taken away: Custom
    vertical->setChecked(false);
    CHECK(custom->isChecked());
    CHECK(dialog.data().table.borders.lines == (BORDER_GRID & ~BORDER_COLUMNS));

    none->click();
    CHECK(dialog.data().table.borders.lines == BORDER_NONE);
    CHECK_FALSE(vertical->isChecked());
    CHECK_FALSE(style->isEnabled());
    box->click();
    CHECK(dialog.data().table.borders.lines == BORDER_BOX);
    CHECK(style->isEnabled());
    // Custom from a tile: the lines of its picture
    custom->click();
    CHECK(dialog.data().table.borders.lines == (BORDER_TOP | BORDER_BOTTOM | BORDER_ROWS));

    style->setCurrentIndex(3);
    lineWidth->setCurrentIndex(0);
    const TableBorders borders = dialog.data().table.borders;
    CHECK(borders.style == BorderStyle::Double);
    CHECK(borders.width == Approx(0.25));
}

// =============================================================================
// In Polish
// =============================================================================

TEST_CASE("Table dialogs in Polish: keys of their own, Latin letters, a small screen",
          "[gui][tables][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    const auto check = [](const QWidget& dialog, const QWidget* skipped = nullptr,
                          const QStringList& more = {}) {
        INFO(dialog.windowTitle().toStdString());
        CHECK(sameKeys(dialog, skipped, more).isEmpty());
        CHECK(foreignKeys(dialog).isEmpty());
        checkFitsSmallScreen(dialog);
    };
    SplitCellsDialog split(true, [](bool) { return 2; });
    CHECK(split.windowTitle() == QStringLiteral("Podziel komórki"));
    check(split);
    SortDialog sort(true, {QStringLiteral("Imię"), QStringLiteral("Wiek")}, true,
                    QStringLiteral("polski"));
    check(sort);
    TextToTableDialog toTable({QStringLiteral("Imię; Wiek"), QStringLiteral("Adam; 2")});
    check(toTable);
    TableToTextDialog toText({{QStringLiteral("Imię"), QStringLiteral("Wiek")}});
    check(toText);

    TablePropertiesDialog properties(gridData(), QColor());
    QStringList tabs;
    for (int tab = 0; tab < properties.tabs()->count(); ++tab) {
        tabs << properties.tabs()->tabText(tab);
    }
    CHECK(tabs == QStringList{QStringLiteral("&Tabela"), QStringLiteral("&Wiersz"),
                              QStringLiteral("&Kolumna"), QStringLiteral("Ko&mórka"),
                              QStringLiteral("&Obramowanie")});
    for (int tab = 0; tab < properties.tabs()->count(); ++tab) {
        INFO("tab " << tab);
        CHECK(sameKeys(*properties.tabs()->widget(tab), nullptr, tabs).isEmpty());
    }
    check(properties, properties.tabs(), tabs);
    // The same English words, Polish ones of their own
    CHECK(buttonOf<QRadioButton>(*properties.tabs()->widget(3), QStringLiteral("Do &góry")) != nullptr);
    CHECK(buttonOf<QCheckBox>(*properties.tabs()->widget(4), QStringLiteral("&Górna")) != nullptr);
    CHECK(buttonOf<QPushButton>(*properties.tabs()->widget(2), QStringLiteral("&Poprzednia")) != nullptr);
}

// =============================================================================
// The editor panel shows them
// =============================================================================

TEST_CASE("Editor panel: the windows of the Table menu change the text", "[gui][tables][dialog]") {
    gui::EditorPanel panel(kalahari::test::book(), kalahari::test::commands());

    SECTION("Text to Table... and back") {
        BookEditor& editor = editorOf(
            panel, QStringLiteral("<kml><p>Name; Age</p><p>Adam; 2</p><p>After</p></kml>"));
        editor.setCursorPosition({1, 3});
        editor.setSelection({{0, 0}, {1, 3}});
        answerNext<TextToTableDialog>([](TextToTableDialog& dialog) { dialog.acceptButton()->click(); });
        panel.showTextToTableDialog();
        // Qt keeps a paragraph before a table at the start
        CHECK(editor.toKml().toStdString() ==
              "<kml><p></p><table><tr><th><p>Name</p></th><th><p>Age</p></th></tr>"
              "<tr><td><p>Adam</p></td><td><p>2</p></td></tr></table><p>After</p></kml>");
        REQUIRE(editor.isInTable());

        answerNext<TableToTextDialog>([](TableToTextDialog& dialog) {
            buttonOf<QRadioButton>(dialog, QStringLiteral("Co&mmas"))->setChecked(true);
            dialog.acceptButton()->click();
        });
        panel.showTableToTextDialog();
        CHECK(editor.toKml().toStdString() == "<kml><p>Name, Age</p><p>Adam, 2</p><p>After</p></kml>");
    }

    SECTION("Split Cells..., Sort... and Table Properties...") {
        BookEditor& editor = editorOf(
            panel, QStringLiteral("<kml><p>Before</p><table>"
                                  "<tr><th><p>Name</p></th><th><p>Age</p></th></tr>"
                                  "<tr><td><p>Lucyna</p></td><td><p>10</p></td></tr>"
                                  "<tr><td><p>Adam</p></td><td><p>2</p></td></tr>"
                                  "</table><p>After</p></kml>"));
        editor.setCursorPosition({5, 0});  // Adam
        answerNext<SortDialog>([](SortDialog& dialog) { dialog.acceptButton()->click(); });
        panel.showSortDialog();
        CHECK(editor.toKml().toStdString() ==
              "<kml><p>Before</p><table><tr><th><p>Name</p></th><th><p>Age</p></th></tr>"
              "<tr><td><p>Adam</p></td><td><p>2</p></td></tr>"
              "<tr><td><p>Lucyna</p></td><td><p>10</p></td></tr></table><p>After</p></kml>");

        editor.setCursorPosition({3, 0});  // Adam, now first
        answerNext<SplitCellsDialog>([](SplitCellsDialog& dialog) { dialog.acceptButton()->click(); });
        panel.showSplitCellsDialog();
        REQUIRE(editor.tablePropertiesData().has_value());
        CHECK(editor.tablePropertiesData()->columnWidths.size() == 3);

        answerNext<TablePropertiesDialog>([](TablePropertiesDialog& dialog) {
            QWidget& tab = *dialog.tabs()->widget(0);
            buttonOf<QRadioButton>(tab, QStringLiteral("&Center"))->setChecked(true);
            dialog.acceptButton()->click();
        });
        panel.showTablePropertiesDialog();
        CHECK(editor.tablePropertiesData()->table.alignment == TableAlignment::Center);
        editor.undo();
        CHECK(editor.tablePropertiesData()->table.alignment == TableAlignment::Left);

        // Nothing changed: no step of Undo, the next one undoes the split
        const std::string split = editor.toKml().toStdString();
        answerNext<TablePropertiesDialog>([](TablePropertiesDialog& dialog) { dialog.acceptButton()->click(); });
        panel.showTablePropertiesDialog();
        CHECK(editor.toKml().toStdString() == split);
        editor.undo();
        CHECK(editor.tablePropertiesData()->columnWidths.size() == 2);
    }
}
