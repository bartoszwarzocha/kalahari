// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_editor.h
/// @brief The tab of a mind map: the bar of its commands, the bar of its view, the map of
/// nodes or the list, and the bar of messages under them

#ifndef KALAHARI_MINDMAP_MIND_MAP_EDITOR_H
#define KALAHARI_MINDMAP_MIND_MAP_EDITOR_H

#include <kalahari/core/window_editor_provider.h>
#include <kalahari/mindmap/mind_map.h>

#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;

namespace kalahari::mindmap {

class MindMapContent;
class MindMapDocument;
class MindMapList;
class MindMapView;

/// @brief The tab of a mind map of the Workshop
///
/// +------------------------------------------------------------------------------------+
/// | [+ Add Node... v] [branch] [below] [name] [fold] [frame] [Arrange v] [del] [<] [>] |
/// |                                                          [-] 100% [+] [whole map]  |
/// +------------------------------------------------------------------------------------+
/// | View: [Map|List]  Background: [grid v]  Lines: [curved v]  Highlight: [nothing v]  |
/// |                                    Mind map · 12 nodes · 9 links            [gear] |
/// +------------------------------------------------------------------------------------+
/// | the map of nodes, or the list                                                      |
/// +------------------------------------------------------------------------------------+
/// | Added "Amara" with its relations to the cards on the map: 2.               [Undo]  |
/// +------------------------------------------------------------------------------------+
///
/// Each change is saved to the file of the map at once. The line under the map says what was
/// done, with Undo where it can be undone; after a while, and when nothing was done, it says
/// what the selection is and what can be done with it. The tooltips of the commands name
/// their keys.
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
    [[nodiscard]] MindMapContent& content() { return *m_content; }

    /// @brief Say @p message in the bar under the map; with @p undoable it offers Undo
    void tell(const QString& message, bool undoable);

    /// @brief The text of the line under the map, for the tests
    [[nodiscard]] QString infoText() const;

    /// @brief Open the window of node @p id: its name, kind, note, rows, attributes, picture
    void openNodeWindow(const QString& id);

    /// @brief Read the book again and draw the map: the book changed
    void bookChanged();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildCommands(QWidget* bar);
    void buildViewBar(QWidget* bar);
    QToolButton* addCommand(QWidget* bar, const char* icon, const QString& name,
                            const QString& keys, const std::function<void()>& run);
    void showMode(MindMapMode mode, bool keep);
    void applyLook();
    void updateBar();
    void showHint();
    void chooseLink(const QString& id);
    void openElement(const QString& elementId);
    void syncRelationMap();
    [[nodiscard]] QString titleOf(const QString& elementId) const;

    core::ProjectManager& m_book;
    QString m_elementId;
    std::function<void()> m_openSettings;
    std::function<void(const QString&)> m_openElement;
    std::unique_ptr<MindMapContent> m_content;
    MindMapDocument* m_document = nullptr;
    MindMapView* m_view = nullptr;
    MindMapList* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_commands = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_branch = nullptr;
    QToolButton* m_below = nullptr;
    QToolButton* m_rename = nullptr;
    QToolButton* m_fold = nullptr;
    QToolButton* m_frame = nullptr;
    QToolButton* m_arrange = nullptr;
    QToolButton* m_remove = nullptr;
    QToolButton* m_undo = nullptr;
    QToolButton* m_redo = nullptr;
    QToolButton* m_mapButton = nullptr;
    QToolButton* m_listButton = nullptr;
    QComboBox* m_background = nullptr;
    QComboBox* m_lines = nullptr;
    QComboBox* m_highlight = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_what = nullptr;
    QToolButton* m_gear = nullptr;
    QWidget* m_info = nullptr;  ///< The line under the map
    QLabel* m_infoText = nullptr;
    QPushButton* m_infoUndo = nullptr;
    QTimer* m_infoTimer = nullptr;  ///< Ends a message: the line shows the hint again
    QTimer* m_bookTimer = nullptr;  ///< Reads the book again once after its changes
    int m_settingsListener = -1;
    bool m_synced = false;  ///< A map of relations took the new relations of its cards
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_EDITOR_H
