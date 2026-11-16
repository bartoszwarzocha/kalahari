// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_fields_dialog.cpp
/// @brief Card Fields: the fields of the cards of characters, locations and items, in the
/// order of the card and where each is shown

#include "kalahari/gui/dialogs/card_fields_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// The field of a row of the table: its id; none for the name and In Short
constexpr int FIELD_ROLE = Qt::UserRole;
/// Whether the row is a field of the writer
constexpr int OWN_ROLE = Qt::UserRole + 1;
/// The rows the table shows without scrolling, and at least
constexpr int TABLE_ROWS = 7;
constexpr int TABLE_MIN_ROWS = 4;
/// Room above and below the text of a row of the table
constexpr int ROW_PADDING = 8;
/// The width of the column of the kinds of the fields, in average characters
constexpr int TYPE_CHARS = 24;

/// The columns with a box to tick
bool isCheckColumn(int column) {
    return column == CardFieldsDialog::CARD_COLUMN ||
           column == CardFieldsDialog::PROPERTIES_COLUMN ||
           column == CardFieldsDialog::LIST_COLUMN;
}

/// The cells of the table: the boxes in the middle of their columns, "· writer's" in grey
/// after the name of a field of the writer
class FieldsDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (isCheckColumn(index.column())) {
            paintCheck(painter, option, index);
            return;
        }
        if (index.column() != CardFieldsDialog::NAME_COLUMN ||
            !index.data(OWN_ROLE).toBool()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString name = opt.text;
        opt.text.clear();
        const QWidget* widget = opt.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget)
                               .adjusted(TEXT_MARGIN, 0, -TEXT_MARGIN, 0);
        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group =
            opt.state.testFlag(QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
        const QColor color = opt.palette.color(
            group, selected ? QPalette::HighlightedText : QPalette::Text);
        const QString shownName =
            opt.fontMetrics.elidedText(name, Qt::ElideRight, text.width());
        painter->save();
        painter->setPen(color);
        painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeading, shownName);
        const int nameWidth = opt.fontMetrics.horizontalAdvance(shownName);
        QRect rest = text;
        rest.setLeft(text.left() + nameWidth);
        painter->setPen(selected ? color : opt.palette.color(group, QPalette::PlaceholderText));
        painter->drawText(rest, Qt::AlignVCenter | Qt::AlignLeading,
                          opt.fontMetrics.elidedText(suffix(), Qt::ElideRight, rest.width()));
        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        if (index.column() == CardFieldsDialog::NAME_COLUMN && index.data(OWN_ROLE).toBool()) {
            hint.rwidth() += option.fontMetrics.horizontalAdvance(suffix());
        }
        return hint;
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        const bool click = event->type() == QEvent::MouseButtonRelease ||
                           event->type() == QEvent::MouseButtonDblClick;
        if (!click || !isCheckColumn(index.column())) {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }
        const Qt::ItemFlags flags = model->flags(index);
        const auto* mouse = static_cast<const QMouseEvent*>(event);
        if (!flags.testFlag(Qt::ItemIsUserCheckable) || !flags.testFlag(Qt::ItemIsEnabled) ||
            mouse->button() != Qt::LeftButton ||
            !checkRect(option, index).contains(mouse->position().toPoint())) {
            return false;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            model->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
        }
        return true;
    }

private:
    /// Room between the text of a cell and its edges, as the style leaves it
    static constexpr int TEXT_MARGIN = 3;

    /// What follows the name of a field of the writer
    static QString suffix() {
        //: After the name of a field the writer added, in the window Card Fields. In Polish:
        //:  · autora
        return CardFieldsDialog::tr(" · writer's");
    }

    /// The box of a cell, in its middle
    [[nodiscard]] QRect checkRect(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QWidget* widget = opt.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        const QSize size =
            style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, widget).size();
        return QStyle::alignedRect(opt.direction, Qt::AlignCenter, size, option.rect);
    }

    void paintCheck(QPainter* painter, const QStyleOptionViewItem& option,
                    const QModelIndex& index) const {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QWidget* widget = opt.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        QStyleOptionViewItem cell(opt);
        cell.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        cell.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, widget);
        QStyleOptionViewItem check(opt);
        check.rect = checkRect(option, index);
        check.state &= ~QStyle::State_HasFocus;
        check.state |=
            opt.checkState == Qt::Checked ? QStyle::State_On : QStyle::State_Off;
        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, widget);
    }
};

