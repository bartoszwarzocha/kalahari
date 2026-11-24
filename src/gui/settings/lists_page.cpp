// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file lists_page.cpp
/// @brief Settings page: Editor > Lists
///
/// F12 and Shift+F12 start a list with the default set of its kind (gui/list_sets.h), and so do
/// the buttons Numbering and Bullets; the writer's sets of the program are kept in the Settings,
/// for every book. The editors take them when the settings change (EditorPanel).

#include "kalahari/core/project_manager.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/list_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/list_sets.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>

namespace kalahari::gui {

namespace {

/// @brief The role of an item of the list with the set it shows (its JSON, list_sets::toJson())
constexpr int SET_ROLE = Qt::UserRole + 1;

/// @brief The fewest characters the lists of the default sets show
constexpr int MIN_SET_CHARACTERS = 24;

/// @brief The rows of the list of sets shown at least
constexpr int MIN_SET_ROWS = 6;

/// @brief The keys of command @p commandId as the writer sees them; empty: none
QString keysOf(const CommandRegistry& commands, const char* commandId) {
    const Command* command = commands.getCommand(commandId);
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QString();
}

/// @brief A label with the keys of a command after it: "Numbering (F12)"
QString withKeys(const QString& label, const CommandRegistry& commands, const char* commandId) {
    const QString keys = keysOf(commands, commandId);
    return keys.isEmpty() ? label : QStringLiteral("%1 (%2)").arg(label, keys);
}

/// @brief A set as the JSON of an item of the list
QByteArray setJson(const ListSet& set) {
    return QJsonDocument(list_sets::toJson({set})).toJson(QJsonDocument::Compact);
}

}  // namespace

EditorListsPage::EditorListsPage(const core::ProjectManager& book, CommandRegistry& commands,
                                 QWidget* parent)
    : SettingsPage(parent)
    , m_book(book)
    , m_commands(commands)
{
    //: The top of Settings > Editor > Lists. In Polish: Zestaw to układ wszystkich dziewięciu
    //: poziomów listy naraz: rodzaj numerów albo punktor, tekst wokół numeru i wcięcia.
    addNote(pageLayout(), tr("A set is the layout of all nine levels of a list at once: the "
                             "kind of numbers or the bullet, the text around the number and the "
                             "indents."));

    // The sets F12 and Shift+F12 give
    //: In Polish: Domyślne zestawy
    QFormLayout* defaults = addGroup(tr("Default Sets"));
    m_numbered = new QComboBox(this);
    m_numbered->setObjectName(QStringLiteral("numberedSet"));
    m_numbered->setMinimumContentsLength(MIN_SET_CHARACTERS);
    m_bulleted = new QComboBox(this);
    m_bulleted->setObjectName(QStringLiteral("bulletedSet"));
    m_bulleted->setMinimumContentsLength(MIN_SET_CHARACTERS);
    fillDefaults();
    //: In Polish: Numerowanie
    QLabel* numberedLabel =
        addField(defaults, withKeys(tr("Numbering"), m_commands, "format.numbering"), m_numbered,
                 list_sets::NUMBERED_DEFAULT_KEY);
    //: In Polish: Punktory
    QLabel* bulletedLabel =
        addField(defaults, withKeys(tr("Bullets"), m_commands, "format.bullets"), m_bulleted,
                 list_sets::BULLETED_DEFAULT_KEY);
    connect(&m_commands, &CommandRegistry::shortcutsChanged, this,
            [this, numberedLabel, bulletedLabel]() {
                numberedLabel->setText(withKeys(tr("Numbering"), m_commands, "format.numbering"));
                bulletedLabel->setText(withKeys(tr("Bullets"), m_commands, "format.bullets"));
            });
    //: In Polish: Te klawisze i przyciski „Punktory” i „Numerowanie” na pasku „Formatowanie”
    //: nadają akapitom ten zestaw. Inny zestaw wybiera się strzałką przy przycisku, w menu
    //: Format > Zestawy list albo w oknie „Punktory i numeracja”.
    addNote(defaults, tr("These keys and the buttons Bullets and Numbering of the Format toolbar "
                         "give the paragraphs this set. Another set is chosen with the arrow of "
                         "the button, in Format > List Sets or in the Bullets and Numbering "
                         "window."));

    // The writer's sets of the program
    //: In Polish: Zestawy programu (dla wszystkich książek)
    QFormLayout* program = addGroup(tr("Sets of the Program (for Every Book)"));
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("programSets"));
    m_list->setMinimumHeight(m_list->fontMetrics().lineSpacing() * MIN_SET_ROWS);
    //: The accessible name of the list of the program's sets. In Polish: Zestawy programu
    m_list->setAccessibleName(tr("Sets of the Program"));
    program->addRow(m_list);
    // In two lines where the window is narrow
    auto* buttonRow = new QWidget(this);
    auto* buttons = new FlowLayout(buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Zmień &nazwę...
    m_rename = new QPushButton(tr("Re&name..."), this);
    //: In Polish: &Usuń
    m_delete = new QPushButton(tr("&Delete"), this);
    //: In Polish: &Eksportuj...
    m_export = new QPushButton(tr("&Export..."), this);
    //: In Polish: &Importuj...
    auto* import = new QPushButton(tr("&Import..."), this);
    for (QPushButton* button : {m_rename, m_delete, m_export, import}) {
        buttons->addWidget(button);
    }
    program->addRow(buttonRow);
    //: In Polish: Zestawy książki są zapisane w samej książce, tak jak jej własne style, więc
    //: przenoszą się z nią na inny komputer; zarządza się nimi na karcie „Zestawy” okna „Punktory i
    //: numeracja”.
    addNote(program, tr("The sets of a book are kept in the book itself, as its own styles are, "
                        "so they go with it to another computer; the tab Sets of the Bullets "
                        "and Numbering window manages them."));
    connect(m_rename, &QPushButton::clicked, this, &EditorListsPage::renameChosen);
    connect(m_delete, &QPushButton::clicked, this, &EditorListsPage::deleteChosen);
    connect(m_export, &QPushButton::clicked, this, [this]() { exportSets(); });
    connect(import, &QPushButton::clicked, this, [this]() { importSets(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, &EditorListsPage::renameChosen);
    const auto enable = [this]() {
        const bool chosen = m_list->currentItem() != nullptr;
        m_rename->setEnabled(chosen);
        m_delete->setEnabled(chosen);
        m_export->setEnabled(m_list->count() > 0);
    };
    connect(m_list, &QListWidget::currentRowChanged, this, enable);

    // The list keeps its sets as one setting
    Binding binding;
    binding.key = []() { return std::string(list_sets::PROGRAM_SETS_KEY); };
    binding.stored = []() {
        return nlohmann::json::parse(
            QJsonDocument(list_sets::toJson(list_sets::program())).toJson(QJsonDocument::Compact).toStdString(),
            nullptr, false);
    };
    binding.shown = [this]() {
        return nlohmann::json::parse(
            QJsonDocument(list_sets::toJson(shownSets())).toJson(QJsonDocument::Compact).toStdString(),
            nullptr, false);
    };
    binding.show = [this, enable](const nlohmann::json& value) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(value.dump()));
        showSets(list_sets::fromJson(document.array(), ListSetPlace::Program));
        enable();
    };
    binding.store = [this](const nlohmann::json& /*value*/) { list_sets::setProgram(shownSets()); };
    bindCustom(std::move(binding));
    pageLayout()->addStretch(1);
}

QList<ListSet> EditorListsPage::shownSets() const {
    QList<ListSet> sets;
    for (int row = 0; row < m_list->count(); ++row) {
        const QJsonDocument document =
            QJsonDocument::fromJson(m_list->item(row)->data(SET_ROLE).toByteArray());
        sets << list_sets::fromJson(document.array(), ListSetPlace::Program);
    }
    return sets;
}

void EditorListsPage::showSets(const QList<ListSet>& sets) {
    const int row = m_list->currentRow();
    m_list->clear();
    for (const ListSet& set : sets) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1: %2").arg(set.displayName(), editor::listDefinitionSummary(set.definition)),
            m_list);
        item->setData(SET_ROLE, setJson(set));
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(std::clamp(row, 0, m_list->count() - 1));
    }
    fillDefaults();
}

