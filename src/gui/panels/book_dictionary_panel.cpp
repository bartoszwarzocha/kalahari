// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary_panel.cpp
/// @brief The book's dictionary on its tab in the middle of the window

#include "kalahari/gui/panels/book_dictionary_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/name_labels.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/utils/language_names.h"

#include <QCollator>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace kalahari::gui {

namespace {

using Part = BookDictionaryPanel::Part;

/// @brief The id of a row (of a name, of a rule, or the word), in the data of its first column
constexpr int ID_ROLE = Qt::UserRole;

/// @brief The margins of the page at its sides and at its top and bottom, and the space
///        between its parts
constexpr int SIDE_MARGIN = 12;
constexpr int END_MARGIN = 10;
constexpr int SPACING = 8;

/// @brief How much larger the title is than the text
constexpr qreal TITLE_SCALE = 1.2;

/// @brief Where a part is in the arrays and among the tabs
constexpr std::size_t indexOf(Part part) {
    return static_cast<std::size_t>(part);
}

/// @brief The forms a name has besides the name itself, for its row: "Anny Wolskiej, Annie
///        Wolskiej, ..."; "does not change" for one whose words all keep their form
QString formsSummary(const core::DictionaryName& name) {
    if (name.variants.isEmpty()) {
        return {};
    }
    const core::NameVariant& main = name.variants.constFirst();
    const QString unchanged = core::NameDeclension::unchanged();
    if (!main.models.isEmpty() &&
        std::ranges::all_of(main.models, [&unchanged](const QString& model) {
            return model == unchanged;
        })) {
        //: The forms of a name that has the same form in every case. In Polish: nie odmienia się
        return BookDictionaryPanel::tr("does not change");
    }
    QStringList forms = main.forms.allForms(name.plural);
    forms.removeAll(main.text);
    return forms.join(QStringLiteral(", "));
}

/// @brief Whether a rule covers other forms than its spellings, for its row: "yes", "yes (like
///        "elf")", "does not change", "no"
QString ruleFormsSummary(const core::SpellingRule& rule) {
    if (!rule.allForms) {
        //: The rule covers its spellings alone, not their forms. In Polish: nie
        return BookDictionaryPanel::tr("no");
    }
    const auto spellings = std::ranges::count_if(rule.wrong, [&rule](const QString& wrong) {
        return !wrong.isEmpty() && wrong != rule.right;
    });
    if (rule.forms.size() <= spellings) {
        //: The word of the rule has no other forms. In Polish: nie odmienia się
        return BookDictionaryPanel::tr("does not change");
    }
    if (!rule.pattern.isEmpty()) {
        //: The rule has the forms of another word; %1: the word in quotes. In Polish: tak (jak
        //: %1)
        return BookDictionaryPanel::tr("yes (like %1)").arg(QLocale().quoteString(rule.pattern));
    }
    //: In Polish: tak
    return BookDictionaryPanel::tr("yes");
}

/// @brief Whether the search finds a name: in its texts or its forms, in any case
bool nameMatches(const core::DictionaryName& name, const QString& query) {
    if (query.isEmpty()) {
        return true;
    }
    return std::ranges::any_of(name.allForms(),
                               [&query](const QString& form) {
                                   return form.contains(query, Qt::CaseInsensitive);
                               }) ||
           std::ranges::any_of(name.variants, [&query](const core::NameVariant& variant) {
               return variant.text.contains(query, Qt::CaseInsensitive);
           });
}

/// @brief Whether the search finds a rule: in its spellings, its forms or its note
bool ruleMatches(const core::SpellingRule& rule, const QString& query) {
    if (query.isEmpty()) {
        return true;
    }
    const auto found = [&query](const QString& text) {
        return text.contains(query, Qt::CaseInsensitive);
    };
    return found(rule.right) || found(rule.note) || std::ranges::any_of(rule.wrong, found) ||
           std::ranges::any_of(rule.forms.keys(), found) ||
           std::ranges::any_of(rule.forms.values(), found);
}

/// @brief A list of the page: its columns, sorted by the program, rows chosen one at a time or
///        several (the words)
QTreeWidget* newList(QWidget* parent, const QStringList& headers, bool several) {
    auto* list = new QTreeWidget(parent);
    list->setColumnCount(static_cast<int>(headers.size()));
    list->setHeaderLabels(headers);
    list->setHeaderHidden(headers.size() == 1);
    list->setRootIsDecorated(false);
    list->setUniformRowHeights(true);
    list->setAlternatingRowColors(true);
    list->setSelectionMode(several ? QAbstractItemView::ExtendedSelection
                                   : QAbstractItemView::SingleSelection);
    list->header()->setStretchLastSection(true);
    list->header()->setSectionResizeMode(QHeaderView::Interactive);
    return list;
}

/// @brief A row of a list: its id, its texts, the first one bold
QTreeWidgetItem* newRow(QTreeWidget* list, const QString& id, const QStringList& texts) {
    auto* item = new QTreeWidgetItem(list, texts);
    item->setData(0, ID_ROLE, id);
    QFont bold = item->font(0);
    bold.setBold(list->columnCount() > 1);
    item->setFont(0, bold);
    for (int column = 0; column < texts.size(); ++column) {
        item->setToolTip(column, texts.at(column));
    }
    return item;
}

/// @brief The ids of the rows chosen in a list, and the one current when none is chosen
QStringList chosenIds(const QTreeWidget* list) {
    QStringList ids;
    for (const QTreeWidgetItem* item : list->selectedItems()) {
        ids.append(item->data(0, ID_ROLE).toString());
    }
    if (ids.isEmpty() && list->currentItem() != nullptr) {
        ids.append(list->currentItem()->data(0, ID_ROLE).toString());
    }
    return ids;
}

/// @brief Choose the rows of @p ids again after a list was filled; the first row when none
void chooseAgain(QTreeWidget* list, const QStringList& ids) {
    QTreeWidgetItem* current = nullptr;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = list->topLevelItem(i);
        if (ids.contains(item->data(0, ID_ROLE).toString())) {
            item->setSelected(true);
            current = current != nullptr ? current : item;
        }
    }
    if (current == nullptr && list->topLevelItemCount() > 0) {
        current = list->topLevelItem(0);
    }
    list->setCurrentItem(current, 0, QItemSelectionModel::NoUpdate);
    if (current != nullptr && list->selectedItems().isEmpty()) {
        current->setSelected(true);
    }
}

}  // namespace

