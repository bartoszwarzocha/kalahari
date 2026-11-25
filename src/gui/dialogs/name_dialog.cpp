// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file name_dialog.cpp
/// @brief The window "Name" of the book's dictionary

#include "kalahari/gui/dialogs/name_dialog.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/name_labels.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/widgets/name_forms_table.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// @brief The kinds of names, in the order of the list
constexpr std::array<core::NameKind, 4> KINDS = {core::NameKind::Character, core::NameKind::Place,
                                                 core::NameKind::Item, core::NameKind::Other};

/// @brief The width the window needs for the list, the models and two columns of forms
constexpr int MINIMUM_WIDTH = 760;

/// @brief The width of the list of the names of the entry
constexpr int LIST_WIDTH = 210;

/// @brief The space between the list and the fields of the one chosen
constexpr int COLUMN_SPACING = 16;

/// @brief The pause in typing after which the models and the forms of the text are found:
///        the dictionary takes a while for every word, and the writer types on meanwhile
constexpr int FORMS_DELAY_MS = 400;

/// @brief The other names of a new name follow the name itself: the first name and the surname
///        alone the program made for it (@p made) take the place of those it made before, but
///        of those the author corrected; the other names the author added stay
void followName(core::DictionaryName& name, const core::DictionaryName& made) {
    QList<core::NameVariant> variants{name.variants.constFirst()};
    for (qsizetype i = 1; i < made.variants.size(); ++i) {
        variants.append(made.variants.at(i));
    }
    for (qsizetype i = 1; i < name.variants.size(); ++i) {
        const core::NameVariant& kept = name.variants.at(i);
        const bool madeAgain = kept.automatic && kept.edited.isEmpty();
        const bool listed = std::ranges::any_of(variants, [&kept](const core::NameVariant& other) {
            return other.text == kept.text;
        });
        if (!madeAgain && !listed) {
            variants.append(kept);
        }
    }
    name.variants = std::move(variants);
}

/// @brief Whether every word keeps its form in every case: each has the model unchanged()
bool keepsForms(const QStringList& models) {
    const QString unchanged = core::NameDeclension::unchanged();
    return !models.isEmpty() && std::ranges::all_of(models, [&unchanged](const QString& model) {
        return model == unchanged;
    });
}

}  // namespace