void EditorListsPage::fillDefaults() {
    // Every set of the kind: the built-in ones, the book's and the program's as the list has them
    QList<ListSet> sets = list_sets::builtIn();
    if (m_book.isProjectOpen()) {
        sets << list_sets::ofBook(m_book);
    }
    if (m_list != nullptr) {
        sets << shownSets();
    }
    for (QComboBox* combo : {m_numbered, m_bulleted}) {
        const editor::ListKind kind =
            combo == m_bulleted ? editor::ListKind::Bulleted : editor::ListKind::Numbered;
        const QString chosen = combo->count() > 0
                                   ? combo->currentData().toString()
                                   : list_sets::defaultId(kind);
        const QSignalBlocker blocker(combo);
        combo->clear();
        for (const ListSet& set : std::as_const(sets)) {
            if (set.kind() != kind) {
                continue;
            }
            const QString name = set.place == ListSetPlace::BuiltIn
                                     ? set.displayName()
                                     : QStringLiteral("%1 (%2)").arg(
                                           set.displayName(), list_sets::groupTitle(set).toLower());
            combo->addItem(name, set.id);
        }
        combo->setCurrentIndex(std::max(0, combo->findData(chosen)));
    }
}

void EditorListsPage::renameChosen() {
    QListWidgetItem* item = m_list->currentItem();
    if (item == nullptr) {
        return;
    }
    QList<ListSet> sets = shownSets();
    const int row = m_list->currentRow();
    dialogs::ListSetNameDialog window(sets.at(row).displayName(), false, this);
    if (window.exec() != QDialog::Accepted || window.name().isEmpty()) {
        return;
    }
    sets[row].name = window.name();
    showSets(sets);
}