BookDictionaryPanel::BookDictionaryPanel(core::ProjectManager& book, QWidget* parent)
    : QWidget(parent), m_book(book) {
    setObjectName(QStringLiteral("bookDictionary"));
    createLayout();
    createKeys();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &BookDictionaryPanel::refreshIcons);
    refreshIcons();
    refresh();
}

void BookDictionaryPanel::createLayout() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(SIDE_MARGIN, END_MARGIN, SIDE_MARGIN, END_MARGIN);
    layout->setSpacing(SPACING);

    // The heading: the icon, the title, the book and the language of its forms, the gear
    auto* heading = new QHBoxLayout();
    m_icon = new QLabel(this);
    heading->addWidget(m_icon);
    //: The title of the page of the book's dictionary. In Polish: Słownik książki
    auto* title = new QLabel(tr("Book Dictionary"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * TITLE_SCALE);
    title->setFont(titleFont);
    heading->addWidget(title);
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("bookDictionarySubtitle"));
    m_subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    heading->addWidget(m_subtitle, 1);
    m_settingsButton = new QToolButton(this);
    m_settingsButton->setObjectName(QStringLiteral("bookDictionarySettings"));
    m_settingsButton->setAutoRaise(true);
    //: The gear of the book's dictionary. In Polish: Ustawienia pisowni: Słownik książki
    m_settingsButton->setToolTip(tr("Spelling Settings: Book Dictionary"));
    m_settingsButton->setAccessibleName(m_settingsButton->toolTip());
    heading->addWidget(m_settingsButton);
    layout->addLayout(heading);

    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("bookDictionaryTabs"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(true);
    for (int i = 0; i < PART_COUNT; ++i) {
        m_tabs->addTab(QString());
    }
    layout->addWidget(m_tabs);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("bookDictionarySearch"));
    m_search->setClearButtonEnabled(true);
    //: %1: the keys of Find. In Polish: Szukaj w słowniku, także w formach (%1)
    m_search->setPlaceholderText(
        tr("Search in the dictionary, also in the forms (%1)")
            .arg(ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_F))));
    //: In Polish: Szukaj w słowniku
    m_search->setAccessibleName(tr("Search in the dictionary"));
    layout->addWidget(m_search);

    // A list for each part; the words with the field of a new one under them
    m_stack = new QStackedWidget(this);
    //: The columns of the names. In Polish: Imię lub nazwa, Rodzaj, Formy, Inne nazwy
    m_lists.at(indexOf(Part::Names)) =
        newList(m_stack, {tr("Name"), tr("Kind"), tr("Forms"), tr("Other Names")}, false);
    //: The columns of the spelling rules. In Polish: Pisz, Zamiast, W odmianach, Uwaga
    m_lists.at(indexOf(Part::Rules)) =
        newList(m_stack, {tr("Write"), tr("Instead of"), tr("In Forms"), tr("Note")}, false);
    //: In Polish: Słowo
    m_lists.at(indexOf(Part::Words)) = newList(m_stack, {tr("Word")}, true);
    m_lists.at(indexOf(Part::Names))->setObjectName(QStringLiteral("bookDictionaryNames"));
    m_lists.at(indexOf(Part::Rules))->setObjectName(QStringLiteral("bookDictionaryRules"));
    m_lists.at(indexOf(Part::Words))->setObjectName(QStringLiteral("bookDictionaryWords"));
    m_stack->addWidget(m_lists.at(indexOf(Part::Names)));
    m_stack->addWidget(m_lists.at(indexOf(Part::Rules)));
    auto* words = new QWidget(m_stack);
    auto* wordsLayout = new QVBoxLayout(words);
    wordsLayout->setContentsMargins(0, 0, 0, 0);
    m_lists.at(indexOf(Part::Words))->setParent(words);
    wordsLayout->addWidget(m_lists.at(indexOf(Part::Words)), 1);
    auto* wordRow = new QHBoxLayout();
    m_newWord = new QLineEdit(words);
    m_newWord->setObjectName(QStringLiteral("bookDictionaryNewWord"));
    //: In Polish: Słowo do dodania
    m_newWord->setPlaceholderText(tr("A word to add"));
    m_newWord->setAccessibleName(m_newWord->placeholderText());
    //: In Polish: Dodaj
    m_addWordButton = new QPushButton(tr("Add"), words);
    wordRow->addWidget(m_newWord, 1);
    wordRow->addWidget(m_addWordButton);
    wordsLayout->addLayout(wordRow);
    m_stack->addWidget(words);
    layout->addWidget(m_stack, 1);

    auto* buttons = new QHBoxLayout();
    m_addButton = new QPushButton(this);
    m_addButton->setObjectName(QStringLiteral("bookDictionaryAdd"));
    //: In Polish: Zmień…
    m_editButton = new QPushButton(tr("Edit..."), this);
    m_editButton->setObjectName(QStringLiteral("bookDictionaryEdit"));
    //: In Polish: Usuń
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setObjectName(QStringLiteral("bookDictionaryRemove"));
    //: Moves the chosen words of the book to the writer's own words. In Polish: Przenieś do
    //: Twoich słów
    m_moveButton = new QPushButton(tr("Move to Your Words"), this);
    m_moveButton->setObjectName(QStringLiteral("bookDictionaryMove"));
    for (QPushButton* button : {m_addButton, m_editButton, m_removeButton, m_moveButton}) {
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    layout->addLayout(buttons);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("bookDictionaryNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name()));
    layout->addWidget(m_note);

    for (QTreeWidget* list : m_lists) {
        list->installEventFilter(this);
        connect(list, &QTreeWidget::itemSelectionChanged, this,
                &BookDictionaryPanel::updateButtons);
        // Without rows chosen, the current one is chosen (selectedIds())
        connect(list, &QTreeWidget::currentItemChanged, this,
                &BookDictionaryPanel::updateButtons);
        // A double click edits; Enter: eventFilter()
        connect(list, &QTreeWidget::itemActivated, this, &BookDictionaryPanel::requestEdit);
    }
    m_newWord->installEventFilter(this);
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        updateButtons();
    });
    connect(m_search, &QLineEdit::textChanged, this, &BookDictionaryPanel::refresh);
    connect(m_newWord, &QLineEdit::textChanged, this, &BookDictionaryPanel::updateButtons);
    connect(m_settingsButton, &QToolButton::clicked, this,
            &BookDictionaryPanel::settingsRequested);
    connect(m_addButton, &QPushButton::clicked, this, &BookDictionaryPanel::requestAdd);
    connect(m_editButton, &QPushButton::clicked, this, &BookDictionaryPanel::requestEdit);
    connect(m_removeButton, &QPushButton::clicked, this, &BookDictionaryPanel::requestRemove);
    connect(m_addWordButton, &QPushButton::clicked, this, &BookDictionaryPanel::requestAddWord);
    connect(m_moveButton, &QPushButton::clicked, this, [this]() {
        if (!isReadOnly() && !selectedIds().isEmpty()) {
            emit moveToUserWordsRequested(selectedIds());
        }
    });
}