NameDialog::NameDialog(const core::DictionaryName& name, core::NameDeclension declension,
                       core::NameDeclension::DictionaryForms dictionary, QWidget* parent)
    : KalahariDialog(parent),
      m_name(name),
      m_declension(std::move(declension)),
      m_dictionary(std::move(dictionary)),
      m_new(name.id.isEmpty()) {
    if (m_name.variants.isEmpty()) {
        m_name.variants.append(core::NameVariant{});
    }
    setObjectName(QStringLiteral("nameDialog"));
    //: The window of a name of the book's dictionary. In Polish: Imię lub nazwa
    setHeading(tr("Name"),
               tr("A name of the book in all its forms: the spelling check takes them for right, "
                  "Quick Insert offers them and Find in Book finds them."));
    setHeadingIcon(QStringLiteral("book.dictionary"));
    setMinimumWidth(MINIMUM_WIDTH);

    auto* columns = new QHBoxLayout();
    columns->setSpacing(COLUMN_SPACING);
    contentLayout()->addLayout(columns, 1);

    // The name itself and its other names
    auto* left = new QVBoxLayout();
    //: The list of the name itself and its other names. In Polish: Nazwy tej pozycji
    auto* listLabel = new QLabel(tr("Names of this entry"), this);
    left->addWidget(listLabel);
    m_list = new QTreeWidget(this);
    m_list->setObjectName(QStringLiteral("nameVariants"));
    m_list->setAccessibleName(listLabel->text());
    m_list->setColumnCount(2);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setFixedWidth(LIST_WIDTH);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(false);
    left->addWidget(m_list, 1);
    auto* listButtons = new QHBoxLayout();
    //: Adds a diminutive or a nickname of the name. In Polish: Dodaj inną nazwę
    m_addButton = new QPushButton(tr("Add Other Name"), this);
    //: In Polish: Usuń
    m_removeButton = new QPushButton(tr("Remove"), this);
    listButtons->addWidget(m_addButton);
    listButtons->addWidget(m_removeButton);
    left->addLayout(listButtons);
    columns->addLayout(left);

    // The fields of the one chosen
    auto* right = new QVBoxLayout();
    auto* fields = new QFormLayout();
    m_text = new QLineEdit(this);
    m_text->setObjectName(QStringLiteral("nameText"));
    //: In Polish: Imię lub nazwa:
    fields->addRow(tr("&Name:"), m_text);
    m_kind = new QComboBox(this);
    m_kind->setObjectName(QStringLiteral("nameKind"));
    for (const core::NameKind kind : KINDS) {
        m_kind->addItem(nameKindChoice(kind), static_cast<int>(kind));
    }
    m_kind->setCurrentIndex(m_kind->findData(static_cast<int>(m_name.kind)));
    //: What the name names: a character, a place... In Polish: Rodzaj:
    fields->addRow(tr("&Kind:"), m_kind);
    right->addLayout(fields);

    //: How the words of the name decline. In Polish: Odmiana
    m_modelsBox = new QGroupBox(tr("Declension"), this);
    auto* modelsBoxLayout = new QVBoxLayout(m_modelsBox);
    m_modelsLayout = new QFormLayout();
    modelsBoxLayout->addLayout(m_modelsLayout);
    //: Every word of the name has the same form in every case. In Polish: Nie odmienia się
    m_unchanged = new QCheckBox(tr("Does not change"), m_modelsBox);
    m_unchanged->setObjectName(QStringLiteral("nameUnchanged"));
    modelsBoxLayout->addWidget(m_unchanged);
    m_modelsBox->setVisible(m_declension.models().size() > 1);
    right->addWidget(m_modelsBox);

    m_forms = new NameFormsTable(m_declension, this);
    m_forms->setPluralShown(m_name.plural);
    right->addWidget(m_forms, 1);
    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("nameHint"));
    m_hint->setWordWrap(true);
    m_hint->setTextFormat(Qt::PlainText);
    m_hint->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    right->addWidget(m_hint);
    //: Gives every cell of the table the forms of the models again. In Polish: Podpowiedz
    //: formy od nowa
    m_suggestButton = new QPushButton(tr("Suggest the Forms Again"), this);
    m_suggestButton->setObjectName(QStringLiteral("nameSuggest"));
    auto* suggestRow = new QHBoxLayout();
    suggestRow->addWidget(m_suggestButton);
    suggestRow->addStretch(1);
    right->addLayout(suggestRow);
    //: In Polish: Także liczba mnoga (np. „u Wolskich”)
    m_plural = new QCheckBox(tr("The plural too (e.g. \"at the Wolskis'\")"), this);
    m_plural->setObjectName(QStringLiteral("namePlural"));
    m_plural->setChecked(m_name.plural);
    right->addWidget(m_plural);
    //: In Polish: Pokazuj w szybkim wstawianiu (@)
    m_quickInsert = new QCheckBox(tr("Show in Quick Insert (@)"), this);
    m_quickInsert->setObjectName(QStringLiteral("nameQuickInsert"));
    m_quickInsert->setChecked(m_name.quickInsert);
    right->addWidget(m_quickInsert);
    columns->addLayout(right, 1);

    // The models and the forms of a text typed come at a pause (user's test of 16.11)
    m_formsTimer = new QTimer(this);
    m_formsTimer->setObjectName(QStringLiteral("nameFormsTimer"));
    m_formsTimer->setSingleShot(true);
    m_formsTimer->setInterval(FORMS_DELAY_MS);
    connect(m_formsTimer, &QTimer::timeout, this, &NameDialog::formsOfText);

    connect(m_list, &QTreeWidget::currentItemChanged, this, [this]() {
        if (m_showing || m_list->currentItem() == nullptr) {
            return;
        }
        const int index = m_list->indexOfTopLevelItem(m_list->currentItem());
        if (!m_formsTimer->isActive()) {
            setCurrentVariant(index);
            return;
        }
        // Chosen while a text was typed: its forms first, out of the signal of the list,
        // which they may fill anew
        QMetaObject::invokeMethod(
            this,
            [this, index]() {
                finishTyping();
                setCurrentVariant(index);
            },
            Qt::QueuedConnection);
    });
    connect(m_addButton, &QPushButton::clicked, this, &NameDialog::addVariant);
    connect(m_removeButton, &QPushButton::clicked, this, &NameDialog::removeVariant);
    connect(m_text, &QLineEdit::textEdited, this, &NameDialog::onTextEdited);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &NameDialog::onKindChanged);
    connect(m_unchanged, &QCheckBox::toggled, this, &NameDialog::onUnchangedToggled);
    connect(m_forms, &NameFormsTable::formsEdited, this, [this]() {
        core::NameVariant& variant = m_name.variants[m_current];
        variant.forms = m_forms->variant().forms;
        variant.edited = m_forms->variant().edited;
    });
    connect(m_suggestButton, &QPushButton::clicked, this, [this]() {
        finishTyping();
        m_name.variants[m_current].edited.clear();
        refreshCurrent();
    });
    connect(m_plural, &QCheckBox::toggled, this, [this](bool plural) {
        m_name.plural = plural;
        m_forms->setPluralShown(plural);
    });
    connect(m_quickInsert, &QCheckBox::toggled, this,
            [this](bool shown) { m_name.quickInsert = shown; });

    //: In Polish: Dodaj / Zapisz
    setAcceptText(m_new ? tr("Add") : tr("Save"));
    fillList();
    showVariant();
    m_text->setFocus();
}

