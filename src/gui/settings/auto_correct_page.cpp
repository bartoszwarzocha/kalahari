/// @file auto_correct_page.cpp
/// @brief Settings page: Editor > Auto-correct
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace kalahari {
namespace gui {

using json = nlohmann::json;

namespace {

/// @brief The columns of the replacement table
enum PairColumn : int {
    ON_COLUMN = 0,           ///< The check box: the pair is made
    WORD_COLUMN = 1,         ///< The word typed
    REPLACEMENT_COLUMN = 2,  ///< What takes its place
    SOURCE_COLUMN = 3        ///< The program's own pair, or the writer's
};

/// @brief The text of a cell before it was edited: put back when the edit cannot be
constexpr int PREVIOUS_TEXT_ROLE = Qt::UserRole;

/// @brief The rows of the replacement table and of the lists of exceptions shown at least
constexpr int LIST_ROWS = 6;

/// @brief The keys of Edit > Undo as the system writes them; empty: none
QString undoKeys() {
    const Command* command = CommandRegistry::getInstance().getCommand("edit.undo");
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QString();
}

/// @brief Whether a text is one word: not empty, without spaces
bool isOneWord(const QString& text) {
    return !text.isEmpty() &&
           std::none_of(text.begin(), text.end(), [](QChar c) { return c.isSpace(); });
}

/// @brief A titled group of a tab, with a form
QFormLayout* addTabGroup(QVBoxLayout* tab, const QString& title) {
    auto* group = new QGroupBox(title);
    auto* form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    tab->addWidget(group);
    return form;
}

/// @brief A title over a list, bold
QLabel* newListTitle(const QString& text) {
    auto* title = new QLabel(text);
    title->setWordWrap(true);
    // In line with the title beside it, also when this one takes fewer lines
    title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);
    return title;
}

/// @brief The height of a list or a table that shows @p rows rows of @p rowHeight
int heightForRows(const QAbstractItemView* view, int rows, int rowHeight, int headerHeight = 0) {
    return rows * rowHeight + headerHeight + 2 * view->frameWidth();
}

/// @brief A list of words with a field to add one: Enter in the field or Add adds the word
///        at the end, Delete in the list or Remove takes the words selected off
class WordListEditor : public QWidget {
public:
    /// @param name What the list is, for the screen readers
    /// @param placeholder An example in the empty field
    WordListEditor(const QString& name, const QString& placeholder, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_field = new QLineEdit();
        m_field->setPlaceholderText(placeholder);
        //: %1 is what the list is, as "Words with two capitals"
        m_field->setAccessibleName(EditorAutoCorrectPage::tr("A word to add: %1").arg(name));
        m_field->setMinimumWidth(m_field->fontMetrics().averageCharWidth() * 8);
        m_field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_field->installEventFilter(this);
        m_add = new QPushButton(EditorAutoCorrectPage::tr("Add"));
        m_remove = new QPushButton(EditorAutoCorrectPage::tr("Remove"));
        // Where the page is narrow the buttons go under the field
        auto* row = new FlowLayout(nullptr, 6, 4);
        row->addWidget(m_field);
        row->addWidget(m_add);
        row->addWidget(m_remove);
        layout->addLayout(row);
        m_list = new QListWidget();
        m_list->setAccessibleName(name);
        m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_list->setMinimumHeight(
            heightForRows(m_list, LIST_ROWS, m_list->fontMetrics().height() + 4));
        m_list->installEventFilter(this);
        layout->addWidget(m_list, 1);

        connect(m_add, &QPushButton::clicked, this, [this]() { addWord(); });
        connect(m_remove, &QPushButton::clicked, this, [this]() { removeWords(); });
        connect(m_field, &QLineEdit::textChanged, this, [this]() { updateButtons(); });
        connect(m_list, &QListWidget::itemSelectionChanged, this,
                [this]() { updateButtons(); });
        updateButtons();
    }

    [[nodiscard]] QStringList words() const {
        QStringList words;
        for (int row = 0; row < m_list->count(); ++row) {
            words.append(m_list->item(row)->text());
        }
        return words;
    }

    void setWords(const QStringList& words) {
        m_list->clear();
        m_list->addItems(words);
        updateButtons();
    }

    /// @brief The words of a value of the settings
    static QStringList wordsOf(const json& value) {
        QStringList words;
        if (value.is_array()) {
            for (const json& word : value) {
                if (word.is_string()) {
                    words.append(QString::fromStdString(word.get<std::string>()));
                }
            }
        }
        return words;
    }

    /// @brief The value of the settings with the words
    static json valueOf(const QStringList& words) {
        json value = json::array();
        for (const QString& word : words) {
            value.push_back(word.toStdString());
        }
        return value;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (watched == m_field &&
                (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
                // The dialog stays open
                addWord();
                return true;
            }
            if (watched == m_list && key->key() == Qt::Key_Delete &&
                key->modifiers() == Qt::NoModifier) {
                removeWords();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    [[nodiscard]] bool canAdd(const QString& word) const {
        return isOneWord(word) &&
               m_list->findItems(word, Qt::MatchExactly | Qt::MatchCaseSensitive).isEmpty();
    }

    void addWord() {
        const QString word = m_field->text().trimmed();
        if (!canAdd(word)) {
            return;
        }
        auto* item = new QListWidgetItem(word, m_list);
        m_list->setCurrentItem(item);
        m_list->scrollToItem(item);
        m_field->clear();
        updateButtons();
    }

    void removeWords() {
        const QList<QListWidgetItem*> selected = m_list->selectedItems();
        if (selected.isEmpty()) {
            return;
        }
        int next = m_list->count();
        for (const QListWidgetItem* item : selected) {
            next = std::min(next, m_list->row(item));
        }
        qDeleteAll(selected);
        // The keys go on from the word after them
        if (m_list->count() > 0) {
            m_list->setCurrentRow(std::min(next, m_list->count() - 1));
        }
        updateButtons();
    }

    void updateButtons() {
        m_add->setEnabled(canAdd(m_field->text().trimmed()));
        m_remove->setEnabled(!m_list->selectedItems().isEmpty());
    }

    QLineEdit* m_field = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
};

}  // namespace

EditorAutoCorrectPage::EditorAutoCorrectPage(QWidget* parent)
    : SettingsPage(parent)
{
    // Short check boxes, one below another, and the details in notes that wrap: the page fits
    // a narrow window, as the other pages do, without scrolling sideways
    auto* tabs = new QTabWidget();
    tabs->addTab(createWhileTypingTab(), tr("While Typing"));
    tabs->addTab(createCapitalsTab(), tr("Capitals"));
    tabs->addTab(createTableTab(), tr("Replacement Table"));
    pageLayout()->addWidget(tabs, 1);

    // Ctrl+Page Down and Ctrl+Page Up: the next and the previous tab, also from the table
    const auto addTabKeys = [tabs](QKeyCombination keys, int step) {
        auto* shortcut = new QShortcut(QKeySequence(keys), tabs);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated, tabs, [tabs, step]() {
            tabs->setCurrentIndex((tabs->currentIndex() + step + tabs->count()) % tabs->count());
        });
    };
    addTabKeys(Qt::ControlModifier | Qt::Key_PageDown, 1);
    addTabKeys(Qt::ControlModifier | Qt::Key_PageUp, -1);

    connect(m_enabled, &QCheckBox::toggled, this, &EditorAutoCorrectPage::syncReplacements);
    for (const auto& [replacement, choice] : m_choices) {
        connect(replacement, &QCheckBox::toggled, this, &EditorAutoCorrectPage::syncReplacements);
    }
    whenLoaded([this]() { syncReplacements(); });
}

QWidget* EditorAutoCorrectPage::createWhileTypingTab()
{
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);
    QFormLayout* replace = addTabGroup(layout, tr("Replace as You Type"));

    // Auto-correct as a whole, as Tools > Auto-correct as You Type turns it on and off: off,
    // the replacements are greyed out and keep their choices
    m_enabled = addCheckBox(replace, tr("Auto-correct as you type"), "editor.autoCorrect.enabled");
    m_replacements.push_back(addCheckBox(replace, tr("Straight quotes with typographic ones"),
                                         "editor.autoCorrect.quotes"));
    m_replacements.push_back(
        addCheckBox(replace, tr("Apostrophe ' with ’"), "editor.autoCorrect.apostrophe"));

    // A replacement to a dash with the dash beside it, under it where the page is narrow; the
    // dash is chosen only for the replacement made
    const auto addDashChoice = [this, replace](const QString& text, const std::string& key,
                                               const QString& dashName,
                                               const std::string& dashKey) {
        auto* box = new QCheckBox(text);
        auto* dash = new QComboBox();
        dash->setAccessibleName(dashName);
        dash->addItem(tr("Em Dash (—)"), QStringLiteral("emDash"));
        dash->addItem(tr("En Dash (–)"), QStringLiteral("enDash"));
        auto* row = new FlowLayout(nullptr, 6, 4);
        row->addWidget(box);
        row->addWidget(dash);
        replace->addRow(row);
        bind(box, key);
        bind(dash, dashKey);
        m_replacements.push_back(box);
        m_choices.emplace_back(box, dash);
        return box;
    };
    addDashChoice(tr("Hyphen opening a paragraph with:"), "editor.autoCorrect.dialogueDash",
                  tr("Dialogue dash"),
                  "editor.autoCorrect.dialogueDashType");
    QCheckBox* betweenWords =
        addDashChoice(tr("“ - ” between words with:"), "editor.autoCorrect.dashBetweenWords",
                      tr("Dash between words"), "editor.autoCorrect.dashBetweenWordsType");

    // In line with the text of the replacement it belongs to
    auto* inDialogue = new QCheckBox(tr("In dialogue: the dash it starts with"));
    auto* indented = new QHBoxLayout();
    indented->setContentsMargins(
        betweenWords->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, betweenWords) +
            betweenWords->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr,
                                               betweenWords),
        0, 0, 0);
    indented->addWidget(inDialogue);
    replace->addRow(indented);
    bind(inDialogue, "editor.autoCorrect.dashInDialogue");
    m_choices.emplace_back(betweenWords, inDialogue);

    m_replacements.push_back(addCheckBox(replace, tr("“--” with an en dash “–”"),
                                         "editor.autoCorrect.doubleHyphen"));
    m_replacements.push_back(addCheckBox(replace, tr("“...” with an ellipsis “…”"),
                                         "editor.autoCorrect.ellipsis"));
    m_useTable = addCheckBox(replace, tr("Replacements from the table"),
                             "editor.autoCorrect.replacementTable");
    m_replacements.push_back(m_useTable);
    addNote(replace, tr("The quotation marks follow the language of the book: „…” and ‚…’ in "
                        "Polish, “…” and ‘…’ in English. Outside the book, as in a file opened "
                        "from the disk, they follow the language of the program. A language "
                        "chosen on the Spelling page goes before both. The hyphen becomes a dash "
                        "when a space is typed after it."));

    // What is typed in a paragraph of its own and Enter makes an element of the text
    QFormLayout* elements = addTabGroup(layout, tr("Text Elements"));
    m_replacements.push_back(addCheckBox(elements, tr("“***” and Enter: a scene break"),
                                         "editor.autoCorrect.sceneBreak"));
    m_replacements.push_back(addCheckBox(elements, tr("“---” and Enter: a horizontal line"),
                                         "editor.autoCorrect.horizontalRule"));
    addNote(elements, tr("Typed in an empty paragraph. “* ” and “1. ” at the start of a "
                         "paragraph will come with the lists."));

    addNote(layout, tr("Tools > Auto-correct as You Type, also a button on the Tools toolbar, "
                       "turns all the replacements on and off at once; the choices above "
                       "stay."));

    // With the keys Undo has now (the user may change them)
    QLabel* undoNote = addNote(layout, QString());
    const auto showUndoKeys = [undoNote]() {
        const QString keys = undoKeys();
        //: %1 is the keys of Edit > Undo, as Ctrl+Z
        const QString undo =
            !keys.isEmpty()
                ? tr("%1 right after a replacement undoes just the replacement.").arg(keys)
                : tr("Edit > Undo right after a replacement undoes just the replacement.");
        undoNote->setText(undo + QLatin1Char(' ') +
                          tr("In a paragraph of the Code style auto-correct changes nothing."));
    };
    showUndoKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, undoNote,
            showUndoKeys);
    layout->addStretch(1);
    return tab;
}