void BookDictionaryPanel::createKeys() {
    // Ctrl+Tab and Ctrl+Shift+Tab go through the tabs of the dictionary, not of the window
    const auto step = [this](int by) {
        m_tabs->setCurrentIndex((m_tabs->currentIndex() + by + PART_COUNT) % PART_COUNT);
    };
    const auto shortcut = [this](QKeyCombination keys) {
        auto* made = new QShortcut(QKeySequence(keys), this);
        made->setContext(Qt::WidgetWithChildrenShortcut);
        return made;
    };
    connect(shortcut(QKeyCombination(Qt::ControlModifier, Qt::Key_Tab)), &QShortcut::activated,
            this, [step]() { step(1); });
    connect(shortcut(QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Tab)),
            &QShortcut::activated, this, [step]() { step(-1); });
    connect(shortcut(QKeyCombination(Qt::ControlModifier, Qt::Key_Backtab)),
            &QShortcut::activated, this, [step]() { step(-1); });
}

void BookDictionaryPanel::refreshIcons() {
    auto& art = core::ArtProvider::getInstance();
    const int size = art.getIconSize(core::IconContext::Panel);
    m_icon->setPixmap(art.getIcon(QStringLiteral("book.dictionary"), core::IconContext::Panel)
                          .pixmap(size, size));
    m_settingsButton->setIcon(art.getIcon(QStringLiteral("dock.settings"),
                                          core::IconContext::Menu));
}

