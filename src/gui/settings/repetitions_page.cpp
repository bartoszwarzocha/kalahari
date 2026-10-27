/// @file repetitions_page.cpp
/// @brief Settings page: Editor > Repetitions
///
/// The coordinator of the repetitions (RepetitionsCoordinator) follows these settings by
/// itself; the color of the marks reaches the editors through the theme (ThemeManager follows
/// its settings).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/word_repetitions.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/repetitions_coordinator.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// @brief The distances the writer can set, in words
constexpr int MIN_DISTANCE = 2;
constexpr int MAX_DISTANCE = 500;

/// @brief The lengths of the words counted the writer can set, in letters
constexpr int MIN_LENGTH = 1;
constexpr int MAX_LENGTH = 20;

/// @brief The rows of the list of the skipped words shown at most before it scrolls
constexpr int WORD_ROWS = 6;

/// @brief A spin box with the word after its number in the form the number needs ("3 słowa",
///        "30 słów"): a label, so typing in the box is not disturbed
/// @param text The number with its word, as tr("%n word(s)", nullptr, number) gives it
QWidget* numberRow(QSpinBox* spin, std::function<QString(int)> text) {
    auto* row = new QWidget();
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* unit = new QLabel();
    layout->addWidget(spin);
    layout->addWidget(unit);
    layout->addStretch();
    const auto update = [spin, unit, text = std::move(text)](int value) {
        const QString number = QString::number(value);
        const QString shown = text(value);
        const int at = shown.indexOf(number);
        unit->setText(at < 0 ? shown : shown.mid(at + number.size()).trimmed());
        spin->setAccessibleDescription(shown);
    };
    QObject::connect(spin, &QSpinBox::valueChanged, unit, update);
    update(spin->value());
    return row;
}

/// @brief The keys of Tools > Next Repetition (the user may change them), or its place in the
///        menu
QString nextRepetitionKeys() {
    const Command* command = CommandRegistry::getInstance().getCommand("tools.nextRepetition");
    if (command != nullptr && !command->shortcut.isEmpty()) {
        return command->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
    }
    //: Next Repetition without keys. In Polish: Narzędzia > Następne powtórzenie
    return EditorRepetitionsPage::tr("Tools > Next Repetition");
}

/// @brief Words as a value of a binding, in lower case, without repeats, sorted
json wordsValue(QStringList words) {
    for (QString& word : words) {
        word = word.toLower();
    }
    words.removeDuplicates();
    words.sort();
    json value = json::array();
    for (const QString& word : words) {
        value.push_back(word.toStdString());
    }
    return value;
}

/// @brief The words of a binding's value
QStringList wordsOf(const json& value) {
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

/// @brief The words of the list
QStringList listedWords(const QListWidget* list) {
    QStringList words;
    for (int row = 0; row < list->count(); ++row) {
        words.append(list->item(row)->text());
    }
    return words;
}

/// @brief Whether a word can be added to the list: one word, not listed yet
bool canAdd(const QListWidget* list, const QString& word) {
    return !word.isEmpty() &&
           std::none_of(word.begin(), word.end(), [](QChar c) { return c.isSpace(); }) &&
           list->findItems(word.toLower(), Qt::MatchExactly | Qt::MatchCaseSensitive).isEmpty();
}

}  // namespace

