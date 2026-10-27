/// @file quick_insert_page.cpp
/// @brief Settings page: Editor > Quick Insert
///
/// The editors follow these settings by themselves (EditorPanel subscribes to them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/quick_insert_item_dialog.h"
#include "kalahari/gui/quick_insert_items.h"

#include <QAction>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;
using editor::QuickInsertItem;
using editor::QuickInsertSource;

/// @brief The rows of the list of fragments shown at most before it scrolls
constexpr int ITEM_ROWS = 8;

/// @brief The sources of the lists, in the order of the page
constexpr std::array<const char*, 3> SOURCES = {quick_insert_keys::SOURCE_CARDS,
                                                quick_insert_keys::SOURCE_FRAGMENTS,
                                                quick_insert_keys::SOURCE_CHARACTERS};

/// @brief The sources of a stored value, in the order of the page (another order, or a source
///        the program does not know, is no change)
json sourcesValue(const json& value) {
    json sources = json::array();
    for (const char* source : SOURCES) {
        if (value.is_array() && std::find(value.begin(), value.end(), source) != value.end()) {
            sources.push_back(source);
        }
    }
    return sources;
}

/// @brief What an item inserts, in a line of the list: the paragraphs with ¶, a space that
///        cannot be seen by its code
QString shownText(const QString& text) {
    if (text.trimmed().isEmpty()) {
        QStringList codes;
        for (const QChar ch : text) {
            codes << QStringLiteral("U+%1").arg(ch.unicode(), 4, 16, QLatin1Char('0')).toUpper();
        }
        return codes.join(QLatin1Char(' '));
    }
    QString shown = text;
    shown.replace(QLatin1Char('\n'), QStringLiteral(" ¶ "));
    return shown;
}

}  // namespace

EditorQuickInsertPage::EditorQuickInsertPage(QWidget* parent)
    : SettingsPage(parent)
{
    createSources();
    createItems();
    pageLayout()->addStretch();
}

void EditorQuickInsertPage::createSources() {
    QFormLayout* form = addGroup(tr("Quick Insert"));
    QCheckBox* enabled = addCheckBox(form, tr("Open the list after @ or # at the start of a word"),
                                     quick_insert_keys::ENABLED);
    enabled->setObjectName(QStringLiteral("quickInsertEnabled"));

    // What each character opens: a column each, a row for each source
    auto* grid = new QWidget();
    grid->setObjectName(QStringLiteral("quickInsertSources"));
    auto* layout = new QGridLayout(grid);
    layout->setContentsMargins(0, 0, 0, 0);
    const std::array<const char*, 2> triggers = {"@", "#"};
    for (int column = 0; column < 2; ++column) {
        auto* heading = new QLabel(QString::fromLatin1(triggers.at(column)));
        heading->setAlignment(Qt::AlignCenter);
        QFont bold = heading->font();
        bold.setBold(true);
        heading->setFont(bold);
        layout->addWidget(heading, 0, column + 1);
    }
    const std::array<QString, 3> names = {tr("Names from the cards of the Workshop"),
                                          tr("Text fragments"), tr("Typographic characters")};
    std::array<std::array<QCheckBox*, 3>, 2> boxes{};
    for (int row = 0; row < 3; ++row) {
        auto* name = new QLabel(names.at(row));
        name->setWordWrap(true);  // the page fits a narrow window
        layout->addWidget(name, row + 1, 0);
        for (int column = 0; column < 2; ++column) {
            auto* box = new QCheckBox();
            box->setObjectName(QStringLiteral("quickInsert_%1_%2")
                                   .arg(column == 0 ? QStringLiteral("at") : QStringLiteral("hash"),
                                        QString::fromLatin1(SOURCES.at(row))));
            //: The check box of a source in the column of @ or #: "Text fragments after #"
            box->setAccessibleName(
                tr("%1 after %2").arg(names.at(row), QString::fromLatin1(triggers.at(column))));
            layout->addWidget(box, row + 1, column + 1, Qt::AlignCenter);
            boxes.at(column).at(row) = box;
        }
    }
    layout->setColumnStretch(0, 1);
    form->addRow(grid);

    const std::array<const char*, 2> keys = {quick_insert_keys::AT_SOURCES,
                                             quick_insert_keys::HASH_SOURCES};
    for (int column = 0; column < 2; ++column) {
        const std::array<QCheckBox*, 3> columnBoxes = boxes.at(column);
        const std::string key = keys.at(column);
        Binding sources;
        sources.key = [key]() { return key; };
        sources.stored = [key]() {
            return sourcesValue(core::SettingsManager::getInstance().get<json>(key));
        };
        sources.shown = [columnBoxes]() {
            json value = json::array();
            for (int row = 0; row < 3; ++row) {
                if (columnBoxes.at(row)->isChecked()) {
                    value.push_back(SOURCES.at(row));
                }
            }
            return value;
        };
        sources.show = [columnBoxes](const json& value) {
            const json shown = sourcesValue(value);
            for (int row = 0; row < 3; ++row) {
                columnBoxes.at(row)->setChecked(
                    std::find(shown.begin(), shown.end(), SOURCES.at(row)) != shown.end());
            }
        };
        sources.store = [key](const json& value) {
            core::SettingsManager::getInstance().set<json>(key, value);
        };
        bindCustom(std::move(sources));
    }

    // Off, the sources wait and keep their choices
    const auto syncSources = [enabled, grid]() { grid->setEnabled(enabled->isChecked()); };
    connect(enabled, &QCheckBox::toggled, this, syncSources);
    whenLoaded(syncSources);

    addNote(form, tr("@ or # in the middle of a word, as in an e-mail address or in C#, stays "
                     "a character. The letters typed after it narrow the list; Esc closes it "
                     "and keeps what was typed."));

    // With the keys the command has now (the user may change them)
    QLabel* commandNote = addNote(form, QString());
    const auto showCommandKeys = [commandNote]() {
        const Command* command = CommandRegistry::getInstance().getCommand("insert.quickInsert");
        const QString keys = command != nullptr && !command->shortcut.isEmpty()
            ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
            : QString();
        //: %1 is the keys of Insert > Quick Insert..., as Ctrl+Shift+W
        commandNote->setText(
            !keys.isEmpty()
                ? tr("Insert > Quick Insert... (%1) opens the list at the cursor with all of "
                     "them, also when the list does not open after @ or #.")
                      .arg(keys)
                : tr("Insert > Quick Insert... opens the list at the cursor with all of them, "
                     "also when the list does not open after @ or #."));
    };
    showCommandKeys();
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, commandNote,
            showCommandKeys);
}

