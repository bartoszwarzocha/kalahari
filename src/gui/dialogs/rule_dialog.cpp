// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file rule_dialog.cpp
/// @brief The window "Spelling Rule" of the book's dictionary

#include "kalahari/gui/dialogs/rule_dialog.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QCheckBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// @brief The forms the window shows at most; "…" says there are more
constexpr qsizetype SHOWN_FORMS = 12;

/// @brief A wrong spelling the program suggested, in the data of its row
constexpr int SUGGESTED_ROLE = Qt::UserRole;

/// @brief The width the window needs for a line of forms
constexpr int MINIMUM_WIDTH = 560;

/// @brief The forms of the right spelling a rule covers besides it, each once, in their order
QStringList rightForms(const core::SpellingRule& rule) {
    QStringList forms;
    for (const QString& form : rule.forms) {
        if (form != rule.right && !forms.contains(form)) {
            forms.append(form);
        }
    }
    return forms;
}

}  // namespace

RuleDialog::RuleDialog(const core::SpellingRule& rule, const core::AffixExpander* forms,
                       WordCheck isWord, QWidget* parent)
    : KalahariDialog(parent), m_rule(rule), m_expander(forms), m_isWord(std::move(isWord)) {
    setObjectName(QStringLiteral("ruleDialog"));
    //: The window of a spelling rule of the book's dictionary. In Polish: Zasada pisowni
    setHeading(tr("Spelling Rule"),
               tr("How you want a word written in this book: the spelling check marks the other "
                  "spellings and offers this one."));
    setHeadingIcon(QStringLiteral("book.dictionary"));
    setMinimumWidth(MINIMUM_WIDTH);
    createFields(rule);

    // The suggestions for the right spelling the rule has; those it does not have were taken
    // out
    m_suggested = core::suggestedWrongSpellings(rule.right.simplified(), m_isWord);
    for (const QString& wrong : rule.wrong) {
        listWrong(wrong, m_suggested.contains(wrong));
    }
    for (const QString& suggested : std::as_const(m_suggested)) {
        if (!rule.wrong.contains(suggested) && !rule.id.isEmpty()) {
            m_dismissed.append(suggested);
        }
    }
    if (rule.id.isEmpty()) {
        onRightEdited();
    }

    connect(m_right, &QLineEdit::textEdited, this, &RuleDialog::onRightEdited);
    connect(m_newWrong, &QLineEdit::textChanged, this, &RuleDialog::updateButtons);
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        if (addWrongSpelling(m_newWrong->text())) {
            m_newWrong->clear();
        }
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        if (const QTreeWidgetItem* item = m_wrong->currentItem()) {
            removeWrongSpelling(item->text(0));
        }
    });
    connect(m_wrong, &QTreeWidget::currentItemChanged, this, &RuleDialog::updateButtons);
    connect(m_allForms, &QCheckBox::toggled, this, &RuleDialog::updateForms);

    //: In Polish: Dodaj / Zapisz
    setAcceptText(rule.id.isEmpty() ? tr("Add") : tr("Save"));
    updateForms();
    updateButtons();
    m_right->setFocus();
}

void RuleDialog::createFields(const core::SpellingRule& rule) {
    auto* fields = new QFormLayout();
    contentLayout()->addLayout(fields);
    m_right = new QLineEdit(rule.right, this);
    m_right->setObjectName(QStringLiteral("ruleRight"));
    //: The right spelling of the rule. In Polish: Pisz:
    fields->addRow(tr("&Write:"), m_right);

    // The wrong spellings, with a field and buttons to add and take out
    auto* wrongColumn = new QVBoxLayout();
    m_wrong = new QTreeWidget(this);
    m_wrong->setObjectName(QStringLiteral("ruleWrong"));
    //: In Polish: Błędne zapisy
    m_wrong->setAccessibleName(tr("Wrong spellings"));
    m_wrong->setColumnCount(2);
    m_wrong->setHeaderHidden(true);
    m_wrong->setRootIsDecorated(false);
    m_wrong->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_wrong->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_wrong->header()->setStretchLastSection(false);
    wrongColumn->addWidget(m_wrong);
    auto* wrongRow = new QHBoxLayout();
    m_newWrong = new QLineEdit(this);
    m_newWrong->setObjectName(QStringLiteral("ruleNewWrong"));
    //: In Polish: Inny błędny zapis
    m_newWrong->setPlaceholderText(tr("Another wrong spelling"));
    m_newWrong->setAccessibleName(m_newWrong->placeholderText());
    m_newWrong->installEventFilter(this);
    //: In Polish: Dodaj
    m_addButton = new QPushButton(tr("Add"), this);
    m_addButton->setAutoDefault(false);
    //: In Polish: Usuń
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setAutoDefault(false);
    wrongRow->addWidget(m_newWrong, 1);
    wrongRow->addWidget(m_addButton);
    wrongRow->addWidget(m_removeButton);
    wrongColumn->addLayout(wrongRow);
    auto* suggestions = new QLabel(
        tr("The program adds the spellings that suggest themselves: the other case of a capital "
           "letter, without the hyphen, with it or apart. The rule holds also where the spelling "
           "dictionary takes them for right."),
        this);
    suggestions->setWordWrap(true);
    suggestions->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    wrongColumn->addWidget(suggestions);
    //: The wrong spellings of the rule. In Polish: Zamiast:
    fields->addRow(tr("&Instead of:"), wrongColumn);

    //: The rule covers every form of the word. In Polish: Także w odmianach
    m_allForms = new QCheckBox(tr("&Also in their forms"), this);
    m_allForms->setObjectName(QStringLiteral("ruleAllForms"));
    m_allForms->setChecked(rule.allForms);
    fields->addRow(QString(), m_allForms);
    m_forms = new QLabel(this);
    m_forms->setObjectName(QStringLiteral("ruleForms"));
    m_forms->setWordWrap(true);
    m_forms->setTextFormat(Qt::PlainText);
    m_forms->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    fields->addRow(QString(), m_forms);

    m_note = new QLineEdit(rule.note, this);
    m_note->setObjectName(QStringLiteral("ruleNote"));
    //: In Polish: Krótkie wyjaśnienie, np. skąd ta pisownia (nieobowiązkowe)
    m_note->setPlaceholderText(tr("A short note, e.g. where the spelling comes from (optional)"));
    //: The author's note on the rule. In Polish: Uwaga:
    fields->addRow(tr("&Note:"), m_note);
    contentLayout()->addStretch(1);
}