EditorRepetitionsPage::EditorRepetitionsPage(QWidget* parent)
    : SettingsPage(parent)
{
    using Coordinator = RepetitionsCoordinator;

    // What counts as a word that comes back too close
    //: In Polish: Co jest powtórzeniem
    QFormLayout* counted = addGroup(tr("What Is a Repetition"));
    auto* distance = new QSpinBox();
    distance->setObjectName(QStringLiteral("repetitionsDistance"));
    distance->setRange(MIN_DISTANCE, MAX_DISTANCE);
    //: In Polish: Słowo wraca w odstępie do
    distance->setAccessibleName(tr("A word comes back within"));
    //: In Polish: Słowo wraca w odstępie do:
    counted->addRow(tr("A word comes back within:"), numberRow(distance, [](int words) {
                        //: After the distance of Settings > Editor > Repetitions. In Polish:
                        //: %n słowo / słowa / słów
                        return tr("%n word(s)", nullptr, words);
                    }));
    bind(distance, Coordinator::DISTANCE_KEY);

    auto* length = new QSpinBox();
    length->setObjectName(QStringLiteral("repetitionsMinLength"));
    length->setRange(MIN_LENGTH, MAX_LENGTH);
    //: In Polish: Liczone słowa mają co najmniej
    length->setAccessibleName(tr("Counted words have at least"));
    //: In Polish: Liczone słowa mają co najmniej:
    counted->addRow(tr("Counted words have at least:"), numberRow(length, [](int letters) {
                        //: After the length of the words counted. In Polish:
                        //: %n literę / litery / liter
                        return tr("%n letter(s)", nullptr, letters);
                    }));
    bind(length, Coordinator::MIN_LENGTH_KEY);

    //: In Polish: Łącz odmiany jednego słowa
    QCheckBox* forms = addCheckBox(counted, tr("Merge the forms of a word"),
                                   Coordinator::MERGE_FORMS_KEY);
    forms->setObjectName(QStringLiteral("repetitionsMergeForms"));
    //: In Polish: „mówił” i „mówiła” to jedno słowo, według słownika pisowni języka tekstu.
    alignWithText(addNote(counted, tr("\"said\" and \"says\" are one word, by the spelling "
                                      "dictionary of the language of the text.")),
                  forms);
    //: In Polish: Pomijaj bardzo częste słowa
    QCheckBox* common = addCheckBox(counted, tr("Skip very common words"),
                                    Coordinator::SKIP_COMMON_KEY);
    common->setObjectName(QStringLiteral("repetitionsSkipCommon"));
    //: The very common words of the language. In Polish: Takie jak i, w, się, że, nie.
    alignWithText(addNote(counted, tr("Such as and, the, of, to, it.")), common);
    QLabel* countedNote = addNote(counted, QString());
    // With the keys Next Repetition has now (the user may change them)
    const auto showCountedNote = [countedNote]() {
        //: %1: the keys of Tools > Next Repetition (Ctrl+Shift+F7). In Polish: Tak liczą panel
        //: „Powtórzenia” (Narzędzia > Powtórzenia słów) i %1, które przechodzi do następnego
        //: powtórzenia.
        countedNote->setText(tr("So count the Repetitions panel (Tools > Word Repetitions) and "
                                "%1, which goes to the next repetition.")
                                 .arg(nextRepetitionKeys()));
    };
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, countedNote,
            showCountedNote);
    showCountedNote();

    // The writer's words that do not count
    //: In Polish: Pomijane słowa
    QFormLayout* skipped = addGroup(tr("Skipped Words"));
    m_words = new QListWidget();
    m_words->setObjectName(QStringLiteral("repetitionsSkipped"));
    m_words->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_words->setSortingEnabled(true);
    m_words->setAccessibleName(tr("Skipped words"));
    m_words->setMaximumHeight(m_words->fontMetrics().height() * WORD_ROWS +
                              2 * m_words->frameWidth());
    m_words->installEventFilter(this);
    skipped->addRow(m_words);

    m_newWord = new QLineEdit();
    m_newWord->setObjectName(QStringLiteral("repetitionsNewWord"));
    //: In Polish: Słowo do pominięcia
    m_newWord->setPlaceholderText(tr("A word to skip"));
    m_newWord->setAccessibleName(tr("A word to skip"));
    m_newWord->installEventFilter(this);
    m_add = new QPushButton(tr("Add"));
    m_add->setObjectName(QStringLiteral("repetitionsAddWord"));
    m_remove = new QPushButton(tr("Remove"));
    m_remove->setObjectName(QStringLiteral("repetitionsRemoveWords"));
    auto* row = new QHBoxLayout();
    row->addWidget(m_newWord, 1);
    row->addWidget(m_add);
    row->addWidget(m_remove);
    skipped->addRow(row);
    //: In Polish: Te słowa nie są liczone w żadnej odmianie. „Pomijaj to słowo” na karcie
    //: panelu „Powtórzenia” (albo Delete) dopisuje tu słowo karty.
    addNote(skipped, tr("These words are not counted in any form. \"Skip This Word\" on a card "
                        "of the Repetitions panel (or Delete) adds the word of the card here."));

    connect(m_add, &QPushButton::clicked, this, &EditorRepetitionsPage::addWord);
    connect(m_remove, &QPushButton::clicked, this, &EditorRepetitionsPage::removeWords);
    connect(m_newWord, &QLineEdit::textChanged, this, &EditorRepetitionsPage::updateButtons);
    connect(m_words, &QListWidget::itemSelectionChanged, this,
            &EditorRepetitionsPage::updateButtons);

    Binding words;
    words.key = []() { return std::string(Coordinator::SKIPPED_KEY); };
    words.stored = []() {
        return wordsValue(wordsOf(core::SettingsManager::getInstance().get<json>(
            Coordinator::SKIPPED_KEY, json::array())));
    };
    words.shown = [this]() { return wordsValue(listedWords(m_words)); };
    words.show = [this](const json& value) {
        m_words->clear();
        m_words->addItems(wordsOf(value));
    };
    words.store = [](const json& value) {
        core::SettingsManager::getInstance().set<json>(Coordinator::SKIPPED_KEY, value);
    };
    bindCustom(std::move(words));
    whenLoaded([this]() { updateButtons(); });

    // The marks in the text
    //: In Polish: W tekście
    QFormLayout* marks = addGroup(tr("In the Text"));
    //: In Polish: Zaznaczaj powtórzenia w tekście
    QCheckBox* highlight = addCheckBox(marks, tr("Mark the repetitions in the text"),
                                       Coordinator::HIGHLIGHT_KEY);
    highlight->setObjectName(QStringLiteral("repetitionsHighlight"));
    auto* style = new QComboBox();
    style->setObjectName(QStringLiteral("repetitionsStyle"));
    //: How the repeated words are marked. In Polish: Tło
    style->addItem(tr("Background"), QStringLiteral("background"));
    //: How the repeated words are marked. In Polish: Falista linia
    style->addItem(tr("Wavy line"), QStringLiteral("wave"));
    //: In Polish: Sposób zaznaczenia:
    QLabel* styleLabel = addField(marks, tr("Marked with:"), style, Coordinator::STYLE_KEY);
    //: In Polish: Słowa są zaznaczone, gdy panel „Powtórzenia” jest widoczny; słowa wybranej
    //: karty mają obwódkę. Widok > Powtórzenia w tekście też to włącza i wyłącza. Każdy motyw
    //: ma swój kolor: to kolor bieżącego motywu.
    addNote(marks, tr("The words are marked while the Repetitions panel is shown; the words of "
                      "the card chosen are framed. View > Repetitions in Text turns it on and "
                      "off too. Each theme has its own color: this is the color of the current "
                      "theme."));
    //: The color of the repeated words in the text. In Polish: Kolor zaznaczenia
    addThemeColor(marks, tr("Mark"), editor::REPETITION_COLOR_KEY);
    addOptimizeColorsCheckBox(marks);
    addRestoreColorsButton(marks);
    const auto syncStyle = [highlight, style, styleLabel]() {
        style->setEnabled(highlight->isChecked());
        styleLabel->setEnabled(highlight->isChecked());
    };
    connect(highlight, &QCheckBox::toggled, this, syncStyle);
    whenLoaded(syncStyle);

    pageLayout()->addStretch();
}

bool EditorRepetitionsPage::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (watched == m_newWord && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
            addWord();
            return true;
        }
        if (watched == m_words && key->key() == Qt::Key_Delete && m_remove->isEnabled()) {
            removeWords();
            return true;
        }
    }
    return SettingsPage::eventFilter(watched, event);
}

void EditorRepetitionsPage::addWord() {
    const QString word = m_newWord->text().trimmed().toLower();
    if (!canAdd(m_words, word)) {
        return;
    }
    auto* item = new QListWidgetItem(word, m_words);
    m_words->setCurrentItem(item);
    m_words->scrollToItem(item);
    m_newWord->clear();
    updateButtons();
}

void EditorRepetitionsPage::removeWords() {
    qDeleteAll(m_words->selectedItems());
    updateButtons();
}

void EditorRepetitionsPage::updateButtons() {
    m_add->setEnabled(canAdd(m_words, m_newWord->text().trimmed()));
    m_remove->setEnabled(!m_words->selectedItems().isEmpty());
}

} // namespace gui
} // namespace kalahari