/// The table of the fields: as high as some rows, as wide as its columns
class FieldTable : public QTableWidget {
public:
    using QTableWidget::QTableWidget;

    [[nodiscard]] QSize sizeHint() const override {
        int width = 2 * frameWidth();
        for (int column = 0; column < columnCount(); ++column) {
            width += std::max(sizeHintForColumn(column),
                              horizontalHeader()->sectionSizeHint(column));
        }
        width = std::max(width, fontMetrics().averageCharWidth() * TYPE_CHARS * 2);
        return {width, rowsHeight(TABLE_ROWS)};
    }

    [[nodiscard]] QSize minimumSizeHint() const override {
        return {QTableWidget::minimumSizeHint().width(), rowsHeight(TABLE_MIN_ROWS)};
    }

private:
    [[nodiscard]] int rowsHeight(int rows) const {
        return (rows * verticalHeader()->defaultSectionSize()) +
               horizontalHeader()->sizeHint().height() + (2 * frameWidth());
    }
};

/// The kinds of cards of the Workshop of the book open in @p book, in the order of its type
QList<core::KindRef> bookCardKinds(const core::ProjectManager& book) {
    QList<core::KindRef> kinds;
    if (const core::BookProject* project = book.project()) {
        for (const core::KindRef& kind :
             project->kindsIn(book.bookTypes(), core::BookPlace::Workshop)) {
            if (kind && kind.kind->isCard()) {
                kinds << kind;
            }
        }
    }
    return kinds;
}

/// The kinds of cards of the installed book types, each once: in the order of the Workshops
/// of the types, then those no type offers
QList<core::KindRef> installedCardKinds() {
    const core::BookTypeRegistry& registry = core::ProjectManager::installedBookTypes();
    QList<core::KindRef> all;
    for (const core::BookTypePackage* type : registry.bookTypes()) {
        all << registry.kindsIn(type->id, core::BookPlace::Workshop);
    }
    all << registry.allKinds();
    QList<core::KindRef> kinds;
    QStringList listed;
    for (const core::KindRef& kind : std::as_const(all)) {
        if (kind && kind.kind->isCard() && !listed.contains(kind.reference())) {
            kinds << kind;
            listed << kind.reference();
        }
    }
    return kinds;
}

/// The kind of cards @p field names, of the package of @p kind; none for another field
core::KindRef namedKind(const core::KindRef& kind, const QString& cardKind) {
    const core::ElementKind* named =
        kind.package != nullptr ? kind.package->ownKind(cardKind) : nullptr;
    return named != nullptr ? core::KindRef{kind.package, named} : core::KindRef{};
}

/// Whether @p event is key @p key without modifiers
bool isKey(const QEvent* event, int key) {
    return event->type() == QEvent::KeyPress &&
           card::isListKey(static_cast<const QKeyEvent*>(event), key);
}

}  // namespace

CardFieldsDialog::CardFieldsDialog(const core::ProjectManager& book, const QString& kind,
                                   QWidget* parent)
    : KalahariDialog(parent),
      m_readOnly(book.isReadOnly()) {
    QList<KindFields> kinds;
    for (const core::KindRef& each : bookCardKinds(book)) {
        kinds << KindFields{each, book.cardFieldSet(each), book.cardValueIds(each)};
    }
    const core::BookProject* project = book.project();
    createContent(true, project != nullptr && !project->books.isEmpty()
                            ? project->books.first().title
                            : QString());
    fillKinds(kinds, kind);
}

CardFieldsDialog::CardFieldsDialog(const core::CardFieldSets& defaults, QWidget* parent)
    : KalahariDialog(parent) {
    QList<KindFields> kinds;
    for (const core::KindRef& each : installedCardKinds()) {
        kinds << KindFields{
            each, core::card_fields::complete(*each.kind, defaults.value(each.reference())), {}};
    }
    createContent(false, QString());
    fillKinds(kinds, QString());
}

