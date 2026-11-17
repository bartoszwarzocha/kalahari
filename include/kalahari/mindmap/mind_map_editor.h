// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_editor.h
/// @brief The tab of a mind map: its bar, the map or the list, and the bar of messages under
/// them

#ifndef KALAHARI_MINDMAP_MIND_MAP_EDITOR_H
#define KALAHARI_MINDMAP_MIND_MAP_EDITOR_H

#include <kalahari/core/window_editor_provider.h>
#include <kalahari/mindmap/mind_map.h>

#include <QString>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QStackedWidget;
class QTimer;
class QToolButton;

namespace kalahari::gui {
class InfoBar;
}  // namespace kalahari::gui

namespace kalahari::mindmap {

class MindMapDocument;
class MindMapList;
class MindMapView;

/// @brief The tab of a mind map of the Workshop
///
/// +----------------------------------------------------------------------------------+
/// | View: [Map|List]  Layout: [Both sides v]  Highlight: [nothing v]  [-] 100% [+]   |
/// |                                                 12 nodes · 2 connections  [gear] |
/// +----------------------------------------------------------------------------------+
/// | the map, or the list                                                             |
/// +----------------------------------------------------------------------------------+
/// | [i] Deleted "Kompas" with its branch: 3 nodes.                    [Undo]  [X]    |
/// +----------------------------------------------------------------------------------+
///
/// Each change is saved to the file of the map at once. The bar under the map says what was
/// done, with Undo where it can be undone.
class MindMapEditor : public QWidget {
    Q_OBJECT

public:
    /// @brief The tab of element @p context.elementId
    MindMapEditor(const core::WindowEditorContext& context, QWidget* parent = nullptr);
    ~MindMapEditor() override;

    MindMapEditor(const MindMapEditor&) = delete;
    MindMapEditor& operator=(const MindMapEditor&) = delete;
    MindMapEditor(MindMapEditor&&) = delete;
    MindMapEditor& operator=(MindMapEditor&&) = delete;

    [[nodiscard]] MindMapDocument& document() { return *m_document; }
    [[nodiscard]] MindMapView* view() const { return m_view; }
    [[nodiscard]] MindMapList* list() const { return m_list; }

    /// @brief Say @p message in the bar under the map; with @p undoable it offers Undo
    void tell(const QString& message, bool undoable);

private:
    void buildBar(QWidget* bar);
    void showMode(MindMapMode mode, bool keep);
    void applyLook();
    void updateBar();
    void editNote(const QString& id);
    void chooseLink(const QString& id);
    void openElement(const QString& elementId);
    [[nodiscard]] QString titleOf(const QString& elementId) const;

    core::ProjectManager& m_book;
    QString m_elementId;
    std::function<void()> m_openSettings;
    std::function<void(const QString&)> m_openElement;
    MindMapDocument* m_document = nullptr;
    MindMapView* m_view = nullptr;
    MindMapList* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QToolButton* m_mapButton = nullptr;
    QToolButton* m_listButton = nullptr;
    QComboBox* m_layout = nullptr;
    QComboBox* m_highlight = nullptr;
    QWidget* m_zoomBox = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_counts = nullptr;
    QToolButton* m_gear = nullptr;
    gui::InfoBar* m_info = nullptr;
    QTimer* m_infoTimer = nullptr;
    int m_settingsListener = -1;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_EDITOR_H