void EditorQuickInsertPage::createItems() {
    QFormLayout* form = addGroup(tr("Text Fragments and Characters"));
    m_items = new QTreeWidget();
    m_items->setObjectName(QStringLiteral("quickInsertItems"));
    m_items->setAccessibleName(tr("Text fragments and characters"));
    m_items->setColumnCount(3);
    m_items->setHeaderLabels({tr("Name"), tr("Inserts"), tr("Group")});
    m_items->setRootIsDecorated(false);
    m_items->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_items->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_items->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_items->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_items->setMaximumHeight(m_items->fontMetrics().lineSpacing() * (ITEM_ROWS + 2) +
                              2 * m_items->frameWidth());

    auto* add = new QPushButton(tr("&Add..."));
    add->setObjectName(QStringLiteral("quickInsertAdd"));
    m_edit = new QPushButton(tr("&Edit..."));
    m_edit->setObjectName(QStringLiteral("quickInsertEdit"));
    m_remove = new QPushButton(tr("&Remove"));
    m_remove->setObjectName(QStringLiteral("quickInsertRemove"));
    m_restore = new QPushButton(tr("Restore &Default Characters"));
    m_restore->setObjectName(QStringLiteral("quickInsertRestore"));
    // The restore in a row of its own: the page fits a narrow window
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(add);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_remove);
    buttons->addStretch();
    auto* restoreRow = new QHBoxLayout();
    restoreRow->addWidget(m_restore);
    restoreRow->addStretch();
    auto* box = new QWidget();
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(m_items);
    column->addLayout(buttons);
    column->addLayout(restoreRow);
    form->addRow(box);
    addNote(form, tr("Type # and the first letters of a name at the start of a word to insert "
                     "its text. The names of the cards are those of the book open; they need "
                     "no list here."));

    connect(add, &QPushButton::clicked, this, &EditorQuickInsertPage::addItem);
    connect(m_edit, &QPushButton::clicked, this, &EditorQuickInsertPage::editItem);
    connect(m_remove, &QPushButton::clicked, this, &EditorQuickInsertPage::removeItems);
    connect(m_restore, &QPushButton::clicked, this, &EditorQuickInsertPage::restoreCharacters);
    // Enter or a double click edits the item, Delete removes the selected ones
    connect(m_items, &QTreeWidget::itemActivated, this, &EditorQuickInsertPage::editItem);
    auto* removeKey = new QAction(m_items);
    removeKey->setShortcut(QKeySequence(Qt::Key_Delete));
    removeKey->setShortcutContext(Qt::WidgetShortcut);
    m_items->addAction(removeKey);
    connect(removeKey, &QAction::triggered, this, &EditorQuickInsertPage::removeItems);
    connect(m_items, &QTreeWidget::itemSelectionChanged, this,
            &EditorQuickInsertPage::updateItemButtons);

    Binding items;
    items.key = []() { return std::string(quick_insert_keys::ITEMS); };
    items.stored = []() { return quickInsertFragmentsValue(quickInsertFragments()); };
    items.shown = [this]() { return quickInsertFragmentsValue(listedItems()); };
    items.show = [this](const json& value) { showItems(quickInsertFragmentsOf(value)); };
    items.store = [](const json& value) {
        setQuickInsertFragments(quickInsertFragmentsOf(value));
    };
    bindCustom(std::move(items));
}