void CardFieldsDialog::createContent(bool forBook, const QString& bookTitle) {
    setObjectName(QStringLiteral("cardFieldsDialog"));
    setHeadingIcon(QStringLiteral("card.fields"));
    setCompactHeading(true);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->setObjectName(QStringLiteral("cardFieldsKind"));
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    //: The kind of the cards whose fields the window Card Fields shows. In Polish: &Rodzaj
    //: kart:
    auto* kindLabel = new QLabel(tr("&Kind of cards:"), this);
    kindLabel->setBuddy(m_kindCombo);
    form->addRow(kindLabel, m_kindCombo);
    contentLayout()->addLayout(form);

    m_table = new FieldTable(0, LIST_COLUMN + 1, this);
    m_table->setObjectName(QStringLiteral("cardFieldsTable"));
    //: The table of the fields of the cards, for screen readers. In Polish: Pola kart
    m_table->setAccessibleName(tr("Fields of the cards"));
    m_table->setHorizontalHeaderLabels({
        //: The column of the names of the fields. In Polish: Pole
        tr("Field"),
        //: The column of what each field holds: a text, a list, a card. In Polish: Rodzaj pola
        tr("Kind of Field"),
        //: The column: the field is shown on the full card. In Polish: Na karcie
        tr("On the Card"),
        //: The column: the field is shown on the short card of the Properties panel. In
        //: Polish: We Właściwościach
        tr("In Properties"),
        //: The column: the field is a column of the list of cards. In Polish: Na liście
        tr("In the List")});
    m_table->verticalHeader()->hide();
    // Rows as low as their text, which the boxes fit in
    m_table->verticalHeader()->setDefaultSectionSize(
        std::max(fontMetrics().height(),
                 style()->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, m_table)) +
        ROW_PADDING);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setTabKeyNavigation(false);
    m_table->setWordWrap(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setItemDelegate(new FieldsDelegate(m_table));
    QHeaderView* header = m_table->horizontalHeader();
    header->setSectionResizeMode(NAME_COLUMN, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TYPE_COLUMN, QHeaderView::Stretch);
    for (const int column : {CARD_COLUMN, PROPERTIES_COLUMN, LIST_COLUMN}) {
        header->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    m_table->installEventFilter(this);
    contentLayout()->addWidget(m_table, 1);
    createButtons();

    createNotes(forBook, bookTitle);
    acceptButton()->setEnabled(!m_readOnly);

    m_menu = new QMenu(this);
    m_menu->setObjectName(QStringLiteral("cardFieldsMenu"));
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            &CardFieldsDialog::showMenu);
    connect(m_kindCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_kindIndex = index;
        updateHeading();
        fillTable();
    });
    connect(m_table, &QTableWidget::cellChanged, this, &CardFieldsDialog::takeCheck);
    connect(m_table, &QTableWidget::currentCellChanged, this, &CardFieldsDialog::updateButtons);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (row >= FIXED_ROWS && !isCheckColumn(column)) {
            changeField();
        }
    });
}

void CardFieldsDialog::createNotes(bool forBook, const QString& bookTitle) {
    const auto grey = [this](const QString& text, const char* name) {
        auto* note = new QLabel(text, this);
        note->setObjectName(QString::fromLatin1(name));
        note->setWordWrap(true);
        note->setTextFormat(Qt::PlainText);
        note->setForegroundRole(QPalette::PlaceholderText);
        return note;
    };
    auto* row = new QHBoxLayout();
    if (!forBook) {
        //: Under the table of the fields of new books (Settings > Cards). In Polish: Pola, od
        //: których zaczyna nowa książka; książki, które już są, mają swoje.
        row->addWidget(grey(tr("The fields a new book starts with; the books there are "
                               "already keep theirs."),
                            "cardFieldsNote"),
                       1);
        contentLayout()->addLayout(row);
        return;
    }
    //: The fields of this book become those that new books start with. In Polish: Zapisz jako
    //: domyślne dla &nowych książek
    m_defaultsCheck = new QCheckBox(tr("Save as defaults for &new books"), this);
    m_defaultsCheck->setObjectName(QStringLiteral("cardFieldsDefaults"));
    m_defaultsCheck->setEnabled(!m_readOnly);
    row->addWidget(m_defaultsCheck);
    //: Beside Save as defaults for new books; %1 is the title of the book. In Polish: Pola tej
    //: książki – „%1”
    row->addWidget(grey(tr("The fields of this book – \"%1\"").arg(bookTitle),
                        "cardFieldsNote"),
                   1);
    contentLayout()->addLayout(row);
    //: Beside the buttons of the window Card Fields. In Polish: Własne pola jednej karty: menu
    //: pola > „Dodaj do wszystkich kart tego rodzaju”
    addSideWidget(grey(tr("Own fields of one card: the menu of the field > \"Add to All Cards "
                          "of This Kind\""),
                       "cardFieldsOwnNote"));
}