QWidget* EditorAutoCorrectPage::createCapitalsTab()
{
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);
    QFormLayout* capitals = addTabGroup(layout, tr("Capitals"));
    m_replacements.push_back(addCheckBox(capitals, tr("A capital at the start of a sentence"),
                                         "editor.autoCorrect.sentenceCapitals"));
    m_replacements.push_back(addCheckBox(capitals, tr("Correct TWo INitial CApitals"),
                                         "editor.autoCorrect.twoInitialCapitals"));
    m_replacements.push_back(addCheckBox(capitals, tr("Correct words typed with Caps Lock on"),
                                         "editor.autoCorrect.capsLock"));
    addNote(capitals, tr("A sentence starts also after a dialogue dash. No capital after an "
                         "abbreviation of the list below, a number with a period (5.), an "
                         "initial (J. Kowalski) or an ellipsis. A word typed with Caps Lock on "
                         "by mistake, as “cAPS”, becomes “Caps”."));

    // The abbreviations of each language and the words with two capitals, side by side
    auto* exceptions = new QGroupBox(tr("Exceptions"));
    auto* grid = new QGridLayout(exceptions);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addWidget(exceptions, 1);

    grid->addWidget(newListTitle(tr("Sentences do not end after these abbreviations")), 0, 0);
    grid->addWidget(newListTitle(tr("Words with two capitals")), 0, 1);

    // The list of the language chosen; another language has the English one
    // As narrow as the column needs (the window may be narrow)
    auto* language = new QComboBox();
    language->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    language->setMinimumContentsLength(10);
    language->addItem(tr("Polish"));
    language->addItem(tr("English and other languages"));
    auto* languageLabel = new QLabel(tr("For a text in:"));
    languageLabel->setBuddy(language);
    auto* languageRow = new FlowLayout(nullptr, 6, 4);
    languageRow->addWidget(languageLabel);
    languageRow->addWidget(language);
    grid->addLayout(languageRow, 1, 0);
    auto* wordsNote = new QVBoxLayout();
    addNote(wordsNote, tr("They stay as typed."))->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    grid->addLayout(wordsNote, 1, 1);

    struct LanguageList {
        const char* code;
        QString name;
        QString example;
    };
    auto* abbreviations = new QStackedWidget();
    for (const LanguageList& language :
         {LanguageList{"pl", tr("Abbreviations, Polish"), tr("e.g. godz.")},
          LanguageList{"en", tr("Abbreviations, English and other languages"),
                       tr("e.g. Mr.")}}) {
        auto* list = new WordListEditor(language.name, language.example);
        abbreviations->addWidget(list);
        std::string key = std::string("editor.autoCorrect.abbreviations.") + language.code;
        Binding words;
        words.key = [key]() { return key; };
        words.stored = [key]() { return core::SettingsManager::getInstance().get<json>(key); };
        words.shown = [list]() { return WordListEditor::valueOf(list->words()); };
        words.show = [list](const json& value) { list->setWords(WordListEditor::wordsOf(value)); };
        words.store = [key](const json& value) {
            core::SettingsManager::getInstance().set<json>(key, value);
        };
        bindCustom(std::move(words));
    }
    connect(language, &QComboBox::currentIndexChanged, abbreviations,
            &QStackedWidget::setCurrentIndex);
    language->setCurrentIndex(programLanguage().startsWith(QStringLiteral("pl")) ? 0 : 1);
    abbreviations->setCurrentIndex(language->currentIndex());
    grid->addWidget(abbreviations, 2, 0);

    auto* twoCapitals = new WordListEditor(tr("Words with two capitals"), tr("e.g. PCs"));
    grid->addWidget(twoCapitals, 2, 1);
    Binding words;
    words.key = []() { return std::string("editor.autoCorrect.twoCapitalsExceptions"); };
    words.stored = []() {
        return core::SettingsManager::getInstance().get<json>(
            "editor.autoCorrect.twoCapitalsExceptions");
    };
    words.shown = [twoCapitals]() { return WordListEditor::valueOf(twoCapitals->words()); };
    words.show = [twoCapitals](const json& value) {
        twoCapitals->setWords(WordListEditor::wordsOf(value));
    };
    words.store = [](const json& value) {
        core::SettingsManager::getInstance().set<json>("editor.autoCorrect.twoCapitalsExceptions",
                                                       value);
    };
    bindCustom(std::move(words));

    // Undoing a capital, or the change of two capitals, adds the word to its list, as in Word
    auto* learn = new QCheckBox(tr("Add an exception when I undo a change"));
    grid->addWidget(learn, 3, 0, 1, 2);
    bind(learn, "editor.autoCorrect.learnExceptions");
    auto* learnNote = new QVBoxLayout();
    QLabel* undoNote = addNote(learnNote, QString());
    alignWithText(undoNote, learn);
    grid->addLayout(learnNote, 4, 0, 1, 2);
    const auto showUndoKeys = [undoNote]() {
        const QString keys = undoKeys();
        //: %1 is the keys of Edit > Undo, as Ctrl+Z
        undoNote->setText(
            !keys.isEmpty()
                ? tr("%1 right after a capital or a change of two capitals adds the "
                     "abbreviation or the word to its list.")
                      .arg(keys)
                : tr("Edit > Undo right after a capital or a change of two capitals adds the "
                     "abbreviation or the word to its list."));
    };
    showUndoKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, undoNote,
            showUndoKeys);
    return tab;
}