QList<QuickInsertItem> EditorQuickInsertPage::listedItems() const {
    QList<QuickInsertItem> items;
    for (int row = 0; row < m_items->topLevelItemCount(); ++row) {
        const QTreeWidgetItem* item = m_items->topLevelItem(row);
        items.append(fragmentItem(
            static_cast<QuickInsertSource>(item->data(0, Qt::UserRole + 1).toInt()),
            item->text(0), item->data(0, Qt::UserRole).toString()));
    }
    return items;
}

void EditorQuickInsertPage::showItems(const QList<QuickInsertItem>& items) {
    m_items->clear();
    for (const QuickInsertItem& item : items) {
        setRow(new QTreeWidgetItem(m_items), item);
    }
    if (m_items->topLevelItemCount() > 0) {
        m_items->setCurrentItem(m_items->topLevelItem(0), 0, QItemSelectionModel::NoUpdate);
    }
    updateItemButtons();
}

void EditorQuickInsertPage::setRow(QTreeWidgetItem* row, const QuickInsertItem& item) {
    row->setText(0, item.name);
    row->setText(1, shownText(item.text));
    row->setText(2, item.group);
    row->setToolTip(1, item.text);
    row->setData(0, Qt::UserRole, item.text);
    row->setData(0, Qt::UserRole + 1, static_cast<int>(item.source));
}

void EditorQuickInsertPage::addItem() {
    QuickInsertItem item;
    item.source = QuickInsertSource::Fragment;
    dialogs::QuickInsertItemDialog dialog(item, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    auto* row = new QTreeWidgetItem(m_items);
    setRow(row, dialog.item());
    m_items->setCurrentItem(row);
    updateItemButtons();
}

void EditorQuickInsertPage::editItem() {
    QTreeWidgetItem* row = m_items->currentItem();
    if (row == nullptr) {
        return;
    }
    const QuickInsertItem item =
        fragmentItem(static_cast<QuickInsertSource>(row->data(0, Qt::UserRole + 1).toInt()),
                     row->text(0), row->data(0, Qt::UserRole).toString());
    dialogs::QuickInsertItemDialog dialog(item, this);
    if (dialog.exec() == QDialog::Accepted) {
        setRow(row, dialog.item());
        updateItemButtons();
    }
}

void EditorQuickInsertPage::removeItems() {
    const QList<QTreeWidgetItem*> selected = m_items->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    // The keys stay in the list, on the row after the last one removed
    const int next = m_items->indexOfTopLevelItem(selected.last()) + 1 - selected.size();
    qDeleteAll(selected);
    if (m_items->topLevelItemCount() > 0) {
        m_items->setCurrentItem(
            m_items->topLevelItem(std::clamp(next, 0, m_items->topLevelItemCount() - 1)));
    }
    updateItemButtons();
}

void EditorQuickInsertPage::restoreCharacters() {
    // The characters the program has at first that the list does not have, at its end
    QStringList texts;
    for (const QuickInsertItem& item : listedItems()) {
        texts << item.text;
    }
    for (const QuickInsertItem& character : defaultQuickInsertCharacters()) {
        if (!texts.contains(character.text)) {
            setRow(new QTreeWidgetItem(m_items), character);
        }
    }
    updateItemButtons();
}

void EditorQuickInsertPage::updateItemButtons() {
    const bool selected = !m_items->selectedItems().isEmpty();
    m_edit->setEnabled(m_items->currentItem() != nullptr && selected);
    m_remove->setEnabled(selected);
    QStringList texts;
    for (const QuickInsertItem& item : listedItems()) {
        texts << item.text;
    }
    bool missing = false;
    for (const QuickInsertItem& character : defaultQuickInsertCharacters()) {
        missing = missing || !texts.contains(character.text);
    }
    m_restore->setEnabled(missing);
}

}  // namespace gui
}  // namespace kalahari