void CardFieldsDialog::createButtons() {
    const auto button = [this](const QString& text, const char* name, const char* iconId) {
        auto* each = new QPushButton(text, this);
        each->setObjectName(QString::fromLatin1(name));
        each->setAutoDefault(false);
        if (iconId != nullptr) {
            card::keepIcon(each, QString::fromLatin1(iconId), core::IconContext::Toolbar);
        }
        return each;
    };
    //: In Polish: Dodaj &pole...
    m_addButton = button(tr("Add &Field..."), "cardFieldsAdd", "common.add");
    //: Takes a field the writer added out of the cards; they keep its values. In Polish: &Usuń
    m_removeButton = button(tr("&Remove"), "cardFieldsRemove", "common.remove");
    //: In Polish: W &górę
    m_upButton = button(tr("Move &Up"), "cardFieldsUp", "common.arrowUpward");
    //: In Polish: W &dół
    m_downButton = button(tr("Move &Down"), "cardFieldsDown", "common.arrowDownward");
    //: The values of the list of a field the writer added. In Polish: &Wartości listy...
    m_choicesButton = button(tr("List &Values..."), "cardFieldsChoices", nullptr);
    //: Back to the fields of the book type, where it shows them; the cards keep the values.
    //: In Polish: Przywróć pola &typu
    m_restoreButton = button(tr("Restore &Type Fields"), "cardFieldsRestore", nullptr);
    connect(m_addButton, &QPushButton::clicked, this, &CardFieldsDialog::addField);
    connect(m_removeButton, &QPushButton::clicked, this, &CardFieldsDialog::removeField);
    connect(m_upButton, &QPushButton::clicked, this, &CardFieldsDialog::moveUp);
    connect(m_downButton, &QPushButton::clicked, this, &CardFieldsDialog::moveDown);
    connect(m_choicesButton, &QPushButton::clicked, this, &CardFieldsDialog::editChoices);
    connect(m_restoreButton, &QPushButton::clicked, this, &CardFieldsDialog::restoreTypeFields);
    auto* row = new QHBoxLayout();
    for (QPushButton* each : {m_addButton, m_removeButton, m_upButton, m_downButton,
                              m_choicesButton, m_restoreButton}) {
        row->addWidget(each);
    }
    row->addStretch(1);
    contentLayout()->addLayout(row);
}

void CardFieldsDialog::fillKinds(const QList<KindFields>& kinds, const QString& current) {
    m_kinds = kinds;
    const QSignalBlocker blocker(m_kindCombo);
    m_kindCombo->clear();
    int shown = 0;
    for (qsizetype i = 0; i < m_kinds.size(); ++i) {
        const core::KindRef& kind = m_kinds.at(i).kind;
        m_kindCombo->addItem(
            core::ArtProvider::getInstance().getIcon(kind.kind->icon, core::IconContext::Menu),
            kind.kind->name.text(programLanguage()), kind.reference());
        if (kind.reference() == current) {
            shown = static_cast<int>(i);
        }
    }
    m_kindIndex = m_kinds.isEmpty() ? -1 : shown;
    m_kindCombo->setCurrentIndex(m_kindIndex);
    updateHeading();
    fillTable();
}

QList<core::KindRef> CardFieldsDialog::kinds() const {
    QList<core::KindRef> kinds;
    for (const KindFields& each : m_kinds) {
        kinds << each.kind;
    }
    return kinds;
}

QList<core::BookCardField> CardFieldsDialog::fieldsOf(const QString& kind) const {
    const auto found = std::find_if(m_kinds.cbegin(), m_kinds.cend(),
                                    [&kind](const KindFields& each) {
                                        return each.kind.reference() == kind;
                                    });
    return found != m_kinds.cend() ? found->fields : QList<core::BookCardField>();
}

bool CardFieldsDialog::savesAsDefaults() const {
    return m_defaultsCheck != nullptr && m_defaultsCheck->isChecked();
}