core::DictionaryName NameDialog::name() const {
    core::DictionaryName name = m_name;
    for (core::NameVariant& variant : name.variants) {
        variant.text = variant.text.simplified();
    }
    // The other names left empty are not names
    for (qsizetype i = name.variants.size() - 1; i > 0; --i) {
        if (name.variants.at(i).text.isEmpty()) {
            name.variants.removeAt(i);
        }
    }
    return name;
}

int NameDialog::currentVariant() const {
    return m_current;
}

bool NameDialog::typing() const {
    return m_formsTimer->isActive();
}

void NameDialog::finishTyping() {
    if (m_formsTimer->isActive()) {
        m_formsTimer->stop();
        formsOfText();
    }
}

void NameDialog::accept() {
    finishTyping();
    KalahariDialog::accept();
}

void NameDialog::setCurrentVariant(int index) {
    finishTyping();
    if (index < 0 || index >= m_name.variants.size()) {
        return;
    }
    m_current = index;
    showVariant();
}

void NameDialog::fillList() {
    m_showing = true;
    m_list->clear();
    for (int i = 0; i < m_name.variants.size(); ++i) {
        const QString text = m_name.variants.at(i).text.simplified();
        auto* item = new QTreeWidgetItem(m_list);
        //: An other name not written yet, in the list of the names. In Polish: (nowa nazwa)
        item->setText(0, text.isEmpty() ? tr("(new name)") : text);
        item->setText(1, roleOf(i));
        item->setForeground(1, SettingsPage::noteColor());
    }
    m_list->setCurrentItem(m_list->topLevelItem(m_current));
    m_showing = false;
}

void NameDialog::showVariant() {
    m_showing = true;
    core::NameVariant& variant = m_name.variants[m_current];
    const QString text = variant.text.simplified();
    if (variant.models.size() != core::NameDeclension::wordsOf(text).size()) {
        variant.models = pickedModels(text, m_name.kind);
    }
    m_list->setCurrentItem(m_list->topLevelItem(m_current));
    m_text->setText(variant.text);
    fillModels();
    m_forms->setVariant(variant);
    m_showing = false;
    updateHint();
    updateButtons();
}