void BookDictionaryPanel::refresh() {
    const core::BookDictionary& dictionary = m_book.bookDictionary();
    fillNames(dictionary);
    fillRules(dictionary);
    fillWords(dictionary);
    updateTabs(dictionary);

    // The book and the language of the forms of its names: a series, whose volumes share the
    // dictionary, by its name, with the language of the current volume
    const core::ProjectBook* book = m_book.isProjectOpen() ? m_book.book() : nullptr;
    QString subtitle;
    if (book != nullptr) {
        subtitle = m_book.isSeries() ? m_book.seriesName() : book->title;
    }
    if (book != nullptr && !book->language.isEmpty()) {
        //: %1: the title of the book, %2: the language of the forms of the names. In Polish:
        //: %1 · formy: %2 (język książki)
        subtitle = tr("%1 · forms: %2 (the language of the book)")
                       .arg(subtitle, utils::languageName(book->language));
    }
    m_subtitle->setText(subtitle);
    updateButtons();
}

void BookDictionaryPanel::fillNames(const core::BookDictionary& dictionary) {
    QTreeWidget* list = m_lists.at(indexOf(Part::Names));
    const QStringList chosen = chosenIds(list);
    QList<const core::DictionaryName*> names;
    for (const core::DictionaryName& name : dictionary.names()) {
        if (nameMatches(name, m_search->text().trimmed())) {
            names.append(&name);
        }
    }
    QCollator collator;
    std::ranges::stable_sort(names, [&collator](const auto* first, const auto* second) {
        return collator.compare(first->text(), second->text()) < 0;
    });
    list->clear();
    for (const core::DictionaryName* name : std::as_const(names)) {
        QStringList others;
        for (qsizetype i = 1; i < name->variants.size(); ++i) {
            others.append(name->variants.at(i).text);
        }
        newRow(list, name->id,
               {name->text(), nameKindChoice(name->kind), formsSummary(*name),
                others.join(QStringLiteral(", "))});
    }
    chooseAgain(list, chosen);
}

