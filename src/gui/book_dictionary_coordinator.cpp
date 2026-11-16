// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary_coordinator.cpp
/// @brief The book's dictionary on its tab, its windows and the commands that change it

#include "kalahari/gui/book_dictionary_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/name_dialog.h"
#include "kalahari/gui/dialogs/rule_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/book_dictionary_panel.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QLocale>
#include <QStatusBar>
#include <QTabWidget>

#include <functional>

namespace kalahari::gui {

namespace {

using Part = BookDictionaryPanel::Part;

/// @brief How long a message stays in the status bar
constexpr int MESSAGE_MS = 5000;

/// @brief The icon of the dictionary, of its tab and its windows
constexpr const char* ICON_ID = "book.dictionary";

/// @brief The question before entries of the dictionary are removed, and the title of its
///        window; empty when the entries are not there
QString removeQuestion(const core::BookDictionary& dictionary, Part part, const QStringList& ids,
                       QString& title) {
    const QLocale locale;
    switch (part) {
    case Part::Names: {
        const core::DictionaryName* name = dictionary.findName(ids.constFirst());
        //: In Polish: Usuń imię lub nazwę
        title = BookDictionaryCoordinator::tr("Remove Name");
        //: %1: the name in quotes. In Polish: Usunąć %1 ze słownika książki? Jego formy nie
        //: będą już poprawne w książce.
        return name == nullptr ? QString()
                               : BookDictionaryCoordinator::tr(
                                     "Remove %1 from the book's dictionary? Its forms will no "
                                     "longer be right in the book.")
                                     .arg(locale.quoteString(name->text()));
    }
    case Part::Rules: {
        const core::SpellingRule* rule = dictionary.findRule(ids.constFirst());
        //: In Polish: Usuń zasadę pisowni
        title = BookDictionaryCoordinator::tr("Remove Spelling Rule");
        //: %1: the right spelling of the rule in quotes. In Polish: Usunąć zasadę %1 ze
        //: słownika książki? Zapisy, które zaznacza, nie będą już zaznaczane.
        return rule == nullptr ? QString()
                               : BookDictionaryCoordinator::tr(
                                     "Remove the rule %1 from the book's dictionary? The "
                                     "spellings it marks will no longer be marked.")
                                     .arg(locale.quoteString(rule->right));
    }
    case Part::Words:
        break;
    }
    //: In Polish: Usuń słowa
    title = BookDictionaryCoordinator::tr("Remove Words");
    //: %1: the word in quotes. In Polish: Usunąć słowo %1 ze słownika książki?
    return ids.size() == 1 ? BookDictionaryCoordinator::tr(
                                 "Remove the word %1 from the book's dictionary?")
                                 .arg(locale.quoteString(ids.constFirst()))
                           //: %1: how many. In Polish: Usunąć wybrane słowa ze słownika
                           //: książki (%1)?
                           : BookDictionaryCoordinator::tr(
                                 "Remove the chosen words from the book's dictionary (%1)?")
                                 .arg(ids.size());
}

/// @brief Take entries of a part out of the dictionary
void removeFrom(core::BookDictionary& dictionary, Part part, const QStringList& ids) {
    for (const QString& id : ids) {
        switch (part) {
        case Part::Names:
            dictionary.removeName(id);
            break;
        case Part::Rules:
            dictionary.removeRule(id);
            break;
        case Part::Words:
            dictionary.removeWord(id);
            break;
        }
    }
}

}  // namespace

BookDictionaryCoordinator::BookDictionaryCoordinator(core::ProjectManager& book,
                                                     CommandRegistry& commands,
                                                     QTabWidget* centralTabs,
                                                     SpellingCoordinator* spelling,
                                                     QStatusBar* statusBar, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_spelling(spelling),
      m_statusBar(statusBar),
      m_book(book),
      m_commands(commands) {
    // The page follows the dictionary
    connect(&m_book, &core::ProjectManager::bookDictionaryChanged, this, [this]() {
        if (BookDictionaryPanel* shown = panel()) {
            shown->refresh();
        }
    });
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() {
                if (BookDictionaryPanel* shown = panel()) {
                    updateTab(shown);
                }
            });
}

BookDictionaryCoordinator::~BookDictionaryCoordinator() {
    // The commands outlive the coordinator
    if (Command* command = m_commands.getCommand(COMMAND_ID)) {
        command->execute = nullptr;
    }
}

void BookDictionaryCoordinator::connectCommands() {
    if (Command* command = m_commands.getCommand(COMMAND_ID)) {
        command->execute = [this]() { openDictionary(true); };
        m_commands.updateActionState(COMMAND_ID);
    }
}

BookDictionaryPanel* BookDictionaryCoordinator::openDictionary(bool keys) {
    if (!m_book.isProjectOpen() || m_centralTabs == nullptr) {
        return nullptr;
    }
    BookDictionaryPanel* shown = panel();
    if (shown == nullptr) {
        shown = new BookDictionaryPanel(m_book, m_centralTabs);
        connectPanel(shown);
        m_centralTabs->addTab(shown, QString());
    } else {
        shown->refresh();
    }
    updateTab(shown);
    DocumentWindows::of(*m_centralTabs).showPage(shown);
    if (keys) {
        shown->focusList();
    }
    return shown;
}

BookDictionaryPanel* BookDictionaryCoordinator::panel() const {
    if (m_centralTabs == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* shown = qobject_cast<BookDictionaryPanel*>(page)) {
            return shown;
        }
    }
    return nullptr;
}

