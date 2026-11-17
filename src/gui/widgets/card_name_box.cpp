// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_name_box.cpp
/// @brief The declension of the name of a card of the Workshop

#include "kalahari/gui/widgets/card_name_box.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/name_labels.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/widgets/name_forms_table.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QStringList>
#include <QStringView>
#include <QToolButton>
#include <QVBoxLayout>

namespace kalahari::gui {

namespace {

/// The parts of the line of the forms of the short card
constexpr QStringView FORMS_SEPARATOR = u" · ";

/// The forms of the name itself, but the first one of its nominative (the name), each once:
/// "Anny Wolskiej · Annie Wolskiej · Annę Wolską"
QStringList otherForms(const core::DictionaryName& name) {
    QStringList forms;
    if (name.variants.isEmpty()) {
        return forms;
    }
    const core::NameVariant& own = name.variants.constFirst();
    for (const QString& form : own.forms.allForms(name.plural)) {
        if (form != own.text && !forms.contains(form)) {
            forms << form;
        }
    }
    return forms;
}

/// The texts of the other names of @p name, in their order
QStringList otherNames(const core::DictionaryName& name) {
    QStringList names;
    for (qsizetype i = 1; i < name.variants.size(); ++i) {
        names << name.variants.at(i).text;
    }
    return names;
}

/// What the models of the words of @p variant are: "Anna + Kowalska"
QString modelsText(const core::NameVariant& variant) {
    QStringList models;
    for (const QString& model : variant.models) {
        // The sample word of the model, before its description ("Anna – a feminine noun")
        models << gui::modelLabel(model).section(QStringLiteral(" – "), 0, 0);
    }
    return models.join(QStringLiteral(" + "));
}

}  // namespace

CardNameBox::CardNameBox(core::ProjectManager& book, bool full, QWidget* parent)
    : QGroupBox(parent), m_book(book), m_full(full) {
    setObjectName(QStringLiteral("cardDeclension"));
    //: The group of a card with the forms of its name and its other names, which the book's
    //: dictionary has. In Polish: Odmiana
    setTitle(tr("Declension"));
    QVBoxLayout* layout = card::groupLayout(this);
    if (m_full) {
        createFullLayout(layout);
    } else {
        createShortLayout(layout);
    }
    m_otherNamesLabel = card::hintLabel(this, QString(), QStringLiteral("cardOtherNames"));
    layout->addWidget(m_otherNamesLabel);
    connect(m_nameButton, &QToolButton::clicked, this,
            [this]() { emit requestNameWindow(m_cardId); });
    installEventFilter(this);

    // The dictionary changes elsewhere too: on its page, in its windows, from the texts
    connect(&m_book, &core::ProjectManager::bookDictionaryChanged, this, [this]() {
        if (!m_cardId.isEmpty()) {
            refresh();
        }
    });
}

void CardNameBox::createShortLayout(QVBoxLayout* layout) {
    m_nameButton = new QToolButton(this);
    m_nameButton->setObjectName(QStringLiteral("cardNameChange"));
    //: The button of the group Declension of the short card that opens the window Name of the
    //: book's dictionary. In Polish: Zmień...
    m_nameButton->setText(tr("Change..."));
    m_nameButton->setAutoRaise(true);
    //: In Polish: Formy, inne imiona, liczba mnoga i wzory odmiany w oknie „Imię lub nazwa”
    m_nameButton->setToolTip(tr("The forms, the other names, the plural and the models of "
                                "declension in the window Name"));
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch(1);
    row->addWidget(m_nameButton);
    layout->addLayout(row);
    m_formsLabel = new QLabel(this);
    m_formsLabel->setObjectName(QStringLiteral("cardForms"));
    m_formsLabel->setWordWrap(true);
    m_formsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_formsLabel);
}