void CardFieldsDialog::showKind(const QString& kind) {
    const int index = m_kindCombo->findData(kind);
    if (index >= 0) {
        m_kindCombo->setCurrentIndex(index);
    }
}

void CardFieldsDialog::setCurrentField(const QString& fieldId) {
    for (int row = FIXED_ROWS; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, NAME_COLUMN)->data(FIELD_ROLE).toString() == fieldId) {
            m_table->setCurrentCell(row, NAME_COLUMN);
            return;
        }
    }
}

CardFieldsDialog::KindFields* CardFieldsDialog::current() {
    return m_kindIndex >= 0 && m_kindIndex < m_kinds.size() ? &m_kinds[m_kindIndex] : nullptr;
}

const CardFieldsDialog::KindFields* CardFieldsDialog::current() const {
    return m_kindIndex >= 0 && m_kindIndex < m_kinds.size() ? &m_kinds.at(m_kindIndex)
                                                            : nullptr;
}

int CardFieldsDialog::currentIndex() const {
    const KindFields* kind = current();
    const int index = m_table->currentRow() - FIXED_ROWS;
    return kind != nullptr && index >= 0 && index < kind->fields.size() ? index : -1;
}

void CardFieldsDialog::updateHeading() {
    const KindFields* kind = current();
    if (m_defaultsCheck == nullptr) {
        //: The window Card Fields opened in Settings > Cards. In Polish: Domyślne pola nowych
        //: książek
        setHeading(tr("Default Fields of New Books"),
                   //: In Polish: Zaznacz, gdzie widać każde pole. Kolejność na liście to
                   //: kolejność na karcie.
                   tr("Tick where each field is shown. The order of the list is the order of "
                      "the card."));
        return;
    }
    const core::KindWords words = kind != nullptr ? wordsOf(kind->kind) : core::KindWords{};
    //: The window of the fields of the cards of a kind: {kind:plural} is the kind, such as
    //: "postacie". In Polish: Pola kart: {kind:plural}
    setHeading(core::fillWords(tr("Card Fields: {kind:plural}"),
                               {{QStringLiteral("kind"), words}}),
               tr("Tick where each field is shown. The order of the list is the order of the "
                  "card."));
}

QString CardFieldsDialog::nameOf(const core::BookCardField& field) const {
    if (field.isOwn()) {
        return field.name;
    }
    const KindFields* kind = current();
    const core::CardField* ofKind = kind != nullptr ? kind->kind.kind->field(field.id) : nullptr;
    return ofKind != nullptr ? ofKind->name.text(programLanguage()) : field.id;
}

QString CardFieldsDialog::typeText(const core::BookCardField& field) const {
    const KindFields* kind = current();
    if (kind == nullptr) {
        return {};
    }
    const core::CardField shown = core::card_fields::effective(*kind->kind.kind, {field}).first();
    switch (shown.type) {
    case core::CardFieldType::LongText:
        //: A field that holds a text of several lines. In Polish: dłuższy tekst
        return tr("longer text");
    case core::CardFieldType::Choice: {
        QStringList names;
        for (const core::CardFieldChoice& choice : shown.choices) {
            names << choice.name.text(programLanguage());
        }
        //: A field with a list of values to choose from; %1 the values. In Polish: lista: %1
        return names.isEmpty() ? tr("list without values")
                               : tr("list: %1").arg(names.join(QStringLiteral(", ")));
    }
    case core::CardFieldType::Card:
        //: A field that names a card; {kind} is its kind, such as "miejsce". In Polish:
        //: karta: {kind}
        return core::fillWords(
            tr("card: {kind}"),
            {{QStringLiteral("kind"), wordsOf(namedKind(kind->kind, shown.cardKind))}});
    case core::CardFieldType::Text:
        break;
    }
    //: A field that holds a line of text. In Polish: tekst
    return tr("text");
}