BookDictionaryPanel* BookDictionaryCoordinator::currentPanel() const {
    return m_centralTabs != nullptr
               ? qobject_cast<BookDictionaryPanel*>(DocumentWindows::of(*m_centralTabs).frontPage())
               : nullptr;
}

void BookDictionaryCoordinator::connectPanel(BookDictionaryPanel* panel) {
    connect(panel, &BookDictionaryPanel::addRequested, this, [this](Part part) {
        if (part == Part::Rules) {
            addRule();
        } else {
            addName();
        }
    });
    connect(panel, &BookDictionaryPanel::editRequested, this,
            [this](Part part, const QString& id) {
                if (part == Part::Rules) {
                    editRule(id);
                } else {
                    editName(id);
                }
            });
    connect(panel, &BookDictionaryPanel::removeRequested, this,
            &BookDictionaryCoordinator::removeEntries);
    connect(panel, &BookDictionaryPanel::addWordRequested, this,
            &BookDictionaryCoordinator::addWord);
    connect(panel, &BookDictionaryPanel::moveToUserWordsRequested, this,
            &BookDictionaryCoordinator::moveToUserWords);
    connect(panel, &BookDictionaryPanel::settingsRequested, this,
            &BookDictionaryCoordinator::settingsRequested);
}

void BookDictionaryCoordinator::updateTab(BookDictionaryPanel* panel) {
    // DockCoordinator::refreshTabIcons() gives the tab the icon of another theme
    const QString iconId = QString::fromLatin1(ICON_ID);
    panel->setProperty("tabIconId", iconId);
    DocumentWindows::setTabIcon(panel, core::ArtProvider::getInstance().getIcon(iconId));
    //: The tab of the book's dictionary. In Polish: Słownik książki
    DocumentWindows::setTabText(panel, tr("Book Dictionary"));
}

bool BookDictionaryCoordinator::addName(const QString& text) {
    if (!m_book.isProjectOpen()) {
        return false;
    }
    // A name the dictionary has opens with its forms
    const QString written = text.simplified();
    if (const core::DictionaryName* known =
            written.isEmpty() ? nullptr : m_book.bookDictionary().findNameWithText(written)) {
        return runNameDialog(*known);
    }
    core::DictionaryName name;
    if (!written.isEmpty()) {
        name = core::makeName(written, core::NameKind::Character, declension(), dictionaryForms());
    }
    return runNameDialog(name);
}

bool BookDictionaryCoordinator::editName(const QString& nameId) {
    const core::DictionaryName* name =
        m_book.isProjectOpen() ? m_book.bookDictionary().findName(nameId) : nullptr;
    return name != nullptr && runNameDialog(*name);
}

bool BookDictionaryCoordinator::editCardName(const QString& cardId, const QString& cardTitle,
                                             core::NameKind kind) {
    if (!m_book.isProjectOpen()) {
        return false;
    }
    // The card's name, else one without a card of the same name, which the card takes over
    const core::BookDictionary& dictionary = m_book.bookDictionary();
    const core::DictionaryName* known = dictionary.findNameOfCard(cardId);
    if (known == nullptr) {
        known = dictionary.findNameWithText(cardTitle);
        known = known != nullptr && known->card.isEmpty() ? known : nullptr;
    }
    core::DictionaryName name =
        known != nullptr ? *known
                         : core::makeName(cardTitle.simplified(), kind, declension(),
                                          dictionaryForms());
    name.card = cardId;
    return runNameDialog(name);
}

bool BookDictionaryCoordinator::addRule() {
    return m_book.isProjectOpen() && runRuleDialog(core::SpellingRule{});
}

bool BookDictionaryCoordinator::editRule(const QString& ruleId) {
    const core::SpellingRule* rule =
        m_book.isProjectOpen() ? m_book.bookDictionary().findRule(ruleId) : nullptr;
    return rule != nullptr && runRuleDialog(*rule);
}