void EditorListsPage::deleteChosen() {
    const int row = m_list->currentRow();
    if (row < 0) {
        return;
    }
    QList<ListSet> sets = shownSets();
    sets.removeAt(row);
    showSets(sets);
}

bool EditorListsPage::exportSets(const QString& path) {
    QString target = path;
    if (target.isEmpty()) {
        //: In Polish: Eksportuj zestawy list / Zestawy list (*.json)
        target = QFileDialog::getSaveFileName(this, tr("Export List Sets"),
                                              QDir::home().filePath(QStringLiteral("list-sets.json")),
                                              tr("List sets (*.json)"));
    }
    if (target.isEmpty()) {
        return false;
    }
    if (!list_sets::exportTo(target, shownSets())) {
        //: In Polish: Nie udało się zapisać pliku %1.
        dialogs::MessageDialog::error(this, tr("Export List Sets"),
                                      tr("The file %1 could not be written.")
                                          .arg(QDir::toNativeSeparators(target)));
        return false;
    }
    return true;
}

int EditorListsPage::importSets(const QString& path) {
    QString source = path;
    if (source.isEmpty()) {
        //: In Polish: Importuj zestawy list
        source = QFileDialog::getOpenFileName(this, tr("Import List Sets"), QDir::homePath(),
                                              tr("List sets (*.json)"));
    }
    if (source.isEmpty()) {
        return 0;
    }
    const std::optional<QList<ListSet>> imported = list_sets::importFrom(source);
    if (!imported) {
        //: In Polish: Plik %1 nie zawiera zestawów list wyeksportowanych z Kalahari.
        dialogs::MessageDialog::error(this, tr("Import List Sets"),
                                      tr("The file %1 has no list sets exported from Kalahari.")
                                          .arg(QDir::toNativeSeparators(source)));
        return -1;
    }
    // A set the program has already stays once; an id taken gets a new one
    QList<ListSet> sets = shownSets();
    QSet<QString> ids;
    for (const ListSet& set : list_sets::all(&m_book)) {
        ids.insert(set.id);
    }
    for (const ListSet& set : std::as_const(sets)) {
        ids.insert(set.id);
    }
    int added = 0;
    for (ListSet set : *imported) {
        const bool known = std::any_of(sets.cbegin(), sets.cend(), [&set](const ListSet& old) {
            return old.name == set.name && old.definition == set.definition;
        });
        if (known) {
            continue;
        }
        if (ids.contains(set.id)) {
            set.id = list_sets::newId();
        }
        ids.insert(set.id);
        sets << set;
        ++added;
    }
    showSets(sets);
    return added;
}

}  // namespace kalahari::gui