void NameDialog::fillModels() {
    const bool showing = m_showing;
    m_showing = true;
    while (m_modelsLayout->rowCount() > 0) {
        m_modelsLayout->removeRow(0);
    }
    m_models.clear();
    const core::NameVariant& variant = m_name.variants.at(m_current);
    const QStringList words = core::NameDeclension::wordsOf(variant.text);
    m_modelWords = words;
    const QString unchanged = core::NameDeclension::unchanged();
    for (int i = 0; i < words.size(); ++i) {
        const QString& word = words.at(i);
        const QString current = variant.models.value(i, unchanged);
        auto* models = new QComboBox(m_modelsBox);
        for (const QString& model : m_declension.models()) {
            if (model == current || m_declension.fits(word, model)) {
                models->addItem(modelLabel(model), model);
            }
        }
        models->setCurrentIndex(std::max(0, models->findData(current)));
        connect(models, &QComboBox::currentIndexChanged, this, [this, i, models]() {
            onModelChosen(i, models->currentData().toString());
        });
        //: The model of declension of a word of the name; %1 is the word in quotes. In
        //: Polish: %1 odmienia się jak:
        m_modelsLayout->addRow(tr("%1 declines like:").arg(QLocale().quoteString(word)), models);
        m_models.append(models);
    }
    m_unchanged->setEnabled(!words.isEmpty());
    m_unchanged->setChecked(!words.isEmpty() && keepsForms(variant.models));
    m_showing = showing;
}

void NameDialog::onTextEdited(const QString& text) {
    if (m_showing) {
        return;
    }
    // The text at once, in the field and in the list; its models and forms at a pause
    m_name.variants[m_current].text = text;
    if (QTreeWidgetItem* item = m_list->topLevelItem(m_current)) {
        const QString shown = text.simplified();
        item->setText(0, shown.isEmpty() ? tr("(new name)") : shown);
    }
    updateButtons();
    m_formsTimer->start();
}

void NameDialog::formsOfText() {
    core::NameVariant& variant = m_name.variants[m_current];
    const QString text = variant.text;
    variant.models = pickedModels(text.simplified(), m_name.kind);
    core::refreshForms(variant, m_name.kind, m_declension, m_dictionary);
    if (m_new && m_current == 0) {
        followName(m_name, core::makeName(text.simplified(), m_name.kind, m_declension,
                                          m_dictionary));
    }
    fillList();
    m_showing = true;
    fillModels();
    m_forms->setVariant(m_name.variants.at(m_current));
    m_showing = false;
    updateHint();
    updateButtons();
}

void NameDialog::onKindChanged() {
    if (m_showing) {
        return;
    }
    finishTyping();
    const core::NameKind before = m_name.kind;
    m_name.kind = static_cast<core::NameKind>(m_kind->currentData().toInt());
    for (core::NameVariant& variant : m_name.variants) {
        // The models the author chose stay
        const QString text = variant.text.simplified();
        if (variant.models == pickedModels(text, before)) {
            variant.models = pickedModels(text, m_name.kind);
        }
        core::refreshForms(variant, m_name.kind, m_declension, m_dictionary);
    }
    showVariant();
}

void NameDialog::onModelChosen(int word, const QString& model) {
    if (m_showing) {
        return;
    }
    if (m_formsTimer->isActive()) {
        // Chosen while the text was typed: the rows of its words first, out of the signal of
        // the list chosen in (they are made anew); the choice stays when the word is there
        const QString chosenFor = m_modelWords.value(word);
        QMetaObject::invokeMethod(
            this,
            [this, word, model, chosenFor]() {
                finishTyping();
                if (word < m_models.size() && m_modelWords.value(word) == chosenFor) {
                    QComboBox* models = m_models.at(word);
                    models->setCurrentIndex(std::max(0, models->findData(model)));
                }
            },
            Qt::QueuedConnection);
        return;
    }
    core::NameVariant& variant = m_name.variants[m_current];
    if (word < 0 || word >= variant.models.size()) {
        return;
    }
    variant.models[word] = model;
    // The lists of the models stay: the one chosen in is still showing its choice
    core::refreshForms(variant, m_name.kind, m_declension, m_dictionary);
    m_showing = true;
    m_unchanged->setChecked(keepsForms(variant.models));
    m_forms->setVariant(variant);
    m_showing = false;
    updateHint();
}

