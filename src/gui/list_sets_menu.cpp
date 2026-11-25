// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file list_sets_menu.cpp
/// @brief Format > List Sets and the arrows of the buttons Numbering and Bullets

#include "kalahari/gui/list_sets_menu.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/list_sets.h"
#include "kalahari/gui/widgets/list_set_tile.h"

#include <QGridLayout>
#include <QShowEvent>
#include <QTimer>
#include <QWidgetAction>

#include <optional>

namespace kalahari::gui {

using editor::ListKind;

namespace {

/// @brief The tiles in a row of the grid
constexpr int TILE_COLUMNS = 3;

/// @brief Room around the grid and between its tiles
constexpr int TILE_MARGIN = 6;

/// @brief The text of an item of a menu: its "&" shown, not a key
QString menuText(const QString& text)
{
    QString escaped = text;
    return escaped.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

}  // namespace

ListSetsMenu::ListSetsMenu(Kind kind, CommandRegistry& commands, EditorInFront editorInFront,
                           BookInFront bookInFront, QWidget* parent)
    : QMenu(parent)
    , m_kind(kind)
    , m_commands(commands)
    , m_editorInFront(std::move(editorInFront))
    , m_bookInFront(std::move(bookInFront))
{
    setObjectName(kind == Kind::All        ? QStringLiteral("listSetsMenu")
                  : kind == Kind::Numbered ? QStringLiteral("numberingSetsMenu")
                                           : QStringLiteral("bulletSetsMenu"));
    connect(this, &QMenu::aboutToShow, this, &ListSetsMenu::rebuild);
    // The keys come back to the tiles from the items under them (the arrow up)
    connect(this, &QMenu::hovered, this, [this](const QAction* action) {
        if (action != nullptr && action == m_tilesAction && m_tilesGrid != nullptr &&
            !m_tilesGrid->isAncestorOf(QWidget::focusWidget())) {
            focusTile();
        }
    });
    rebuild();
}

void ListSetsMenu::rebuild()
{
    clear();
    m_setActions.clear();
    m_tiles.clear();
    m_tilesAction = nullptr;
    m_tilesGrid = nullptr;

    editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    const bool enabled = text != nullptr && !text->isReadOnly();
    const core::ProjectManager* book = m_bookInFront ? m_bookInFront() : nullptr;
    QList<ListSet> sets;
    for (const ListSet& set : list_sets::all(book)) {
        if (m_kind == Kind::All ||
            (set.kind() == ListKind::Bulleted) == (m_kind == Kind::Bulleted)) {
            sets.append(set);
        }
    }
    QString checked;
    if (text != nullptr) {
        if (const std::optional<editor::ListItem> item = text->currentListItem()) {
            if (const std::optional<ListSet> set = list_sets::matching(item->definition, sets)) {
                checked = set->id;
            }
        }
    }

    if (m_kind != Kind::All) {
        addTiles(sets, checked, enabled);
        addSeparator();
        if (m_kind == Kind::Numbered) {
            addNumberingItems(text, enabled);
            addSeparator();
        }
        if (QAction* dialog = m_commands.getAction(QStringLiteral("format.bulletsNumbering"))) {
            addAction(dialog);
        }
        return;
    }

    const QString numberedDefault = list_sets::defaultId(ListKind::Numbered);
    const QString bulletedDefault = list_sets::defaultId(ListKind::Bulleted);
    QString group;
    for (const ListSet& set : std::as_const(sets)) {
        if (const QString title = list_sets::groupTitle(set); title != group) {
            addSection(title);
            group = title;
        }
        QString label = menuText(set.displayName());
        if (set.id == (set.kind() == ListKind::Bulleted ? bulletedDefault : numberedDefault)) {
            //: By the default set of its kind in Format > List Sets. In Polish: domyślny
            label += QLatin1Char('\t') + tr("default");
        }
        QAction* action = addAction(label);
        action->setData(set.id);
        action->setCheckable(true);
        action->setChecked(set.id == checked);
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, [this, id = set.id]() { chooseSet(id); });
        m_setActions.append(action);
    }
}

void ListSetsMenu::addTiles(const QList<ListSet>& sets, const QString& checked, bool enabled)
{
    m_tilesGrid = new QWidget(this);
    auto* grid = new QGridLayout(m_tilesGrid);
    grid->setContentsMargins(TILE_MARGIN, TILE_MARGIN, TILE_MARGIN, TILE_MARGIN);
    grid->setSpacing(TILE_MARGIN);
    const QString defaultId =
        list_sets::defaultId(m_kind == Kind::Bulleted ? ListKind::Bulleted : ListKind::Numbered);
    int index = 0;
    for (const ListSet& set : sets) {
        auto* tile = new ListSetTile(set, set.id == defaultId, m_tilesGrid);
        tile->setChecked(set.id == checked);
        tile->setEnabled(enabled);
        tile->setClickOnEnter(true);
        tile->setMove([this](const ListSetTile* from, int key) { moveFrom(from, key); });
        connect(tile, &QAbstractButton::clicked, this, [this, id = set.id]() {
            // The menu goes first: the keys go back to the text the set goes on
            hide();
            chooseSet(id);
        });
        grid->addWidget(tile, index / TILE_COLUMNS, index % TILE_COLUMNS);
        m_tiles.append(tile);
        ++index;
    }
    m_tilesAction = new QWidgetAction(this);
    m_tilesAction->setDefaultWidget(m_tilesGrid);
    addAction(m_tilesAction);
}

void ListSetsMenu::addNumberingItems(const editor::BookEditor* text, bool enabled)
{
    const std::optional<editor::ListItem> item =
        text != nullptr ? text->currentListItem() : std::nullopt;
    const bool numbered = item && !item->levelFormat().isBullet();
    const editor::ListLevel level = numbered ? item->levelFormat() : editor::ListLevel{};
    //: %1 is the first number of the level of the list item: 1, a, i. In Polish: Zacznij numerowanie &od %1
    QAction* restart = addAction(
        tr("&Restart at %1").arg(editor::listNumberText(level.start, level.format)), this,
        [this]() {
            if (editor::BookEditor* editor = m_editorInFront ? m_editorInFront() : nullptr) {
                editor->restartNumbering();
                emit setChosen(QString());
            }
        });
    restart->setEnabled(enabled && numbered &&
                        (!item->numbered || text->currentListNumber() != level.start));
    //: In Polish: &Kontynuuj numerowanie
    QAction* goOn = addAction(tr("&Continue Numbering"), this, [this]() {
        if (editor::BookEditor* editor = m_editorInFront ? m_editorInFront() : nullptr) {
            editor->continueNumbering();
            emit setChosen(QString());
        }
    });
    goOn->setEnabled(enabled && numbered && text->canContinueNumbering());
}

QString ListSetsMenu::checkedSet() const
{
    for (const QAction* action : m_setActions) {
        if (action->isChecked()) {
            return action->data().toString();
        }
    }
    for (const ListSetTile* tile : m_tiles) {
        if (tile->isChecked()) {
            return tile->set().id;
        }
    }
    return {};
}

bool ListSetsMenu::chooseSet(const QString& id)
{
    editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    if (text == nullptr || text->isReadOnly()) {
        return false;
    }
    const std::optional<ListSet> set =
        list_sets::find(id, m_bookInFront ? m_bookInFront() : nullptr);
    if (!set) {
        return false;
    }
    text->applyListDefinition(set->definition);
    emit setChosen(id);
    return true;
}

void ListSetsMenu::showEvent(QShowEvent* event)
{
    QMenu::showEvent(event);
    if (m_tilesAction != nullptr) {
        // Once the menu is up: the arrows go over the tiles right away
        QTimer::singleShot(0, this, [this]() {
            if (isVisible() && m_tilesAction != nullptr) {
                setActiveAction(m_tilesAction);
                focusTile();
            }
        });
    }
}

void ListSetsMenu::focusTile()
{
    ListSetTile* target = nullptr;
    for (ListSetTile* tile : std::as_const(m_tiles)) {
        if (tile->isEnabled() && (target == nullptr || tile->isChecked())) {
            target = tile;
        }
    }
    if (target != nullptr) {
        target->setFocus(Qt::TabFocusReason);
    }
}

void ListSetsMenu::moveFrom(const ListSetTile* tile, int key)
{
    if (ListSetTile* next = ListSetTile::nearest(m_tiles, tile, key, m_tilesGrid)) {
        next->setFocus(Qt::TabFocusReason);
        return;
    }
    if (key != Qt::Key_Down) {
        return;
    }
    // Under the last row: the first item under the tiles that can be given
    const QList<QAction*> items = actions();
    for (qsizetype i = items.indexOf(m_tilesAction) + 1; i < items.size(); ++i) {
        if (!items[i]->isSeparator() && items[i]->isEnabled() && items[i]->isVisible()) {
            setFocus(Qt::TabFocusReason);
            setActiveAction(items[i]);
            return;
        }
    }
}

}  // namespace kalahari::gui