void CardFieldsDialog::fillTable() {
    m_filling = true;
    const QSignalBlocker blocker(m_table);
    const KindFields* kind = current();
    const int currentRow = m_table->currentRow();
    m_table->setRowCount(kind != nullptr ? FIXED_ROWS + static_cast<int>(kind->fields.size())
                                         : 0);
    if (kind != nullptr) {
        // The name and In Short: every card has them, everywhere
        //: The name of a card, a field that every card has. In Polish: Nazwa
        const QStringList fixed{tr("Name"),
                                //: In Polish: W skrócie
                                tr("In Short")};
        for (int row = 0; row < FIXED_ROWS; ++row) {
            auto* name = new QTableWidgetItem(fixed.at(row));
            name->setFlags(Qt::NoItemFlags);
            m_table->setItem(row, NAME_COLUMN, name);
            //: The kind of the name and In Short, which every card has. In Polish: stałe
            auto* type = new QTableWidgetItem(tr("always there"));
            type->setFlags(Qt::NoItemFlags);
            m_table->setItem(row, TYPE_COLUMN, type);
            for (const int column : {CARD_COLUMN, PROPERTIES_COLUMN, LIST_COLUMN}) {
                auto* check = new QTableWidgetItem();
                check->setFlags(Qt::ItemIsUserCheckable);
                check->setCheckState(Qt::Checked);
                m_table->setItem(row, column, check);
            }
        }
        for (qsizetype i = 0; i < kind->fields.size(); ++i) {
            fillRow(FIXED_ROWS + static_cast<int>(i), kind->fields.at(i));
        }
    }
    m_filling = false;
    const int row =
        std::clamp(currentRow, FIXED_ROWS, std::max(FIXED_ROWS, m_table->rowCount() - 1));
    if (row < m_table->rowCount()) {
        m_table->setCurrentCell(row, NAME_COLUMN);
    }
    updateButtons();
}

void CardFieldsDialog::fillRow(int row, const core::BookCardField& field) {
    const Qt::ItemFlags enabled = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    const QPalette::ColorRole color =
        field.onCard ? QPalette::Text : QPalette::PlaceholderText;
    auto* name = new QTableWidgetItem(nameOf(field));
    name->setFlags(enabled);
    name->setData(FIELD_ROLE, field.id);
    name->setData(OWN_ROLE, field.isOwn());
    name->setForeground(palette().brush(color));
    if (field.isOwn()) {
        //: The tooltip of a field the writer added. In Polish: Pole autora; F2 albo dwuklik je
        //: zmienia
        name->setToolTip(tr("A field of the writer; F2 or a double-click changes it"));
        //: For screen readers: the name of a field the writer added. In Polish: %1, pole autora
        name->setData(Qt::AccessibleTextRole, tr("%1, a field of the writer").arg(field.name));
    }
    m_table->setItem(row, NAME_COLUMN, name);
    auto* type = new QTableWidgetItem(typeText(field));
    type->setFlags(enabled);
    type->setToolTip(type->text());
    type->setForeground(palette().brush(QPalette::PlaceholderText));
    m_table->setItem(row, TYPE_COLUMN, type);
    const Qt::ItemFlags checkable =
        m_readOnly ? enabled : enabled | Qt::ItemIsUserCheckable;
    const QList<std::pair<int, bool>> checks{{CARD_COLUMN, field.onCard},
                                             {PROPERTIES_COLUMN, field.inProperties},
                                             {LIST_COLUMN, field.inList}};
    for (const auto& [column, on] : checks) {
        auto* check = new QTableWidgetItem();
        check->setFlags(checkable);
        check->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        check->setData(Qt::AccessibleTextRole,
                       m_table->horizontalHeaderItem(column)->text());
        m_table->setItem(row, column, check);
    }
}

void CardFieldsDialog::takeCheck(int row, int column) {
    KindFields* kind = current();
    const int index = row - FIXED_ROWS;
    if (m_filling || kind == nullptr || !isCheckColumn(column) || index < 0 ||
        index >= kind->fields.size()) {
        return;
    }
    core::BookCardField& field = kind->fields[index];
    const bool on = m_table->item(row, column)->checkState() == Qt::Checked;
    if (column == CARD_COLUMN) {
        field.onCard = on;
    } else if (column == PROPERTIES_COLUMN) {
        field.inProperties = on;
    } else {
        field.inList = on;
    }
    // A field that is not on the card is grey
    const QSignalBlocker blocker(m_table);
    m_table->item(row, NAME_COLUMN)
        ->setForeground(palette().brush(field.onCard ? QPalette::Text : QPalette::PlaceholderText));
    updateButtons();
}