QWidget* EditorAutoCorrectPage::createTableTab()
{
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);
    auto* group = new QGroupBox(tr("All Fixed Replacements"));
    auto* box = new QVBoxLayout(group);
    layout->addWidget(group, 1);

    // A new pair: the word and its replacement, each with its label; the buttons go on in the
    // next line where the page is narrow
    m_newWord = new QLineEdit();
    m_newWord->setPlaceholderText(tr("e.g. (c)"));
    m_newWord->setAccessibleName(tr("The word of a new pair"));
    m_newReplacement = new QLineEdit();
    m_newReplacement->setPlaceholderText(tr("e.g. ©"));
    m_newReplacement->setAccessibleName(tr("Its replacement"));
    const auto labelled = [](const QString& text, QLineEdit* field) {
        auto* label = new QLabel(text);
        label->setBuddy(field);
        field->setMinimumWidth(field->fontMetrics().averageCharWidth() * 10);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* row = new QHBoxLayout();
        row->addWidget(label);
        row->addWidget(field, 1);
        return row;
    };
    m_addPair = new QPushButton(tr("Add"));
    m_removePairs = new QPushButton(tr("Remove"));
    auto* restore = new QPushButton(tr("Restore Defaults"));
    restore->setToolTip(tr("Brings back the built-in replacements as they were; your own stay"));
    auto* newPair = new FlowLayout(nullptr, 6, 4);
    newPair->addLayout(labelled(tr("Replace"), m_newWord));
    newPair->addLayout(labelled(tr("with"), m_newReplacement));
    newPair->addWidget(m_addPair);
    newPair->addWidget(m_removePairs);
    newPair->addWidget(restore);
    box->addLayout(newPair);
    m_pairNote = new QLabel();
    m_pairNote->setWordWrap(true);
    m_pairNote->setTextFormat(Qt::PlainText);
    m_pairNote->hide();
    box->addWidget(m_pairNote);

    m_pairs = new QTableWidget(0, 4);
    m_pairs->setAccessibleName(tr("Replacement table"));
    m_pairs->setHorizontalHeaderLabels({QString(), tr("Replace"), tr("With"), QString()});
    m_pairs->verticalHeader()->hide();
    m_pairs->setShowGrid(false);
    m_pairs->setWordWrap(false);
    m_pairs->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pairs->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_pairs->setEditTriggers(QAbstractItemView::DoubleClicked |
                             QAbstractItemView::EditKeyPressed);
    m_pairs->verticalHeader()->setDefaultSectionSize(m_pairs->fontMetrics().height() + 8);
    QHeaderView* header = m_pairs->horizontalHeader();
    header->setHighlightSections(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(ON_COLUMN, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(WORD_COLUMN, QHeaderView::Stretch);
    header->setSectionResizeMode(REPLACEMENT_COLUMN, QHeaderView::Stretch);
    header->setSectionResizeMode(SOURCE_COLUMN, QHeaderView::ResizeToContents);
    m_pairs->setMinimumHeight(heightForRows(m_pairs, LIST_ROWS,
                                            m_pairs->verticalHeader()->defaultSectionSize(),
                                            header->sizeHint().height()));
    m_pairs->installEventFilter(this);
    box->addWidget(m_pairs, 1);

    addNote(box, tr("Each replacement can be turned off, changed or removed; Restore Defaults "
                    "brings back the built-in ones. In the table Space turns the pairs "
                    "selected on and off, F2 changes a cell, Delete removes the pairs and "
                    "Insert starts a new one."));
    QLabel* unused = addNote(box, tr("The table is not used now: “Replacements from the table” "
                                     "is off on the While Typing tab."));
    const auto showUnused = [this, unused]() {
        unused->setVisible(!m_enabled->isChecked() || !m_useTable->isChecked());
    };
    connect(m_enabled, &QCheckBox::toggled, unused, showUnused);
    connect(m_useTable, &QCheckBox::toggled, unused, showUnused);
    whenLoaded(showUnused);

    m_newWord->installEventFilter(this);
    m_newReplacement->installEventFilter(this);
    connect(m_addPair, &QPushButton::clicked, this, &EditorAutoCorrectPage::addPair);
    connect(m_removePairs, &QPushButton::clicked, this, &EditorAutoCorrectPage::removePairs);
    connect(restore, &QPushButton::clicked, this, &EditorAutoCorrectPage::restoreDefaultPairs);
    // A note on an edit put back stays until the fields change
    for (QLineEdit* field : {m_newWord, m_newReplacement}) {
        connect(field, &QLineEdit::textChanged, this, [this]() {
            m_pairNote->hide();
            updatePairButtons();
        });
    }
    connect(m_pairs, &QTableWidget::itemSelectionChanged, this,
            &EditorAutoCorrectPage::updatePairButtons);
    connect(m_pairs, &QTableWidget::itemChanged, this, &EditorAutoCorrectPage::pairEdited);

    Binding pairs;
    pairs.key = []() { return std::string(REPLACEMENTS_SETTING); };
    pairs.stored = []() { return replacementsValue(replacementEntries()); };
    pairs.shown = [this]() { return replacementsValue(shownPairs()); };
    pairs.show = [this](const json& value) { showPairs(replacementEntriesOf(value)); };
    pairs.store = [](const json& value) { setReplacementEntries(replacementEntriesOf(value)); };
    bindCustom(std::move(pairs));
    return tab;
}

bool EditorAutoCorrectPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return SettingsPage::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    if ((watched == m_newWord || watched == m_newReplacement) &&
        (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
        // The dialog stays open
        addPair();
        return true;
    }
    if (watched == m_pairs && key->modifiers() == Qt::NoModifier) {
        switch (key->key()) {
            case Qt::Key_Space:
                togglePairs();
                return true;
            case Qt::Key_Delete:
                removePairs();
                return true;
            case Qt::Key_Insert:
                m_newWord->setFocus(Qt::ShortcutFocusReason);
                m_newWord->selectAll();
                return true;
            default:
                break;
        }
    }
    return SettingsPage::eventFilter(watched, event);
}

void EditorAutoCorrectPage::showPairs(const std::vector<ReplacementEntry>& pairs)
{
    m_fillingPairs = true;
    m_pairs->setRowCount(0);
    for (const ReplacementEntry& pair : pairs) {
        appendPair(pair);
    }
    m_fillingPairs = false;
    updatePairButtons();
}

std::vector<ReplacementEntry> EditorAutoCorrectPage::shownPairs() const
{
    std::vector<ReplacementEntry> pairs;
    pairs.reserve(static_cast<std::size_t>(m_pairs->rowCount()));
    for (int row = 0; row < m_pairs->rowCount(); ++row) {
        pairs.push_back({m_pairs->item(row, WORD_COLUMN)->text().trimmed(),
                         m_pairs->item(row, REPLACEMENT_COLUMN)->text(),
                         m_pairs->item(row, ON_COLUMN)->checkState() == Qt::Checked});
    }
    return pairs;
}

void EditorAutoCorrectPage::appendPair(const ReplacementEntry& pair)
{
    const bool filling = m_fillingPairs;
    m_fillingPairs = true;
    const int row = m_pairs->rowCount();
    m_pairs->insertRow(row);
    auto* on = new QTableWidgetItem();
    on->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    on->setCheckState(pair.enabled ? Qt::Checked : Qt::Unchecked);
    m_pairs->setItem(row, ON_COLUMN, on);
    for (const auto& [column, text] :
         {std::pair{WORD_COLUMN, pair.from}, std::pair{REPLACEMENT_COLUMN, pair.to}}) {
        auto* item = new QTableWidgetItem(text);
        item->setData(PREVIOUS_TEXT_ROLE, text);
        m_pairs->setItem(row, column, item);
    }
    auto* source = new QTableWidgetItem();
    source->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    source->setForeground(noteColorOn(m_pairs->palette().color(QPalette::Base)));
    m_pairs->setItem(row, SOURCE_COLUMN, source);
    updateSource(row);
    m_fillingPairs = filling;
}

int EditorAutoCorrectPage::rowOfWord(const QString& word, int except) const
{
    for (int row = 0; row < m_pairs->rowCount(); ++row) {
        if (row != except && m_pairs->item(row, WORD_COLUMN)->text().trimmed() == word) {
            return row;
        }
    }
    return -1;
}

void EditorAutoCorrectPage::addPair()
{
    const QString word = m_newWord->text().trimmed();
    const QString replacement = m_newReplacement->text();
    if (!isOneWord(word) || replacement.trimmed().isEmpty()) {
        return;
    }
    int row = rowOfWord(word);
    m_fillingPairs = true;
    if (row >= 0) {
        // The word has a pair: the new replacement in its place
        QTableWidgetItem* item = m_pairs->item(row, REPLACEMENT_COLUMN);
        item->setText(replacement);
        item->setData(PREVIOUS_TEXT_ROLE, replacement);
        m_pairs->item(row, ON_COLUMN)->setCheckState(Qt::Checked);
        updateSource(row);
    } else {
        appendPair({word, replacement, true});
        row = m_pairs->rowCount() - 1;
    }
    m_fillingPairs = false;
    m_pairs->setCurrentCell(row, WORD_COLUMN);
    m_pairs->scrollToItem(m_pairs->item(row, WORD_COLUMN));
    m_newWord->clear();
    m_newReplacement->clear();
    m_newWord->setFocus(Qt::OtherFocusReason);
}

void EditorAutoCorrectPage::removePairs()
{
    QList<int> rows;
    for (const QModelIndex& index : m_pairs->selectionModel()->selectedRows()) {
        rows.append(index.row());
    }
    if (rows.isEmpty()) {
        return;
    }
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (const int row : rows) {
        m_pairs->removeRow(row);
    }
    // The keys go on from the pair after them
    if (m_pairs->rowCount() > 0) {
        m_pairs->setCurrentCell(std::min(rows.last(), m_pairs->rowCount() - 1), WORD_COLUMN);
    }
    updatePairButtons();
}

void EditorAutoCorrectPage::togglePairs()
{
    const QTableWidgetItem* current = m_pairs->item(m_pairs->currentRow(), ON_COLUMN);
    const QModelIndexList rows = m_pairs->selectionModel()->selectedRows();
    if (current == nullptr || rows.isEmpty()) {
        return;
    }
    const Qt::CheckState state =
        current->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
    for (const QModelIndex& index : rows) {
        m_pairs->item(index.row(), ON_COLUMN)->setCheckState(state);
    }
}

void EditorAutoCorrectPage::restoreDefaultPairs()
{
    m_fillingPairs = true;
    for (const ReplacementEntry& pair : defaultReplacementEntries()) {
        const int row = rowOfWord(pair.from);
        if (row < 0) {
            appendPair(pair);
            continue;
        }
        QTableWidgetItem* item = m_pairs->item(row, REPLACEMENT_COLUMN);
        item->setText(pair.to);
        item->setData(PREVIOUS_TEXT_ROLE, pair.to);
        m_pairs->item(row, ON_COLUMN)->setCheckState(pair.enabled ? Qt::Checked : Qt::Unchecked);
        updateSource(row);
    }
    m_fillingPairs = false;
    updatePairButtons();
}

void EditorAutoCorrectPage::pairEdited(QTableWidgetItem* item)
{
    if (m_fillingPairs ||
        (item->column() != WORD_COLUMN && item->column() != REPLACEMENT_COLUMN)) {
        return;
    }
    const int row = item->row();
    const QString previous = item->data(PREVIOUS_TEXT_ROLE).toString();
    QString text = item->column() == WORD_COLUMN ? item->text().trimmed() : item->text();
    QString why;
    if (item->column() == WORD_COLUMN) {
        if (text.isEmpty()) {
            //: %1 is the word the pair had
            why = tr("A replacement needs a word: “%1” stays.").arg(previous);
        } else if (!isOneWord(text)) {
            why = tr("A replacement is one word: without spaces.");
        } else if (rowOfWord(text, row) >= 0) {
            //: %1 is the word typed, %2 the word the pair had
            why = tr("The table has a pair of “%1” already: “%2” stays.").arg(text, previous);
        }
    } else if (text.trimmed().isEmpty()) {
        //: %1 is the replacement the pair had
        why = tr("A word cannot be replaced with nothing: “%1” stays.").arg(previous);
    }
    m_fillingPairs = true;
    if (!why.isEmpty()) {
        text = previous;
    }
    item->setText(text);
    item->setData(PREVIOUS_TEXT_ROLE, text);
    m_fillingPairs = false;
    updateSource(row);
    m_pairNote->setText(why);
    m_pairNote->setVisible(!why.isEmpty());
    updatePairButtons();
}

void EditorAutoCorrectPage::updateSource(int row)
{
    const QString word = m_pairs->item(row, WORD_COLUMN)->text().trimmed();
    const QString replacement = m_pairs->item(row, REPLACEMENT_COLUMN)->text();
    const std::vector<ReplacementEntry> defaults = defaultReplacementEntries();
    const bool builtIn = std::any_of(
        defaults.begin(), defaults.end(), [&word, &replacement](const ReplacementEntry& pair) {
            return pair.from == word && pair.to == replacement;
        });
    //: The program's own pair of the replacement table
    m_pairs->item(row, SOURCE_COLUMN)->setText(builtIn ? tr("built-in") : tr("your own"));
}

void EditorAutoCorrectPage::updatePairButtons()
{
    const QString word = m_newWord->text().trimmed();
    const QString replacement = m_newReplacement->text();
    const int row = rowOfWord(word);
    const bool same = row >= 0 &&
                      m_pairs->item(row, REPLACEMENT_COLUMN)->text() == replacement &&
                      m_pairs->item(row, ON_COLUMN)->checkState() == Qt::Checked;
    m_addPair->setEnabled(isOneWord(word) && !replacement.trimmed().isEmpty() && !same);
    m_removePairs->setEnabled(!m_pairs->selectionModel()->selectedRows().isEmpty());

    // What Add does with the word typed; without one, a note on an edit put back stays
    if (word.isEmpty()) {
        return;
    }
    QString note;
    if (!isOneWord(word)) {
        note = tr("A replacement is one word: without spaces.");
    } else if (row >= 0 && !same && !replacement.trimmed().isEmpty()) {
        note = tr("Auto-correct replaces “%1” with “%2” now: Add puts “%3” in its place.")
                   .arg(word, m_pairs->item(row, REPLACEMENT_COLUMN)->text(), replacement);
    }
    m_pairNote->setText(note);
    m_pairNote->setVisible(!note.isEmpty());
}

void EditorAutoCorrectPage::syncReplacements()
{
    const bool enabled = m_enabled->isChecked();
    for (QCheckBox* replacement : m_replacements) {
        replacement->setEnabled(enabled);
    }
    for (const auto& [replacement, choice] : m_choices) {
        choice->setEnabled(enabled && replacement->isChecked());
    }
}

} // namespace gui
} // namespace kalahari