void NameDialog::onUnchangedToggled(bool unchanged) {
    if (m_showing) {
        return;
    }
    finishTyping();
    core::NameVariant& variant = m_name.variants[m_current];
    const QString text = variant.text.simplified();
    variant.models = unchanged ? QStringList(core::NameDeclension::wordsOf(text).size(),
                                             core::NameDeclension::unchanged())
                               : pickedModels(text, m_name.kind);
    refreshCurrent();
}

void NameDialog::refreshCurrent() {
    core::NameVariant& variant = m_name.variants[m_current];
    core::refreshForms(variant, m_name.kind, m_declension, m_dictionary);
    m_showing = true;
    fillModels();
    m_forms->setVariant(variant);
    m_showing = false;
    updateHint();
}

void NameDialog::addVariant() {
    finishTyping();
    m_name.variants.append(core::NameVariant{});
    m_current = static_cast<int>(m_name.variants.size()) - 1;
    fillList();
    showVariant();
    m_text->setFocus();
}

void NameDialog::removeVariant() {
    finishTyping();
    if (m_current <= 0) {
        return;
    }
    m_name.variants.removeAt(m_current);
    m_current = std::min(m_current, static_cast<int>(m_name.variants.size()) - 1);
    fillList();
    showVariant();
}

void NameDialog::updateHint() {
    QString hint;
    if (m_declension.models().size() <= 1) {
        hint = tr("The program has no models of declension for the language of the book: "
                  "write the forms yourself.");
    } else if (m_declension.cases().size() == 1) {
        hint = tr("The forms are the singular and the plural; the spelling check takes the forms "
                  "with 's (Anna's) for right by itself.");
    } else {
        QStringList unknown;
        const QStringList words =
            core::NameDeclension::wordsOf(m_name.variants.at(m_current).text);
        for (const QString& word : words) {
            if (!m_dictionary || m_dictionary(word).isEmpty()) {
                unknown.append(QLocale().quoteString(word));
            }
        }
        if (unknown.isEmpty()) {
            hint = tr("The spelling dictionary knows the words of the name and confirms the forms "
                      "of the models with them. Check the forms: the program only suggests them.");
        } else if (unknown.size() == words.size()) {
            hint = tr("The spelling dictionary does not know the words of the name, so the forms "
                      "come from the models of the same endings alone. Check them and correct "
                      "them if needed.");
        } else {
            //: %1: the words in quotes the spelling dictionary does not know
            hint = tr("The spelling dictionary does not know %1, so its forms come from the "
                      "model alone. Check them and correct them if needed.")
                       .arg(unknown.join(QStringLiteral(", ")));
        }
        hint += QLatin1Char(' ') + tr("A cell can have several forms, apart by commas.");
    }
    m_hint->setText(hint);
}

void NameDialog::updateButtons() {
    acceptButton()->setEnabled(!m_name.variants.constFirst().text.simplified().isEmpty());
    m_removeButton->setEnabled(m_current > 0);
    m_suggestButton->setEnabled(!m_name.variants.at(m_current).text.simplified().isEmpty());
}

QStringList NameDialog::pickedModels(const QString& text, core::NameKind kind) const {
    return m_declension.modelsFor(text, kind, m_dictionary);
}

QString NameDialog::roleOf(int index) const {
    if (index == 0) {
        //: The name itself, in the list of the names of an entry. In Polish: główna
        return tr("main");
    }
    const core::NameVariant& variant = m_name.variants.at(index);
    if (!variant.automatic) {
        //: A diminutive or a nickname. In Polish: inna nazwa
        return tr("other name");
    }
    const QStringList words = core::NameDeclension::wordsOf(m_name.variants.constFirst().text);
    if (!words.isEmpty() && variant.text == words.constFirst()) {
        //: In Polish: samo imię
        return tr("first name alone");
    }
    //: In Polish: samo nazwisko
    return tr("surname alone");
}

}  // namespace kalahari::gui::dialogs
