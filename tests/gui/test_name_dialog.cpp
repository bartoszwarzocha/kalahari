// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_name_dialog.cpp
/// @brief The windows of the book's dictionary: the table of the forms of a name, the window
///        of a name (its other names, models, forms, kind, plural and Quick Insert, in Polish,
///        English and a language without models) and the window of a spelling rule (its
///        suggested wrong spellings, the forms it covers and its keys)

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/affix_expander.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/gui/dialogs/name_dialog.h"
#include "kalahari/gui/dialogs/rule_dialog.h"
#include "kalahari/gui/name_labels.h"
#include "kalahari/gui/widgets/name_forms_table.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <algorithm>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
using core::DictionaryName;
using core::FormTable;
using core::GrammaticalCase;
using core::NameDeclension;
using core::NameKind;
using core::SpellingRule;
using gui::NameFormsTable;
using gui::dialogs::NameDialog;
using gui::dialogs::RuleDialog;

namespace {

/// The child of @p parent with the object name @p name
template <typename Child>
Child* child(const QWidget& parent, const char* name) {
    auto* found = parent.findChild<Child*>(QLatin1String(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The button of @p parent with the text @p text
QPushButton* button(const QWidget& parent, const QString& text) {
    const QList<QPushButton*> buttons = parent.findChildren<QPushButton*>();
    const auto found = std::ranges::find_if(
        buttons, [&text](const QPushButton* each) { return each->text() == text; });
    REQUIRE(found != buttons.end());
    return *found;
}

/// Write @p text in place of the text of @p field, as the writer does
void type(QLineEdit* field, const QString& text) {
    field->setText(text);
    emit field->textEdited(text);
}

/// Press a key in @p widget
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/// The texts of the rows of a list: "text (role)"
QStringList rows(const QTreeWidget* list) {
    QStringList texts;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = list->topLevelItem(i);
        texts.append(item->text(1).isEmpty()
                         ? item->text(0)
                         : QStringLiteral("%1 (%2)").arg(item->text(0), item->text(1)));
    }
    return texts;
}

/// The text of a cell of the forms: row by case, column 1 the singular, 2 the plural
QString cell(const NameFormsTable* table, int row, int column = 1) {
    const QTableWidgetItem* item = table->item(row, column);
    REQUIRE(item != nullptr);
    return item->text();
}

/// The lists of the models of the words of the one chosen in the window of a name
QList<QComboBox*> modelLists(const NameDialog& dialog) {
    for (const QGroupBox* box : dialog.findChildren<QGroupBox*>()) {
        if (box->title() == QStringLiteral("Declension")) {
            return box->findChildren<QComboBox*>();
        }
    }
    return {};
}

/// The models of the words of the one chosen in the window of a name
QStringList chosenModels(const NameDialog& dialog) {
    QStringList models;
    for (const QComboBox* list : modelLists(dialog)) {
        models.append(list->currentData().toString());
    }
    return models;
}

/// A spelling dictionary of the tests: "smok" and "elf" with thirteen endings each
class Dictionary {
public:
    Dictionary() {
        write(QStringLiteral("test.aff"),
              "SET UTF-8\nSFX A Y 13\nSFX A 0 a .\nSFX A 0 owi .\nSFX A 0 iem .\nSFX A 0 u .\n"
              "SFX A 0 i .\nSFX A 0 ów .\nSFX A 0 om .\nSFX A 0 ami .\nSFX A 0 ach .\n"
              "SFX A 0 em .\nSFX A 0 ie .\nSFX A 0 y .\nSFX A 0 o .\n");
        write(QStringLiteral("test.dic"), "2\nsmok/A\nelf/A\n");
        REQUIRE(m_expander.load(m_folder.filePath(QStringLiteral("test.aff")),
                                m_folder.filePath(QStringLiteral("test.dic"))));
    }

    [[nodiscard]] const core::AffixExpander* expander() const { return &m_expander; }

private:
    void write(const QString& name, const QByteArray& content) const {
        QFile file(m_folder.filePath(name));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    QTemporaryDir m_folder;
    core::AffixExpander m_expander;
};

}  // namespace

// =============================================================================
// The table of the forms
// =============================================================================

TEST_CASE("Name forms table: the forms of a name by case", "[gui][dictionary][names]") {
    const NameDeclension polish(QStringLiteral("pl"));
    NameFormsTable table(polish);
    CHECK(table.rowCount() == 7);
    CHECK(table.columnCount() == 2);
    CHECK(table.horizontalHeaderItem(0)->text() == QStringLiteral("Case"));
    CHECK(table.horizontalHeaderItem(1)->text() == QStringLiteral("Singular"));
    CHECK(cell(&table, 1, 0) == QStringLiteral("Genitive (of whom? of what?)"));
    CHECK(cell(&table, 1).isEmpty());

    const core::NameVariant anna =
        core::makeName(QStringLiteral("Anna Wolska"), NameKind::Character, polish).variants.front();
    table.setVariant(anna);
    CHECK(cell(&table, 0) == QStringLiteral("Anna Wolska"));
    CHECK(cell(&table, 1) == QStringLiteral("Anny Wolskiej"));
    CHECK(table.variant() == anna);
    CHECK(table.currentColumn() == 1);

    SECTION("a cell has its forms apart by commas") {
        const QStringList both{QStringLiteral("Zakopanem"), QStringLiteral("Zakopanym")};
        CHECK(NameFormsTable::cellText(both) == QStringLiteral("Zakopanem, Zakopanym"));
        CHECK(NameFormsTable::formsOfText(QStringLiteral(" Zakopanem ,, Zakopanym , Zakopanem ")) ==
              QStringList{QStringLiteral("Zakopanem"), QStringLiteral("Zakopanym")});
        CHECK(NameFormsTable::formsOfText(QStringLiteral(" , ")).isEmpty());
    }

    SECTION("the plural in a column of its own") {
        CHECK_FALSE(table.isPluralShown());
        table.setPluralShown(true);
        table.setPluralShown(true);  // shown already
        CHECK(table.isPluralShown());
        CHECK(table.columnCount() == 3);
        CHECK(table.horizontalHeaderItem(2)->text() == QStringLiteral("Plural"));
        CHECK(cell(&table, 0, 2) ==
              NameFormsTable::cellText(anna.forms.cell(GrammaticalCase::Nominative, true)));
        table.setCurrentCell(3, 2);
        table.setPluralShown(false);
        CHECK(table.columnCount() == 2);
        CHECK(table.currentColumn() == 1);
    }

    SECTION("a cell the author corrects is one of the edited cells") {
        int edited = 0;
        QObject::connect(&table, &NameFormsTable::formsEdited, [&edited]() { ++edited; });
        table.item(5, 1)->setText(QStringLiteral("Annie Wolskiej, Anni Wolskiej"));
        CHECK(edited == 1);
        CHECK(table.variant().forms.cell(GrammaticalCase::Locative, false) ==
              QStringList{QStringLiteral("Annie Wolskiej"), QStringLiteral("Anni Wolskiej")});
        CHECK(table.variant().edited ==
              QStringList{FormTable::cellKey(GrammaticalCase::Locative, false)});

        // The same forms again, written otherwise, and the column of the cases: no change
        table.item(5, 1)->setText(QStringLiteral("Annie Wolskiej , Anni Wolskiej"));
        table.item(5, 0)->setText(QStringLiteral("Locative"));
        CHECK(edited == 1);
        table.item(5, 1)->setText(QStringLiteral("Annie Wolskiej"));
        CHECK(edited == 2);
        CHECK(table.variant().edited.size() == 1);
    }

    SECTION("not to be changed in a book open read-only") {
        CHECK(table.item(1, 1)->flags().testFlag(Qt::ItemIsEditable));
        CHECK_FALSE(table.item(1, 0)->flags().testFlag(Qt::ItemIsEditable));
        table.setEditable(false);
        CHECK_FALSE(table.item(1, 1)->flags().testFlag(Qt::ItemIsEditable));
        CHECK(table.item(1, 1)->flags().testFlag(Qt::ItemIsSelectable));
    }

    SECTION("a language without cases: the form alone") {
        const NameFormsTable english(NameDeclension(QStringLiteral("en")));
        CHECK(english.rowCount() == 1);
        CHECK(english.horizontalHeaderItem(0)->text().isEmpty());
        CHECK(cell(&english, 0, 0) == QStringLiteral("Form"));
    }
}

// =============================================================================
// The window of a name
// =============================================================================

TEST_CASE("Name window: a new name with its forms and other names", "[gui][dictionary][names]") {
    const NameDeclension polish(QStringLiteral("pl"));
    NameDialog dialog(DictionaryName{}, polish, {});
    auto* list = child<QTreeWidget>(dialog, "nameVariants");
    auto* text = child<QLineEdit>(dialog, "nameText");
    const auto* hint = child<QLabel>(dialog, "nameHint");
    QPushButton* remove = button(dialog, QStringLiteral("Remove"));
    const auto* suggest = child<QPushButton>(dialog, "nameSuggest");

    // Empty: nothing to add yet
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(remove->isEnabled());
    CHECK_FALSE(suggest->isEnabled());
    CHECK_FALSE(child<QCheckBox>(dialog, "nameUnchanged")->isEnabled());
    CHECK(rows(list) == QStringList{QStringLiteral("(new name) (main)")});
    CHECK(dialog.currentVariant() == 0);

    // The name: the models of its words by their endings, its forms, and the first name and
    // the surname alone after it
    type(text, QStringLiteral("Anna Wolska"));
    CHECK(rows(list) == QStringList{QStringLiteral("Anna Wolska (main)"),
                                    QStringLiteral("Anna (first name alone)"),
                                    QStringLiteral("Wolska (surname alone)")});
    CHECK(chosenModels(dialog) == QStringList{QStringLiteral("anna"), QStringLiteral("kowalska")});
    CHECK(cell(dialog.formsTable(), 1) == QStringLiteral("Anny Wolskiej"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(suggest->isEnabled());
    CHECK(hint->text().startsWith(
        QStringLiteral("The spelling dictionary does not know the words of the name")));
    CHECK(hint->text().endsWith(QStringLiteral("A cell can have several forms, apart by commas.")));
    const QList<QComboBox*> lists = modelLists(dialog);
    REQUIRE(lists.size() == 2);
    CHECK(lists.at(1)->findData(QStringLiteral("kowalska")) >= 0);
    CHECK(lists.at(1)->itemText(lists.at(1)->findData(QStringLiteral("kowalska"))) ==
          gui::modelLabel(QStringLiteral("kowalska")));
    CHECK(lists.at(1)->findData(NameDeclension::unchanged()) >= 0);
    const QString wordLabel = QLocale().quoteString(QStringLiteral("Wolska")) +
                              QStringLiteral(" declines like:");
    CHECK(std::ranges::any_of(dialog.findChildren<QLabel*>(), [&wordLabel](const QLabel* label) {
        return label->text() == wordLabel;
    }));

    // The surname alone, chosen in the list: its own fields
    list->setCurrentItem(list->topLevelItem(2));
    CHECK(dialog.currentVariant() == 2);
    CHECK(text->text() == QStringLiteral("Wolska"));
    CHECK(remove->isEnabled());
    CHECK(cell(dialog.formsTable(), 1) == QStringLiteral("Wolskiej"));
    dialog.formsTable()->item(6, 1)->setText(QStringLiteral("Wolska!"));  // corrected

    // An other name the author adds: empty at first, not a name until written
    button(dialog, QStringLiteral("Add Other Name"))->click();
    CHECK(dialog.currentVariant() == 3);
    CHECK(rows(list).at(3) == QStringLiteral("(new name) (other name)"));
    CHECK(dialog.name().variants.size() == 3);
    type(text, QStringLiteral("Ania"));
    CHECK(rows(list).at(3) == QStringLiteral("Ania (other name)"));
    CHECK(dialog.name().variants.size() == 4);

    // The name itself changed: the surname alone the program made follows it, the one the
    // author corrected and the other names stay
    dialog.setCurrentVariant(0);
    type(text, QStringLiteral("Anna Nowak"));
    CHECK(rows(list) == QStringList{QStringLiteral("Anna Nowak (main)"),
                                    QStringLiteral("Anna (first name alone)"),
                                    QStringLiteral("Nowak (surname alone)"),
                                    QStringLiteral("Wolska (surname alone)"),
                                    QStringLiteral("Ania (other name)")});

    // Taken out: the one after it is chosen; the name itself cannot be
    dialog.setCurrentVariant(3);
    remove->click();
    CHECK(rows(list).size() == 4);
    CHECK(dialog.currentVariant() == 3);
    CHECK(text->text() == QStringLiteral("Ania"));
    dialog.setCurrentVariant(0);
    CHECK_FALSE(remove->isEnabled());
    dialog.setCurrentVariant(-1);
    dialog.setCurrentVariant(9);
    CHECK(dialog.currentVariant() == 0);

    const DictionaryName name = dialog.name();
    CHECK(name.id.isEmpty());
    CHECK(name.text() == QStringLiteral("Anna Nowak"));
    CHECK(name.kind == NameKind::Character);
    CHECK(name.variants.size() == 4);
}

TEST_CASE("Name window: models, forms, kind, plural and Quick Insert", "[gui][dictionary][names]") {
    const NameDeclension polish(QStringLiteral("pl"));
    DictionaryName anna =
        core::makeName(QStringLiteral("Anna Wolska"), NameKind::Character, polish);
    anna.id = QStringLiteral("n1");
    const NameDialog dialog(anna, polish, {});
    auto* unchanged = child<QCheckBox>(dialog, "nameUnchanged");
    const NameFormsTable* forms = dialog.formsTable();
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Save"));
    CHECK(dialog.name() == anna);

    SECTION("a model chosen for a word gives it its forms") {
        QComboBox* surname = modelLists(dialog).at(1);
        surname->setCurrentIndex(surname->findData(NameDeclension::unchanged()));
        CHECK(modelLists(dialog).at(1) == surname);  // the list stays
        CHECK(dialog.name().variants.front().models ==
              QStringList{QStringLiteral("anna"), NameDeclension::unchanged()});
        CHECK(cell(forms, 1) == QStringLiteral("Anny Wolska"));
        CHECK_FALSE(unchanged->isChecked());
    }

    SECTION("does not change: every word keeps its form, and back") {
        unchanged->setChecked(true);
        CHECK(chosenModels(dialog) ==
              QStringList{NameDeclension::unchanged(), NameDeclension::unchanged()});
        CHECK(cell(forms, 1) == QStringLiteral("Anna Wolska"));
        unchanged->setChecked(false);
        CHECK(chosenModels(dialog) ==
              QStringList{QStringLiteral("anna"), QStringLiteral("kowalska")});
        CHECK(cell(forms, 1) == QStringLiteral("Anny Wolskiej"));
    }

    SECTION("a corrected cell keeps its forms; Suggest the Forms Again gives them anew") {
        forms->item(3, 1)->setText(QStringLiteral("Annę Wolską, Anne Wolską"));
        CHECK(dialog.name().variants.front().edited ==
              QStringList{FormTable::cellKey(GrammaticalCase::Accusative, false)});
        unchanged->setChecked(true);
        CHECK(cell(forms, 3) == QStringLiteral("Annę Wolską, Anne Wolską"));
        CHECK(cell(forms, 1) == QStringLiteral("Anna Wolska"));
        child<QPushButton>(dialog, "nameSuggest")->click();
        CHECK(dialog.name().variants.front().edited.isEmpty());
        CHECK(cell(forms, 3) == QStringLiteral("Anna Wolska"));
    }

    SECTION("another kind: the models the program picks for it") {
        auto* kind = child<QComboBox>(dialog, "nameKind");
        CHECK(kind->currentText() == gui::nameKindChoice(NameKind::Character));
        kind->setCurrentIndex(kind->findData(static_cast<int>(NameKind::Place)));
        const DictionaryName place = dialog.name();
        CHECK(place.kind == NameKind::Place);
        CHECK(place.variants.front().models ==
              polish.modelsFor(QStringLiteral("Anna Wolska"), NameKind::Place));
    }

    SECTION("the plural, and Quick Insert") {
        auto* plural = child<QCheckBox>(dialog, "namePlural");
        CHECK_FALSE(forms->isPluralShown());
        plural->setChecked(true);
        CHECK(forms->isPluralShown());
        CHECK(dialog.name().plural);
        auto* quickInsert = child<QCheckBox>(dialog, "nameQuickInsert");
        CHECK(quickInsert->isChecked());
        quickInsert->setChecked(false);
        CHECK_FALSE(dialog.name().quickInsert);
    }
}

TEST_CASE("Name window: what it says of the forms, in each language", "[gui][dictionary][names]") {
    const auto hintOf = [](const NameDialog& dialog) {
        return child<QLabel>(dialog, "nameHint")->text();
    };
    const auto declension = [](const char* language) {
        return NameDeclension(QLatin1String(language));
    };

    SECTION("Polish: the spelling dictionary knows the words, or some of them") {
        const NameDeclension::DictionaryForms knowsAnna = [](const QString& word) {
            return word == QStringLiteral("Anna")
                       ? QStringList{QStringLiteral("Anna"), QStringLiteral("Anny"),
                                     QStringLiteral("Annie"), QStringLiteral("Annę"),
                                     QStringLiteral("Anną"), QStringLiteral("Anno")}
                       : QStringList();
        };
        const DictionaryName anna =
            core::makeName(QStringLiteral("Anna Wolska"), NameKind::Character, declension("pl"));
        const NameDialog some(anna, declension("pl"), knowsAnna);
        CHECK(hintOf(some).startsWith(
            QStringLiteral("The spelling dictionary does not know %1, so its forms come from")
                .arg(QLocale().quoteString(QStringLiteral("Wolska")))));

        NameDialog all(anna, declension("pl"), knowsAnna);
        all.setCurrentVariant(1);
        CHECK(hintOf(all).startsWith(
            QStringLiteral("The spelling dictionary knows the words of the name")));
    }

    SECTION("English: the singular and the plural") {
        const NameDialog dialog(core::makeName(QStringLiteral("Anna"), NameKind::Character,
                                               declension("en")),
                                declension("en"), {});
        CHECK(hintOf(dialog).startsWith(
            QStringLiteral("The forms are the singular and the plural")));
        CHECK(dialog.formsTable()->rowCount() == 1);
    }

    SECTION("a language without models: the forms the author writes") {
        const NameDialog dialog(DictionaryName{}, declension("de"), {});
        CHECK(hintOf(dialog).startsWith(QStringLiteral("The program has no models of declension")));
        CHECK(modelLists(dialog).isEmpty());
        const QList<QGroupBox*> boxes = dialog.findChildren<QGroupBox*>();
        CHECK(std::ranges::all_of(boxes, [](const QGroupBox* box) { return box->isHidden(); }));
    }
}

// =============================================================================
// The window of a spelling rule
// =============================================================================

TEST_CASE("Rule window: the wrong spellings follow the right one", "[gui][dictionary][rules]") {
    RuleDialog dialog(SpellingRule{}, nullptr, {});
    auto* right = child<QLineEdit>(dialog, "ruleRight");
    auto* wrong = child<QTreeWidget>(dialog, "ruleWrong");
    auto* newWrong = child<QLineEdit>(dialog, "ruleNewWrong");
    const auto* forms = child<QLabel>(dialog, "ruleForms");
    QPushButton* add = button(dialog, QStringLiteral("Add"));
    QPushButton* remove = button(dialog, QStringLiteral("Remove"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(dialog.wrongSpellings().isEmpty());

    // The spellings that suggest themselves
    type(right, QStringLiteral("Smok"));
    CHECK(rows(wrong) == QStringList{QStringLiteral("smok (suggested)")});
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(forms->text() == QStringLiteral("The spelling dictionary knows no other forms of the "
                                          "word: the rule covers the spellings listed."));
    type(right, QStringLiteral("e-mail"));
    CHECK(dialog.wrongSpellings() ==
          QStringList{QStringLiteral("email"), QStringLiteral("e mail")});

    // One taken out does not come back; the author can add it again
    dialog.removeWrongSpelling(QStringLiteral("email"));
    type(right, QStringLiteral("e-mail "));
    CHECK(dialog.wrongSpellings() == QStringList{QStringLiteral("e mail")});
    CHECK(dialog.addWrongSpelling(QStringLiteral(" email ")));
    CHECK(rows(wrong) ==
          QStringList{QStringLiteral("e mail (suggested)"), QStringLiteral("email")});
    CHECK_FALSE(dialog.addWrongSpelling(QStringLiteral("email")));
    CHECK_FALSE(dialog.addWrongSpelling(QStringLiteral("e-mail")));
    CHECK_FALSE(dialog.addWrongSpelling(QStringLiteral("  ")));

    // The field of another one: Add, or Enter, which does not close the window
    CHECK_FALSE(add->isEnabled());
    newWrong->setText(QStringLiteral("mail"));
    CHECK(add->isEnabled());
    press(newWrong, Qt::Key_Return);
    CHECK(newWrong->text().isEmpty());
    CHECK(dialog.wrongSpellings().contains(QStringLiteral("mail")));
    newWrong->setText(QStringLiteral("e-majl"));
    add->click();
    CHECK(dialog.wrongSpellings().contains(QStringLiteral("e-majl")));
    newWrong->setText(QStringLiteral("e mail"));  // listed
    CHECK_FALSE(add->isEnabled());
    press(newWrong, Qt::Key_Return);
    CHECK(newWrong->text() == QStringLiteral("e mail"));
    newWrong->clear();

    // Remove takes out the one chosen
    wrong->setCurrentItem(wrong->topLevelItem(0));
    CHECK(remove->isEnabled());
    remove->click();
    CHECK(dialog.wrongSpellings() ==
          QStringList{QStringLiteral("email"), QStringLiteral("mail"), QStringLiteral("e-majl")});

    // Its spellings alone, with the author's note
    child<QCheckBox>(dialog, "ruleAllForms")->setChecked(false);
    CHECK(forms->text() == QStringLiteral("Only the spellings listed, in no other forms."));
    child<QLineEdit>(dialog, "ruleNote")->setText(QStringLiteral(" As the publisher writes it. "));
    const SpellingRule rule = dialog.rule();
    CHECK(rule.right == QStringLiteral("e-mail"));
    CHECK(rule.wrong == dialog.wrongSpellings());
    CHECK_FALSE(rule.allForms);
    CHECK(rule.note == QStringLiteral("As the publisher writes it."));
    CHECK(rule.forms.value(QStringLiteral("mail")) == QStringLiteral("e-mail"));
}

TEST_CASE("Rule window: a rule of the book, and the forms it covers", "[gui][dictionary][rules]") {
    const Dictionary dictionary;

    SECTION("a rule the book has: the suggestions it does not have stay out") {
        SpellingRule smok;
        smok.id = QStringLiteral("r1");
        smok.right = QStringLiteral("Smok");
        smok.wrong = {QStringLiteral("SMOK")};
        RuleDialog dialog(smok, dictionary.expander(), {});
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Save"));
        CHECK(rows(child<QTreeWidget>(dialog, "ruleWrong")) == QStringList{QStringLiteral("SMOK")});
        type(child<QLineEdit>(dialog, "ruleRight"), QStringLiteral("Smok"));
        CHECK(dialog.wrongSpellings() == QStringList{QStringLiteral("SMOK")});
        CHECK(dialog.rule().id == QStringLiteral("r1"));

        // Its forms from the spelling dictionary, twelve at most
        const QString forms = child<QLabel>(dialog, "ruleForms")->text();
        CHECK(forms.startsWith(QStringLiteral("Forms from the spelling dictionary: ")));
        CHECK(forms.contains(QStringLiteral("Smokiem")));
        CHECK(forms.endsWith(QStringLiteral(", …")));
        CHECK(dialog.rule().forms.size() > 1);

        // The right spelling alone: nothing to save
        dialog.removeWrongSpelling(QStringLiteral("SMOK"));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        CHECK_FALSE(button(dialog, QStringLiteral("Remove"))->isEnabled());
    }

    SECTION("a word the spelling dictionary does not know: the forms of the word it ends with") {
        const RuleDialog::WordCheck isWord = [](const QString& word) {
            return word == QStringLiteral("pół") || word == QStringLiteral("elf");
        };
        const RuleDialog dialog(SpellingRule{}, dictionary.expander(), isWord);
        type(child<QLineEdit>(dialog, "ruleRight"), QStringLiteral("półelf"));
        CHECK(dialog.wrongSpellings() ==
              QStringList{QStringLiteral("pół-elf"), QStringLiteral("pół elf")});
        const QLocale locale;
        const QString forms = child<QLabel>(dialog, "ruleForms")->text();
        CHECK(forms.startsWith(QStringLiteral("Forms like those of %1, as the spelling dictionary "
                                              "does not know %2: ")
                                   .arg(locale.quoteString(QStringLiteral("elf")),
                                        locale.quoteString(QStringLiteral("półelf")))));
        CHECK(forms.contains(QStringLiteral("półelfem")));
        CHECK(dialog.rule().pattern == QStringLiteral("elf"));
    }
}
