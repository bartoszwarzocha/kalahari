// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_menus.h
/// @brief What the map and the list of a mind map share: the menus of the markers and of the
/// colors, copying, pasting and deleting with their messages

#ifndef KALAHARI_MINDMAP_MIND_MAP_MENUS_H
#define KALAHARI_MINDMAP_MIND_MAP_MENUS_H

#include <kalahari/mindmap/mind_map_look.h>

#include <QCoreApplication>
#include <QString>
#include <QStringList>

class QColor;
class QMenu;
class QPainter;
class QRectF;

namespace kalahari::mindmap {

class MindMapDocument;

/// @brief The parts of the menus and the changes that the map and the list share
class MindMapMenus {
    Q_DECLARE_TR_FUNCTIONS(MindMapMenus)

public:
    /// @brief The menu of the markers of node @p id
    static void addMarkers(QMenu& menu, MindMapDocument& document, const MindMapLook& look,
                           const QString& id);

    /// @brief The menu of the color of the branch of node @p id
    static void addColors(QMenu& menu, MindMapDocument& document, const MindMapLook& look,
                          const QString& id);

    /// @brief Delete node @p id with its branch
    /// @param message Gets what to tell
    /// @return Whether it went, so that Undo can take it back
    static bool remove(MindMapDocument& document, const QString& id, QString* message);

    /// @brief Copy node @p id with its branch as an outline
    /// @return What to tell
    static QString copy(const MindMapDocument& document, const QString& id);

    /// @brief Paste the text of the clipboard inside node @p parent: an outline, or one node
    /// (Settings > Mind Maps)
    /// @param message Gets what to tell
    /// @return The nodes added at the top
    static QStringList paste(MindMapDocument& document, const QString& parent,
                             QString* message);

    /// @brief Whether the clipboard has text to paste
    [[nodiscard]] static bool canPaste();

    /// @brief The icon of a link: two rings of a chain, across the square @p rect
    static void paintLinkIcon(QPainter& painter, const QRectF& rect, const QColor& color);

    /// @brief The icon of a note: a sheet with lines, in the square @p rect
    static void paintNoteIcon(QPainter& painter, const QRectF& rect, const QColor& color);
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_MENUS_H
