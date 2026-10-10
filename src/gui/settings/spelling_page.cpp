/// @file spelling_page.cpp
/// @brief Settings page: Editor > Spelling
///
/// The coordinator of the spelling (SpellingCoordinator) follows the settings by itself;
/// the writer's own words go to its dictionary when the page is applied.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/utils/language_names.h"

#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// @brief The setting of the language checked
constexpr const char* LANGUAGE_KEY = "editor.spellCheck.language";

/// @brief The name of a dictionary in the list: its language in the language of the program,
///        and its code (Polish (pl_PL)); just the code when the language is not known
QString dictionaryName(const QString& dictionary) {
    const QString code = dictionary.trimmed();
    const QString name = utils::languageName(code);
    return name == code ? code : QStringLiteral("%1 (%2)").arg(name, code);
}

/// @brief The dictionaries found, with their names in the list, in the order of the names
std::vector<std::pair<QString, QString>> namedDictionaries() {
    std::vector<std::pair<QString, QString>> named;
    for (const QString& dictionary : editor::SpellCheckService::availableDictionaries()) {
        named.emplace_back(dictionaryName(dictionary), dictionary);
    }
    const QCollator collator(QLocale(
        QString::fromStdString(core::SettingsManager::getInstance().getLanguage())));
    std::sort(named.begin(), named.end(), [&collator](const auto& first, const auto& second) {
        return collator.compare(first.first, second.first) < 0;
    });
    return named;
}

/// @brief Words as a value of a binding
json wordsValue(const QStringList& words) {
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

/// @brief The words of the list, sorted
QStringList listedWords(const QListWidget* list) {
    QStringList words;
    for (int row = 0; row < list->count(); ++row) {
        words.append(list->item(row)->text());
    }
    words.sort();
    return words;
}

/// @brief Whether a word can be added to the list: one word, not listed yet (# starts a
///        comment in the file of the words)
bool canAdd(const QListWidget* list, const QString& word) {
    return !word.isEmpty() && !word.startsWith(QLatin1Char('#')) &&
           std::none_of(word.begin(), word.end(), [](QChar c) { return c.isSpace(); }) &&
           list->findItems(word, Qt::MatchExactly | Qt::MatchCaseSensitive).isEmpty();
}

}  // namespace

EditorSpellingPage::EditorSpellingPage(editor::SpellCheckService* spelling, QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* checking = addGroup(tr("Spelling as You Type"));
    QCheckBox* enabled =
        addCheckBox(checking, tr("Check spelling as you type"), "editor.spellCheck.enabled");

    // The dictionaries found; the language of the book is the one set in its properties
    auto* language = new QComboBox();
    language->addItem(tr("Language of the book"), QString());
    for (const auto& [name, dictionary] : namedDictionaries()) {
        language->addItem(name, dictionary);
    }
    // A language chosen before whose dictionary is gone stays until another one is chosen
    const QString stored = QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(LANGUAGE_KEY, std::string()));
    if (!stored.trimmed().isEmpty() && language->findData(stored) < 0) {
        //: %1 is the language as in the list, for example French (fr_FR)
        language->addItem(editor::SpellCheckService::dictionaryFor(stored).isEmpty()
                              ? tr("%1: no dictionary").arg(dictionaryName(stored))
                              : dictionaryName(stored),
                          stored);
    }
    QLabel* languageLabel = addField(checking, tr("Language:"), language, LANGUAGE_KEY);
    addNote(checking, tr("A misspelled word is underlined with a wavy line; right-click it for "
                         "suggestions. Tools > Check Spelling as You Type (Shift+F7) turns the "
                         "checking on and off."));
    const auto syncLanguage = [enabled, language, languageLabel]() {
        language->setEnabled(enabled->isChecked());
        languageLabel->setEnabled(enabled->isChecked());
    };
    connect(enabled, &QCheckBox::toggled, this, syncLanguage);
    whenLoaded(syncLanguage);

    if (spelling == nullptr) {
        return;
    }

    // The writer's own words, put in the dictionary when the page is applied
    QFormLayout* own = addGroup(tr("Your Words"));
    m_words = new QListWidget();
    m_words->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_words->setSortingEnabled(true);
    m_words->setAccessibleName(tr("Your words"));
    own->addRow(m_words);

    m_newWord = new QLineEdit();
    m_newWord->setPlaceholderText(tr("A word to add"));
    m_newWord->setAccessibleName(tr("A word to add"));
    m_newWord->installEventFilter(this);
    m_add = new QPushButton(tr("Add"));
    m_remove = new QPushButton(tr("Remove"));
    auto* row = new QHBoxLayout();
    row->addWidget(m_newWord, 1);
    row->addWidget(m_add);
    row->addWidget(m_remove);
    own->addRow(row);
    addNote(own, tr("Words that are right whatever the dictionary says; Add to Dictionary in the "
                    "context menu of a misspelled word puts it here. A word in lower case is "
                    "right also with capital letters."));

    connect(m_add, &QPushButton::clicked, this, &EditorSpellingPage::addWord);
    connect(m_remove, &QPushButton::clicked, this, &EditorSpellingPage::removeWords);
    connect(m_newWord, &QLineEdit::textChanged, this, &EditorSpellingPage::updateButtons);
    connect(m_words, &QListWidget::itemSelectionChanged, this,
            &EditorSpellingPage::updateButtons);

    Binding words;
    words.key = []() { return std::string(); };  // not a setting
    words.stored = [spelling]() { return wordsValue(spelling->userDictionaryWords()); };
    words.shown = [this]() { return wordsValue(listedWords(m_words)); };
    words.show = [this](const json& value) {
        m_words->clear();
        m_words->addItems(wordsOf(value));
    };
    words.store = [spelling](const json& value) {
        spelling->setUserDictionaryWords(wordsOf(value));
    };
    bindCustom(std::move(words));
    whenLoaded([this]() { updateButtons(); });
}

bool EditorSpellingPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_newWord && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            addWord();
            return true;
        }
    }
    return SettingsPage::eventFilter(watched, event);
}

void EditorSpellingPage::addWord() {
    const QString word = m_newWord->text().trimmed();
    if (!canAdd(m_words, word)) {
        return;
    }
    auto* item = new QListWidgetItem(word, m_words);
    m_words->setCurrentItem(item);
    m_words->scrollToItem(item);
    m_newWord->clear();
    updateButtons();
}

void EditorSpellingPage::removeWords() {
    qDeleteAll(m_words->selectedItems());
    updateButtons();
}

void EditorSpellingPage::updateButtons() {
    if (m_words == nullptr) {
        return;
    }
    m_add->setEnabled(canAdd(m_words, m_newWord->text().trimmed()));
    m_remove->setEnabled(!m_words->selectedItems().isEmpty());
}

} // namespace gui
} // namespace kalahari
