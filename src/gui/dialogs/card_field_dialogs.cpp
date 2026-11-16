// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_field_dialogs.cpp
/// @brief Card Field and List Values: a field the writer adds to the cards of a kind, and the
/// values of its list

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/dialogs/card_fields_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace kalahari::gui::dialogs {

namespace {

/// The id of a value of the list
constexpr int CHOICE_ROLE = Qt::UserRole;
/// The rows the list of the values shows
constexpr int CHOICE_ROWS = 8;
/// The width of the name of a field, in average characters
constexpr int NAME_CHARS = 32;

/// Whether @p event is key @p key without modifiers
bool isKey(const QEvent* event, int key) {
    return event->type() == QEvent::KeyPress &&
           card::isListKey(static_cast<const QKeyEvent*>(event), key);
}

/// The kinds of fields of the writer, in the order of the list
constexpr std::array<core::CardFieldType, 4> TYPES{
    core::CardFieldType::Text, core::CardFieldType::LongText, core::CardFieldType::Choice,
    core::CardFieldType::Card};

/// The name of kind of field @p type in the list of the kinds
QString typeName(core::CardFieldType type) {
    switch (type) {
    case core::CardFieldType::LongText:
        //: A kind of field of a card: several lines. In Polish: Dłuższy tekst
        return CardFieldDialog::tr("Longer text");
    case core::CardFieldType::Choice:
        //: A kind of field of a card: values to choose from. In Polish: Lista do wyboru
        return CardFieldDialog::tr("List to choose from");
    case core::CardFieldType::Card:
        //: A kind of field of a card: the name of another card. In Polish: Karta
        return CardFieldDialog::tr("Card");
    case core::CardFieldType::Text:
        break;
    }
    //: A kind of field of a card: one line. In Polish: Tekst
    return CardFieldDialog::tr("Text");
}

/// Whether a field of kind @p from can become a field of kind @p to: a text, a longer text
bool canBecome(core::CardFieldType from, core::CardFieldType to) {
    const auto isText = [](core::CardFieldType type) {
        return type == core::CardFieldType::Text || type == core::CardFieldType::LongText;
    };
    return from == to || (isText(from) && isText(to));
}

}  // namespace

CardFieldDialog::CardFieldDialog(const core::KindRef& kind, const core::BookCardField& field,
                                 const QStringList& names, QWidget* parent)
    : KalahariDialog(parent),
      m_field(field),
      m_names(names),
      m_name(new QLineEdit(this)),
      m_type(new QComboBox(this)),
      m_cardKind(new QComboBox(this)),
      m_inverse(new QLineEdit(this)),
      m_note(new QLabel(this)) {
    setObjectName(QStringLiteral("cardFieldDialog"));
    const bool isNew = field.id.isEmpty();
    setHeading(isNew
                   //: The window that adds a field to the cards of a kind. In Polish: Nowe
                   //: pole kart
                   ? tr("New Card Field")
                   //: The window that changes a field the writer added. In Polish: Pole kart
                   : tr("Card Field"),
               //: In Polish: Pole mają wszystkie karty tego rodzaju.
               tr("All the cards of this kind have the field."));
    setHeadingIcon(QStringLiteral("card.fields"));
    setCompactHeading(true);
    setAcceptText(isNew
                      //: In Polish: &Dodaj
                      ? tr("&Add")
                      //: In Polish: &Zapisz
                      : tr("&Save"));

    m_name->setObjectName(QStringLiteral("cardFieldName"));
    m_name->setText(field.name);
    m_name->setMinimumWidth(fontMetrics().averageCharWidth() * NAME_CHARS);
    m_type->setObjectName(QStringLiteral("cardFieldType"));
    for (const core::CardFieldType type : TYPES) {
        if (isNew || canBecome(field.type, type)) {
            m_type->addItem(typeName(type), static_cast<int>(type));
        }
    }
    m_type->setCurrentIndex(std::max(0, m_type->findData(static_cast<int>(field.type))));
    m_cardKind->setObjectName(QStringLiteral("cardFieldCardKind"));
    if (kind.package != nullptr) {
        for (const core::ElementKind& each : kind.package->kinds) {
            if (each.isCard()) {
                m_cardKind->addItem(
                    core::ArtProvider::getInstance().getIcon(each.icon, core::IconContext::Menu),
                    each.name.text(programLanguage()), each.id);
            }
        }
    }
    const QString cardKind = field.cardKind.isEmpty() && kind ? kind.kind->id : field.cardKind;
    m_cardKind->setCurrentIndex(std::max(0, m_cardKind->findData(cardKind)));
    m_inverse->setObjectName(QStringLiteral("cardFieldInverse"));
    m_inverse->setText(field.inverseName);
    //: The name of the relation the named card has, when it is empty. In Polish: tak samo
    //: w obie strony
    m_inverse->setPlaceholderText(tr("the same both ways"));
    m_note->setObjectName(QStringLiteral("cardFieldNote"));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    m_note->setForegroundRole(QPalette::PlaceholderText);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    //: In Polish: &Nazwa pola:
    auto* nameLabel = new QLabel(tr("Field &name:"), this);
    nameLabel->setBuddy(m_name);
    form->addRow(nameLabel, m_name);
    //: In Polish: &Rodzaj pola:
    auto* typeLabel = new QLabel(tr("&Kind of field:"), this);
    typeLabel->setBuddy(m_type);
    form->addRow(typeLabel, m_type);
    //: The kind of the cards a field of the kind Card names. In Polish: &Karta:
    m_cardKindLabel = new QLabel(tr("&Card of:"), this);
    m_cardKindLabel->setBuddy(m_cardKind);
    form->addRow(m_cardKindLabel, m_cardKind);
    //: What the card is to the card the field names: "Uczeń" for "Mentor". In Polish: &W drugą
    //: stronę:
    m_inverseLabel = new QLabel(tr("The &other way:"), this);
    m_inverseLabel->setBuddy(m_inverse);
    form->addRow(m_inverseLabel, m_inverse);
    form->addRow(QString(), m_note);
    contentLayout()->addLayout(form);
    contentLayout()->addStretch(1);

    connect(m_name, &QLineEdit::textChanged, this, &CardFieldDialog::update);
    connect(m_type, &QComboBox::currentIndexChanged, this, &CardFieldDialog::update);
    connect(m_cardKind, &QComboBox::currentIndexChanged, this, &CardFieldDialog::update);
    update();
    m_name->setFocus(Qt::OtherFocusReason);
}

core::BookCardField CardFieldDialog::field() const {
    core::BookCardField field = m_field;
    field.name = m_name->text().simplified();
    field.type = static_cast<core::CardFieldType>(m_type->currentData().toInt());
    if (field.type == core::CardFieldType::Card) {
        field.cardKind = m_cardKind->currentData().toString();
        field.inverseName = m_inverse->text().simplified();
    } else {
        field.cardKind.clear();
        field.relation.clear();
        field.inverseName.clear();
    }
    if (field.type != core::CardFieldType::Choice) {
        field.choices.clear();
    }
    return field;
}

void CardFieldDialog::update() {
    const core::BookCardField shown = field();
    const bool card = shown.type == core::CardFieldType::Card;
    m_cardKindLabel->setVisible(card);
    m_cardKind->setVisible(card);
    m_inverseLabel->setVisible(card);
    m_inverse->setVisible(card);
    const bool taken = std::any_of(m_names.cbegin(), m_names.cend(), [&shown](const QString& name) {
        return name.trimmed().compare(shown.name, Qt::CaseInsensitive) == 0;
    });
    QString note;
    if (taken) {
        //: In Polish: Karty mają już pole o tej nazwie. Pole ukryte włącza się w oknie „Pola
        //: kart”.
        note = tr("The cards have a field of this name already. A hidden field is shown in the "
                  "window Card Fields.");
    } else if (card) {
        //: %1 is the name of the field. In Polish: Pole podpowiada nazwy kart i zapisuje
        //: relację „%1” z kartą, którą nazywa; przyjmuje też każdy inny tekst.
        note = tr("The field suggests the names of the cards and keeps the relation \"%1\" with "
                  "the card it names; it takes any other text too.")
                   .arg(shown.name.isEmpty() ? typeName(shown.type) : shown.name);
    } else if (shown.type == core::CardFieldType::Choice) {
        //: In Polish: Wartości listy ustala „Wartości listy...” w oknie „Pola kart”.
        note = tr("The values of the list are set with List Values... in the window Card "
                  "Fields.");
    }
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
    acceptButton()->setEnabled(!shown.name.isEmpty() && !taken);
}

CardChoicesDialog::CardChoicesDialog(const QString& fieldName,
                                     const QList<core::CardFieldChoice>& choices,
                                     QWidget* parent)
    : KalahariDialog(parent),
      m_all(choices),
      m_list(new QListWidget(this)) {
    setObjectName(QStringLiteral("cardChoicesDialog"));
    //: The window of the values of a list of a field of the cards; %1 is the field. In Polish:
    //: Wartości listy: %1
    setHeading(tr("List Values: %1").arg(fieldName),
               //: In Polish: Kolejność tu to kolejność na liście karty. Karta z wartością,
               //: która znika z listy, zachowuje ją jako tekst.
               tr("The order here is the order of the list of the card. A card with a value "
                  "that goes from the list keeps it as text."));
    setHeadingIcon(QStringLiteral("card.fields"));
    setCompactHeading(true);
    //: In Polish: &Zapisz
    setAcceptText(tr("&Save"));

    m_list->setObjectName(QStringLiteral("cardChoicesList"));
    //: For screen readers. In Polish: Wartości listy
    m_list->setAccessibleName(tr("List values"));
    m_list->setEditTriggers(QAbstractItemView::DoubleClicked |
                            QAbstractItemView::EditKeyPressed);
    m_list->setMinimumHeight(m_list->sizeHintForRow(0) > 0
                                 ? m_list->sizeHintForRow(0) * CHOICE_ROWS
                                 : fontMetrics().height() * CHOICE_ROWS);
    for (const core::CardFieldChoice& choice : choices) {
        auto* item = new QListWidgetItem(choice.name.text(programLanguage()), m_list);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(CHOICE_ROLE, choice.id);
    }
    m_list->installEventFilter(this);

    const auto button = [this](const QString& text, const char* name, const char* iconId) {
        auto* each = new QPushButton(text, this);
        each->setObjectName(QString::fromLatin1(name));
        each->setAutoDefault(false);
        card::keepIcon(each, QString::fromLatin1(iconId), core::IconContext::Toolbar);
        return each;
    };
    //: Adds a value to the list. In Polish: &Dodaj
    m_addButton = button(tr("&Add"), "cardChoicesAdd", "common.add");
    //: In Polish: &Usuń
    m_removeButton = button(tr("&Remove"), "cardChoicesRemove", "common.remove");
    //: In Polish: W &górę
    m_upButton = button(tr("Move &Up"), "cardChoicesUp", "common.arrowUpward");
    //: In Polish: W &dół
    m_downButton = button(tr("Move &Down"), "cardChoicesDown", "common.arrowDownward");
    connect(m_addButton, &QPushButton::clicked, this, &CardChoicesDialog::addChoice);
    connect(m_removeButton, &QPushButton::clicked, this, &CardChoicesDialog::removeChoice);
    connect(m_upButton, &QPushButton::clicked, this, [this]() { moveBy(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { moveBy(1); });
    auto* buttons = new QVBoxLayout();
    for (QPushButton* each : {m_addButton, m_removeButton, m_upButton, m_downButton}) {
        buttons->addWidget(each);
    }
    buttons->addStretch(1);
    auto* row = new QHBoxLayout();
    row->addWidget(m_list, 1);
    row->addLayout(buttons);
    contentLayout()->addLayout(row, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, &CardChoicesDialog::updateButtons);
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    updateButtons();
    m_list->setFocus(Qt::OtherFocusReason);
}

QList<core::CardFieldChoice> CardChoicesDialog::choices() const {
    QList<core::CardFieldChoice> choices;
    for (int row = 0; row < m_list->count(); ++row) {
        const QListWidgetItem* item = m_list->item(row);
        const QString name = item->text().simplified();
        if (!name.isEmpty()) {
            choices << core::CardFieldChoice{item->data(CHOICE_ROLE).toString(),
                                             core::LocalizedText::plain(name)};
        }
    }
    return choices;
}

void CardChoicesDialog::addChoice() {
    const QString id = core::card_fields::newChoiceId(m_all);
    m_all << core::CardFieldChoice{id, {}};
    auto* item = new QListWidgetItem(m_list);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setData(CHOICE_ROLE, id);
    m_list->setCurrentItem(item);
    m_list->editItem(item);
}

void CardChoicesDialog::removeChoice() {
    const int row = m_list->currentRow();
    if (row >= 0) {
        delete m_list->takeItem(row);
    }
    updateButtons();
}

void CardChoicesDialog::moveBy(int by) {
    const int row = m_list->currentRow();
    const int to = row + by;
    if (row < 0 || to < 0 || to >= m_list->count()) {
        return;
    }
    QListWidgetItem* item = m_list->takeItem(row);
    m_list->insertItem(to, item);
    m_list->setCurrentRow(to);
}

void CardChoicesDialog::updateButtons() {
    const int row = m_list->currentRow();
    m_removeButton->setEnabled(row >= 0);
    m_upButton->setEnabled(row > 0);
    m_downButton->setEnabled(row >= 0 && row + 1 < m_list->count());
}

bool CardChoicesDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list && !card::isEditing(m_list)) {
        if (isKey(event, Qt::Key_Insert)) {
            addChoice();
            return true;
        }
        if (isKey(event, Qt::Key_Delete)) {
            removeChoice();
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

}  // namespace kalahari::gui::dialogs