void CardNameBox::createFullLayout(QVBoxLayout* layout) {
    // The name itself and its other names choose whose forms the table shows
    m_variantRow = new QHBoxLayout();
    m_variantRow->setContentsMargins(0, 0, 0, 0);
    //: Before the buttons of the name of a card and of its other names, which choose whose
    //: forms the table shows. In Polish: Formy:
    m_variantRow->addWidget(new QLabel(tr("Forms of:"), this));
    m_variantGroup = new QButtonGroup(this);
    m_variantGroup->setExclusive(true);
    connect(m_variantGroup, &QButtonGroup::idClicked, this, [this](int index) {
        m_variant = index;
        refresh();
    });
    //: In Polish: Inne imię lub nazwa... (Insert)
    m_addButton = card::listButton(this, "common.add", tr("Another Name... (Insert)"),
                                   QStringLiteral("cardNameAdd"));
    connect(m_addButton, &QToolButton::clicked, this,
            [this]() { emit requestOtherName(m_cardId); });
    m_addButton->installEventFilter(this);
    m_variantRow->addWidget(m_addButton);
    m_variantRow->addStretch(1);
    layout->addLayout(m_variantRow);

    m_tableLayout = new QVBoxLayout();
    m_tableLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_tableLayout);
    m_modelLabel = card::hintLabel(this, QString(), QStringLiteral("cardNameModels"));
    layout->addWidget(m_modelLabel);

    m_nameButton = new QToolButton(this);
    m_nameButton->setObjectName(QStringLiteral("cardNameWindow"));
    //: The button of the group Declension of the full card: the window Name of the book's
    //: dictionary, where the plural and the models of declension change. In Polish: Okno
    //: „Imię lub nazwa”...
    m_nameButton->setText(tr("Name Window..."));
    m_nameButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    card::keepIcon(m_nameButton, QStringLiteral("book.dictionary"), core::IconContext::Button);
    m_nameButton->setToolTip(tr("The forms, the other names, the plural and the models of "
                                "declension in the window Name"));
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_nameButton);
    row->addStretch(1);
    layout->addLayout(row);
}

QList<QToolButton*> CardNameBox::variantButtons() const {
    QList<QToolButton*> buttons;
    if (m_variantGroup != nullptr) {
        for (QAbstractButton* button : m_variantGroup->buttons()) {
            buttons << qobject_cast<QToolButton*>(button);
        }
    }
    return buttons;
}

void CardNameBox::showCard(const QString& cardId) {
    if (cardId != m_cardId) {
        m_variant = 0;
    }
    m_cardId = cardId;
    refresh();
}

void CardNameBox::refresh() {
    const bool wasShowing = m_showing;
    m_showing = true;
    const core::DictionaryName* name = m_book.bookDictionary().findNameOfCard(m_cardId);
    const bool readOnly = m_book.isReadOnly();
    m_nameButton->setEnabled(!readOnly);
    if (name == nullptr || name->variants.isEmpty()) {
        showNoName();
    } else if (m_full) {
        m_addButton->setEnabled(!readOnly);
        showFull(*name);
    } else {
        showShort(*name);
    }
    m_showing = wasShowing;
}

void CardNameBox::showNoName() {
    //: A card whose name the book's dictionary does not have. In Polish: Słownik książki nie
    //: ma jeszcze tej nazwy ani jej form. Doda je okno „Imię lub nazwa”.
    m_otherNamesLabel->setText(tr("The book's dictionary does not have this name and its forms "
                                  "yet. The window Name adds them."));
    m_otherNamesLabel->setVisible(true);
    if (m_full) {
        fillVariantButtons(core::DictionaryName{});
        m_addButton->setEnabled(false);
        if (m_table != nullptr) {
            m_table->setVisible(false);
        }
        m_modelLabel->setVisible(false);
        return;
    }
    m_formsLabel->setVisible(false);
}

void CardNameBox::showShort(const core::DictionaryName& name) {
    const QStringList forms = otherForms(name);
    m_formsLabel->setVisible(true);
    //: The short card of a name whose forms are all the same (a name of a language without
    //: cases). In Polish: Nazwa się nie odmienia.
    m_formsLabel->setText(forms.isEmpty() ? tr("The name does not change.")
                                          : forms.join(FORMS_SEPARATOR));
    const QStringList names = otherNames(name);
    const QString shown = names.join(QStringLiteral(", "));
    m_otherNamesLabel->setText(name.kind == core::NameKind::Character
                                   //: %1: the other names of a character, apart by commas. In
                                   //: Polish: Inne imiona: %1
                                   ? tr("Other names: %1", "character").arg(shown)
                                   //: %1: the other names of a location or an item, apart by
                                   //: commas. In Polish: Inne nazwy: %1
                                   : tr("Other names: %1", "place or item").arg(shown));
    m_otherNamesLabel->setVisible(!names.isEmpty());
}

void CardNameBox::showFull(const core::DictionaryName& name) {
    m_otherNamesLabel->setVisible(false);
    if (m_variant >= name.variants.size()) {
        m_variant = 0;
    }
    fillVariantButtons(name);
    prepareTable();
    const core::NameVariant& variant = name.variants.at(m_variant);
    // The table keeps a cell the writer has just corrected, which is in the dictionary now
    if (!(m_table->variant() == variant) || m_table->isPluralShown() != name.plural) {
        m_table->setPluralShown(name.plural);
        m_table->setVariant(variant);
        card::fitRows(m_table);
    }
    m_table->setEditable(!m_book.isReadOnly());
    m_table->setVisible(true);
    const QString models = modelsText(variant);
    //: %1: the sample words of the models of declension of the words of a name, each declined
    //: as its sample: "Anna + Kowalska". In Polish: Odmienia się jak: %1
    m_modelLabel->setText(tr("Declined as: %1").arg(models));
    m_modelLabel->setVisible(!models.isEmpty());
}