core::SpellingRule RuleDialog::rule() const {
    core::SpellingRule rule = m_rule;
    rule.right = m_right->text().simplified();
    rule.wrong = wrongSpellings();
    rule.wrong.removeAll(rule.right);
    rule.allForms = m_allForms->isChecked();
    rule.note = m_note->text().trimmed();
    core::fillRuleForms(rule, m_expander);
    return rule;
}

QStringList RuleDialog::wrongSpellings() const {
    QStringList wrong;
    for (int i = 0; i < m_wrong->topLevelItemCount(); ++i) {
        wrong.append(m_wrong->topLevelItem(i)->text(0));
    }
    return wrong;
}

bool RuleDialog::addWrongSpelling(const QString& wrong) {
    const QString written = wrong.simplified();
    if (written.isEmpty() || written == m_right->text().simplified() ||
        wrongSpellings().contains(written)) {
        return false;
    }
    m_dismissed.removeAll(written);
    listWrong(written, false);
    m_wrong->setCurrentItem(m_wrong->topLevelItem(m_wrong->topLevelItemCount() - 1));
    updateForms();
    updateButtons();
    return true;
}

void RuleDialog::removeWrongSpelling(const QString& wrong) {
    for (int i = 0; i < m_wrong->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = m_wrong->topLevelItem(i);
        if (item->text(0) != wrong) {
            continue;
        }
        if (item->data(0, SUGGESTED_ROLE).toBool()) {
            m_dismissed.append(wrong);
        }
        delete m_wrong->takeTopLevelItem(i);
        break;
    }
    updateForms();
    updateButtons();
}

bool RuleDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_newWrong && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
        if (enter && !m_newWrong->text().trimmed().isEmpty()) {
            if (addWrongSpelling(m_newWrong->text())) {
                m_newWrong->clear();
            }
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

void RuleDialog::onRightEdited() {
    const QStringList fresh = core::suggestedWrongSpellings(m_right->text().simplified(),
                                                           m_isWord);
    // The suggestions for the spelling before go, unless they are suggested again
    for (int i = m_wrong->topLevelItemCount() - 1; i >= 0; --i) {
        const QTreeWidgetItem* item = m_wrong->topLevelItem(i);
        if (item->data(0, SUGGESTED_ROLE).toBool() && !fresh.contains(item->text(0))) {
            delete m_wrong->takeTopLevelItem(i);
        }
    }
    const QStringList listed = wrongSpellings();
    for (const QString& suggested : fresh) {
        if (!listed.contains(suggested) && !m_dismissed.contains(suggested)) {
            listWrong(suggested, true);
        }
    }
    m_suggested = fresh;
    updateForms();
    updateButtons();
}

void RuleDialog::listWrong(const QString& wrong, bool suggested) {
    auto* item = new QTreeWidgetItem(m_wrong);
    item->setText(0, wrong);
    item->setData(0, SUGGESTED_ROLE, suggested);
    if (suggested) {
        //: A wrong spelling the program added to the rule. In Polish: podpowiedź programu
        item->setText(1, tr("suggested"));
        item->setForeground(1, SettingsPage::noteColor());
    }
}

void RuleDialog::updateForms() {
    if (!m_allForms->isChecked()) {
        m_forms->setText(tr("Only the spellings listed, in no other forms."));
        return;
    }
    const core::SpellingRule now = rule();
    const QStringList forms = rightForms(now);
    if (now.right.isEmpty() || forms.isEmpty()) {
        m_forms->setText(tr("The spelling dictionary knows no other forms of the word: the rule "
                            "covers the spellings listed."));
        return;
    }
    QString shown = forms.mid(0, SHOWN_FORMS).join(QStringLiteral(", "));
    if (forms.size() > SHOWN_FORMS) {
        shown += QStringLiteral(", …");
    }
    const QLocale locale;
    m_forms->setText(
        now.pattern.isEmpty()
            //: %1: the forms the rule covers. In Polish: Formy ze słownika pisowni: %1
            ? tr("Forms from the spelling dictionary: %1").arg(shown)
            //: %1: the word whose forms the rule has, %2: the right spelling the spelling
            //: dictionary does not know, %3: the forms. In Polish: Formy według wzoru %1 –
            //: słownik pisowni nie zna słowa %2: %3
            : tr("Forms like those of %1, as the spelling dictionary does not know %2: %3")
                  .arg(locale.quoteString(now.pattern), locale.quoteString(now.right), shown));
}

void RuleDialog::updateButtons() {
    const QString right = m_right->text().simplified();
    const QStringList wrong = wrongSpellings();
    acceptButton()->setEnabled(!right.isEmpty() && !wrong.isEmpty() &&
                               (wrong.size() != 1 || wrong.constFirst() != right));
    const QString typed = m_newWrong->text().simplified();
    m_addButton->setEnabled(!typed.isEmpty() && typed != right && !wrong.contains(typed));
    m_removeButton->setEnabled(m_wrong->currentItem() != nullptr);
}

}  // namespace kalahari::gui::dialogs