bool BookDictionaryCoordinator::removeEntries(Part part, const QStringList& ids) {
    if (!m_book.isProjectOpen() || ids.isEmpty()) {
        return false;
    }
    QString title;
    const QString question = removeQuestion(m_book.bookDictionary(), part, ids, title);
    if (question.isEmpty() ||
        !dialogs::MessageDialog::confirm(window(), title, question, tr("&Remove"))) {
        return false;
    }
    return change([part, &ids](core::BookDictionary& dictionary) {
        removeFrom(dictionary, part, ids);
    });
}

bool BookDictionaryCoordinator::addWord(const QString& word) {
    const QString written = word.trimmed();
    if (!m_book.isProjectOpen() || written.isEmpty()) {
        return false;
    }
    return change([&written](core::BookDictionary& dictionary) { dictionary.addWord(written); });
}

bool BookDictionaryCoordinator::moveToUserWords(const QStringList& words) {
    editor::SpellCheckService* service = m_spelling != nullptr ? m_spelling->service() : nullptr;
    if (!m_book.isProjectOpen() || words.isEmpty()) {
        return false;
    }
    if (service == nullptr) {
        //: In Polish: Twoich słów nie można teraz zmienić
        showMessage(tr("Your words cannot be changed now"));
        return false;
    }
    if (!change([&words](core::BookDictionary& dictionary) {
            removeFrom(dictionary, Part::Words, words);
        })) {
        return false;
    }
    for (const QString& word : words) {
        service->addToUserDictionary(word);
    }
    return true;
}

bool BookDictionaryCoordinator::runNameDialog(const core::DictionaryName& name) {
    dialogs::NameDialog dialog(name, declension(), dictionaryForms(), window());
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const core::DictionaryName changed = dialog.name();
    core::Logger::getInstance().info("Book dictionary: name {}", changed.text().toStdString());
    return change([&changed](core::BookDictionary& dictionary) { dictionary.putName(changed); });
}

bool BookDictionaryCoordinator::runRuleDialog(const core::SpellingRule& rule) {
    // The forms of the words, and whether a word is right, from the dictionary of the
    // chapters when it is of the language of the book
    const editor::SpellCheckService* service =
        m_spelling != nullptr ? m_spelling->service() : nullptr;
    const bool known = service != nullptr && service->isActive() &&
                       service->currentDictionary().startsWith(declension().language());
    dialogs::RuleDialog::WordCheck isWord;
    if (known) {
        isWord = [service](const QString& word) { return service->isCorrect(word); };
    }
    dialogs::RuleDialog dialog(rule, known ? service->affixExpander() : nullptr, isWord,
                               window());
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const core::SpellingRule changed = dialog.rule();
    core::Logger::getInstance().info("Book dictionary: rule {}", changed.right.toStdString());
    return change([&changed](core::BookDictionary& dictionary) { dictionary.putRule(changed); });
}

bool BookDictionaryCoordinator::change(
    const std::function<void(core::BookDictionary&)>& change) {
    if (m_book.changeBookDictionary(change)) {
        return true;
    }
    showMessage(m_book.isReadOnly()
                    //: In Polish: Książka jest otwarta tylko do odczytu: jej słownika nie można
                    //: zmienić
                    ? tr("The book is open read-only: its dictionary cannot be changed")
                    //: In Polish: Nie można zapisać słownika książki
                    : tr("The book's dictionary cannot be saved"));
    return false;
}

core::NameDeclension BookDictionaryCoordinator::declension() const {
    // The forms are in the language of the book
    const core::ProjectBook* book = m_book.isProjectOpen() ? m_book.book() : nullptr;
    const QString language = book != nullptr ? book->language.trimmed() : QString();
    return core::NameDeclension(language.isEmpty() ? SpellingCoordinator::wantedLanguage(m_book)
                                                   : language);
}

core::NameDeclension::DictionaryForms BookDictionaryCoordinator::dictionaryForms() const {
    const editor::SpellCheckService* service =
        m_spelling != nullptr ? m_spelling->service() : nullptr;
    if (service == nullptr || !service->isActive() ||
        !service->currentDictionary().startsWith(declension().language())) {
        return {};
    }
    return [service](const QString& word) { return service->relatedForms(word); };
}

void BookDictionaryCoordinator::showMessage(const QString& message) {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

QWidget* BookDictionaryCoordinator::window() const {
    if (const BookDictionaryPanel* shown = currentPanel()) {
        return shown->window();
    }
    return m_centralTabs != nullptr ? m_centralTabs->window() : nullptr;
}

}  // namespace kalahari::gui