void CardNameBox::fillVariantButtons(const core::DictionaryName& name) {
    QList<QToolButton*> buttons = variantButtons();
    // The buttons there are take the texts anew; those over go, new ones come before "+"
    while (buttons.size() > name.variants.size()) {
        QToolButton* extra = buttons.takeLast();
        m_variantGroup->removeButton(extra);
        m_variantRow->removeWidget(extra);
        extra->hide();
        extra->deleteLater();  // it may be the one whose key is being handled
    }
    QWidget* previous = buttons.isEmpty() ? m_addButton->previousInFocusChain()
                                          : buttons.constLast();
    for (qsizetype i = buttons.size(); i < name.variants.size(); ++i) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("cardNameVariant"));
        button->setCheckable(true);
        button->installEventFilter(this);
        m_variantGroup->addButton(button, static_cast<int>(i));
        // Before "+" and the stretch at the end of the row, and so with the keys
        m_variantRow->insertWidget(m_variantRow->count() - 2, button);
        setTabOrder(previous, button);
        previous = button;
        buttons << button;
    }
    for (qsizetype i = 0; i < buttons.size(); ++i) {
        const core::NameVariant& variant = name.variants.at(i);
        buttons.at(i)->setText(variant.text);
        buttons.at(i)->setToolTip(variantTip(i, variant));
    }
    if (QAbstractButton* chosen = m_variantGroup->button(m_variant)) {
        chosen->setChecked(true);
    }
}

QString CardNameBox::variantTip(qsizetype index, const core::NameVariant& variant) {
    if (index == 0) {
        //: The button of the name of the card itself, among its other names. In Polish: Nazwa
        //: karty
        return tr("The name itself");
    }
    //: An other name of a card that the program added: the first name or the surname alone.
    //: In Polish: Dodał program
    return variant.automatic ? tr("Added by the program")
                             //: In Polish: Dodał autor
                             : tr("Added by the author");
}

QList<QWidget*> CardNameBox::keyParts() const {
    QList<QWidget*> parts;
    if (m_full) {
        for (QToolButton* button : variantButtons()) {
            parts << button;
        }
        parts << m_addButton;
        if (m_table != nullptr) {
            parts << m_table;
        }
    }
    parts << m_nameButton;
    return parts;
}

void CardNameBox::prepareTable() {
    const QString language = m_book.bookLanguage();
    if (m_table != nullptr && language == m_language) {
        return;
    }
    if (m_table != nullptr) {
        m_tableLayout->removeWidget(m_table);
        m_table->hide();
        m_table->deleteLater();  // it may be the one whose key is being handled
    }
    m_language = language;
    m_table = new NameFormsTable(core::NameDeclension(language), this);
    m_table->setObjectName(QStringLiteral("cardFormsTable"));
    //: In Polish: Formy według przypadków
    m_table->setAccessibleName(tr("Forms by case"));
    m_table->installEventFilter(this);
    connect(m_table, &NameFormsTable::formsEdited, this, &CardNameBox::saveForms);
    m_tableLayout->addWidget(m_table);
    setTabOrder(m_addButton, m_table);
    setTabOrder(m_table, m_nameButton);
}

void CardNameBox::saveForms() {
    const core::DictionaryName* own = m_book.bookDictionary().findNameOfCard(m_cardId);
    if (m_showing || m_table == nullptr || own == nullptr || m_variant >= own->variants.size()) {
        return;
    }
    core::DictionaryName name = *own;
    name.variants[m_variant] = m_table->variant();
    m_book.changeBookDictionary(
        [&name](core::BookDictionary& dictionary) { dictionary.putName(name); });
}

bool CardNameBox::eventFilter(QObject* watched, QEvent* event) {
    // As "+": not in a book open read-only, nor for a card without a name in the dictionary
    if (!m_full || !card::isKeyEvent(event) || !m_addButton->isEnabled() ||
        !card::isListKey(static_cast<const QKeyEvent*>(event), Qt::Key_Insert)) {
        return QGroupBox::eventFilter(watched, event);
    }
    // The same key of the window's commands waits while the group has the keys
    event->accept();
    if (event->type() == QEvent::KeyPress) {
        emit requestOtherName(m_cardId);
    }
    return true;
}

}  // namespace kalahari::gui