void BookDictionaryPanel::fillRules(const core::BookDictionary& dictionary) {
    QTreeWidget* list = m_lists.at(indexOf(Part::Rules));
    const QStringList chosen = chosenIds(list);
    QList<const core::SpellingRule*> rules;
    for (const core::SpellingRule& rule : dictionary.rules()) {
        if (ruleMatches(rule, m_search->text().trimmed())) {
            rules.append(&rule);
        }
    }
    QCollator collator;
    std::ranges::stable_sort(rules, [&collator](const auto* first, const auto* second) {
        return collator.compare(first->right, second->right) < 0;
    });
    list->clear();
    for (const core::SpellingRule* rule : std::as_const(rules)) {
        newRow(list, rule->id,
               {rule->right, rule->wrong.join(QStringLiteral(", ")), ruleFormsSummary(*rule),
                rule->note});
    }
    chooseAgain(list, chosen);
}

void BookDictionaryPanel::fillWords(const core::BookDictionary& dictionary) {
    QTreeWidget* list = m_lists.at(indexOf(Part::Words));
    const QStringList chosen = chosenIds(list);
    list->clear();
    const QString query = m_search->text().trimmed();
    for (const QString& word : dictionary.words()) {
        if (word.contains(query, Qt::CaseInsensitive)) {
            newRow(list, word, {word});
        }
    }
    chooseAgain(list, chosen);
}

void BookDictionaryPanel::updateTabs(const core::BookDictionary& dictionary) {
    //: The tabs of the book's dictionary with their counts. In Polish: Imiona i nazwy (%1)
    m_tabs->setTabText(static_cast<int>(Part::Names),
                       tr("Names (%1)").arg(dictionary.names().size()));
    //: In Polish: Zasady pisowni (%1)
    m_tabs->setTabText(static_cast<int>(Part::Rules),
                       tr("Spelling Rules (%1)").arg(dictionary.rules().size()));
    //: In Polish: Słowa książki (%1)
    m_tabs->setTabText(static_cast<int>(Part::Words),
                       tr("Book Words (%1)").arg(dictionary.words().size()));
}