void CardFieldsDialog::updateButtons() {
    const KindFields* kind = current();
    const int index = currentIndex();
    const core::BookCardField* field =
        index >= 0 && kind != nullptr ? &kind->fields.at(index) : nullptr;
    const bool own = field != nullptr && field->isOwn();
    const bool can = !m_readOnly && kind != nullptr;
    m_addButton->setEnabled(can);
    m_removeButton->setEnabled(can && own);
    m_upButton->setEnabled(can && index > 0);
    m_downButton->setEnabled(can && index >= 0 && index + 1 < kind->fields.size());
    m_choicesButton->setEnabled(can && own && field->type == core::CardFieldType::Choice);
    m_restoreButton->setEnabled(can && !core::card_fields::isKindSet(*kind->kind.kind,
                                                                     kind->fields));
}

QStringList CardFieldsDialog::namesBut(int exceptIndex) const {
    QStringList names{tr("Name"), tr("In Short")};
    const KindFields* kind = current();
    if (kind == nullptr) {
        return names;
    }
    for (qsizetype i = 0; i < kind->fields.size(); ++i) {
        const core::BookCardField& field = kind->fields.at(i);
        if (i == exceptIndex) {
            continue;
        }
        if (field.isOwn()) {
            names << field.name;
        } else if (const core::CardField* ofKind = kind->kind.kind->field(field.id)) {
            names << ofKind->name.values.values();
        }
    }
    return names;
}