void BookDictionaryPanel::updateButtons() {
    const Part shown = part();
    const bool words = shown == Part::Words;
    const bool chosen = !selectedIds().isEmpty();
    const bool readOnly = isReadOnly();
    //: In Polish: Dodaj imię lub nazwę… / Dodaj zasadę…
    m_addButton->setText(shown == Part::Names ? tr("Add Name...") : tr("Add Rule..."));
    m_addButton->setVisible(!words);
    m_editButton->setVisible(!words);
    m_moveButton->setVisible(words);
    m_addButton->setEnabled(!readOnly);
    m_editButton->setEnabled(!readOnly && chosen);
    m_removeButton->setEnabled(!readOnly && chosen);
    m_moveButton->setEnabled(!readOnly && chosen);
    m_newWord->setEnabled(!readOnly);
    m_addWordButton->setEnabled(!readOnly && !m_newWord->text().trimmed().isEmpty());
    const QString why = readOnly ? readOnlyReason() : QString();
    for (QWidget* button : std::initializer_list<QWidget*>{m_addButton, m_editButton,
                                                           m_removeButton, m_moveButton,
                                                           m_newWord, m_addWordButton}) {
        button->setToolTip(why);
    }

    static const std::array<const char*, PART_COUNT> notes = {
        QT_TR_NOOP("Names written in the dictionary: they are right in all their forms, Quick "
                   "Insert offers them and Find in Book finds them. Once there are cards of "
                   "characters, locations and items, their names join them here."),
        QT_TR_NOOP("The spelling check marks a departure from a rule with a wavy line of its "
                   "own color, and offers the rule's spelling."),
        QT_TR_NOOP("Words right in this book only. The words of every book are in Settings > "
                   "Editor > Spelling > Your Words; \"Move to Your Words\" moves the chosen "
                   "words there.")};
    m_note->setText(tr(notes.at(indexOf(shown))));
}

void BookDictionaryPanel::showPart(Part part) {
    m_tabs->setCurrentIndex(static_cast<int>(part));
}

BookDictionaryPanel::Part BookDictionaryPanel::part() const {
    return static_cast<Part>(std::clamp(m_tabs->currentIndex(), 0, PART_COUNT - 1));
}

void BookDictionaryPanel::focusSearch() {
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void BookDictionaryPanel::focusList() {
    currentList()->setFocus(Qt::OtherFocusReason);
}

QStringList BookDictionaryPanel::selectedIds() const {
    return chosenIds(currentList());
}

bool BookDictionaryPanel::selectId(const QString& id) {
    QTreeWidget* list = currentList();
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = list->topLevelItem(i);
        if (item->data(0, ID_ROLE).toString() == id) {
            list->clearSelection();
            list->setCurrentItem(item);
            item->setSelected(true);
            return true;
        }
    }
    return false;
}

QTreeWidget* BookDictionaryPanel::list(Part part) const {
    return m_lists.at(indexOf(part));
}

QTreeWidget* BookDictionaryPanel::currentList() const {
    return m_lists.at(indexOf(part()));
}

bool BookDictionaryPanel::isReadOnly() const {
    return m_book.isReadOnly();
}

bool BookDictionaryPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->modifiers() != Qt::NoModifier && key->modifiers() != Qt::KeypadModifier) {
        return QWidget::eventFilter(watched, event);
    }
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    if (watched == m_newWord && enter) {
        requestAddWord();
        return true;
    }
    if (watched != currentList()) {
        return QWidget::eventFilter(watched, event);
    }
    // Enter edits on every system: on macOS a list does not activate its row with it
    if (enter) {
        requestEdit();
        return true;
    }
    if (key->key() == Qt::Key_Insert) {
        requestAdd();
        return true;
    }
    if (key->key() == Qt::Key_Delete) {
        requestRemove();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void BookDictionaryPanel::requestAdd() {
    if (isReadOnly()) {
        return;
    }
    if (part() == Part::Words) {
        m_newWord->setFocus(Qt::ShortcutFocusReason);
        return;
    }
    emit addRequested(part());
}

void BookDictionaryPanel::requestEdit() {
    const QStringList ids = selectedIds();
    if (isReadOnly() || ids.isEmpty() || part() == Part::Words) {
        return;
    }
    emit editRequested(part(), ids.constFirst());
}

void BookDictionaryPanel::requestRemove() {
    const QStringList ids = selectedIds();
    if (isReadOnly() || ids.isEmpty()) {
        return;
    }
    emit removeRequested(part(), ids);
}

void BookDictionaryPanel::requestAddWord() {
    const QString word = m_newWord->text().trimmed();
    if (isReadOnly() || word.isEmpty()) {
        return;
    }
    emit addWordRequested(word);
    m_newWord->clear();
}

}  // namespace kalahari::gui