void CardFieldsDialog::addField() {
    KindFields* kind = current();
    if (kind == nullptr || m_readOnly) {
        return;
    }
    core::BookCardField field;
    field.onCard = true;
    CardFieldDialog dialog(kind->kind, field, namesBut(-1), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    field = dialog.field();
    field.id = core::card_fields::newOwnId(kind->fields, kind->taken);
    // Its id is not taken again in the window, also when it goes
    kind->taken << field.id;
    const int index = currentIndex();
    const int at = index >= 0 ? index + 1 : static_cast<int>(kind->fields.size());
    kind->fields.insert(at, field);
    fillTable();
    setCurrentField(field.id);
    // A list needs its values
    if (field.type == core::CardFieldType::Choice) {
        editChoices();
    }
}

void CardFieldsDialog::changeField() {
    KindFields* kind = current();
    const int index = currentIndex();
    if (kind == nullptr || index < 0 || m_readOnly || !kind->fields.at(index).isOwn()) {
        return;
    }
    CardFieldDialog dialog(kind->kind, kind->fields.at(index), namesBut(index), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    kind->fields[index] = dialog.field();
    fillTable();
    setCurrentField(kind->fields.at(index).id);
}

void CardFieldsDialog::removeField() {
    KindFields* kind = current();
    const int index = currentIndex();
    if (kind == nullptr || index < 0 || m_readOnly || !kind->fields.at(index).isOwn()) {
        return;
    }
    kind->fields.removeAt(index);
    fillTable();
}

void CardFieldsDialog::moveUp() {
    moveBy(-1);
}

void CardFieldsDialog::moveDown() {
    moveBy(1);
}

void CardFieldsDialog::moveBy(int by) {
    KindFields* kind = current();
    const int index = currentIndex();
    const int to = index + by;
    if (kind == nullptr || index < 0 || to < 0 || to >= kind->fields.size() || m_readOnly) {
        return;
    }
    kind->fields.move(index, to);
    const int column = std::max(m_table->currentColumn(), 0);
    fillTable();
    m_table->setCurrentCell(FIXED_ROWS + to, column);
}

void CardFieldsDialog::editChoices() {
    KindFields* kind = current();
    const int index = currentIndex();
    if (kind == nullptr || index < 0 || m_readOnly) {
        return;
    }
    core::BookCardField& field = kind->fields[index];
    if (!field.isOwn() || field.type != core::CardFieldType::Choice) {
        return;
    }
    CardChoicesDialog dialog(field.name, field.choices, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    field.choices = dialog.choices();
    const QString id = field.id;
    fillTable();
    setCurrentField(id);
}

void CardFieldsDialog::restoreTypeFields() {
    KindFields* kind = current();
    if (kind == nullptr || m_readOnly) {
        return;
    }
    kind->fields = core::card_fields::ofKind(*kind->kind.kind);
    fillTable();
}

void CardFieldsDialog::showMenu(const QPoint& pos) {
    if (const QTableWidgetItem* item = m_table->itemAt(pos)) {
        m_table->setCurrentCell(item->row(), m_table->currentColumn() >= 0
                                                 ? m_table->currentColumn()
                                                 : NAME_COLUMN);
    }
    const KindFields* kind = current();
    const int index = currentIndex();
    const bool own = index >= 0 && kind != nullptr && kind->fields.at(index).isOwn();
    m_menu->clear();
    QAction* change = m_menu->addAction(
        //: The menu of the table of the window Card Fields: changes a field the writer added.
        //: In Polish: &Zmień pole...
        tr("&Change Field..."), this, &CardFieldsDialog::changeField);
    change->setEnabled(own && !m_readOnly);
    for (QPushButton* button : {m_addButton, m_removeButton, m_upButton, m_downButton,
                                m_choicesButton}) {
        QAction* action = m_menu->addAction(button->icon(), button->text(), button,
                                            &QPushButton::click);
        action->setEnabled(button->isEnabled());
    }
    m_menu->popup(m_table->viewport()->mapToGlobal(pos));
}

bool CardFieldsDialog::eventFilter(QObject* watched, QEvent* event) {
    // The keys of the table, before the commands of the window
    if (watched == m_table && event->type() == QEvent::ShortcutOverride) {
        const auto* key = static_cast<const QKeyEvent*>(event);
        if (card::isListKey(key, Qt::Key_Insert) || card::isListKey(key, Qt::Key_Delete) ||
            card::isListKey(key, Qt::Key_F2)) {
            event->accept();
            return true;
        }
    }
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        if (isKey(event, Qt::Key_Insert)) {
            addField();
            return true;
        }
        if (isKey(event, Qt::Key_Delete)) {
            removeField();
            return true;
        }
        if (isKey(event, Qt::Key_F2)) {
            changeField();
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

core::CardFieldSets defaultsOf(const QList<core::KindRef>& kinds, const core::CardFieldSets& sets,
                               const core::CardFieldSets& defaults) {
    core::CardFieldSets result = defaults;
    for (const core::KindRef& kind : kinds) {
        const QString reference = kind.reference();
        if (!sets.contains(reference)) {
            continue;
        }
        const QList<core::BookCardField> fields = sets.value(reference);
        const bool ofPackage = core::card_fields::isKindSet(*kind.kind, fields);
        QList<core::BookCardField> set = core::card_fields::keepUnknown(
            *kind.kind, defaults.value(reference),
            ofPackage ? core::card_fields::ofKind(*kind.kind) : fields);
        if (ofPackage && set.size() == kind.kind->fields.size()) {
            result.remove(reference);
            continue;
        }
        // The relation of a book is no relation of the next one
        for (core::BookCardField& field : set) {
            field.relation.clear();
        }
        result.insert(reference, set);
    }
    return result;
}

bool editCardFields(core::ProjectManager& book, const QString& kind, QWidget* parent) {
    CardFieldsDialog dialog(book, kind, parent);
    if (dialog.exec() != QDialog::Accepted || book.isReadOnly()) {
        return false;
    }
    bool failed = false;
    bool saved = false;
    core::CardFieldSets sets;
    for (const core::KindRef& each : dialog.kinds()) {
        const QList<core::BookCardField> fields = dialog.fieldsOf(each.reference());
        sets.insert(each.reference(), fields);
        if (fields == book.cardFieldSet(each)) {
            continue;
        }
        if (book.setCardFields(each, fields)) {
            saved = true;
        } else {
            failed = true;
        }
    }
    if (dialog.savesAsDefaults()) {
        core::card_fields::setDefaults(
            defaultsOf(dialog.kinds(), sets, core::card_fields::defaults()));
    }
    if (failed) {
        //: In Polish: Nie zapisano pól kart
        MessageDialog::warning(parent, CardFieldsDialog::tr("Card Fields Not Saved"),
                               CardFieldsDialog::tr("Failed to save changes."));
    }
    return saved;
}

std::optional<core::CardFieldSets> editDefaultCardFields(const core::CardFieldSets& defaults,
                                                         QWidget* parent) {
    CardFieldsDialog dialog(defaults, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    core::CardFieldSets sets;
    for (const core::KindRef& each : dialog.kinds()) {
        sets.insert(each.reference(), dialog.fieldsOf(each.reference()));
    }
    return defaultsOf(dialog.kinds(), sets, defaults);
}

}  // namespace kalahari::gui::dialogs
