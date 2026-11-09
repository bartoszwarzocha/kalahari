/// @file paragraph_marks_coordinator.h
/// @brief The paperclips beside the paragraphs of the chapters that materials are linked with

#pragma once

#include <kalahari/editor/editor_types.h>

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QTabWidget;
class QTimer;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class EditorPanel;

/// @brief Shows a paperclip beside each paragraph of an open chapter that a material is
/// linked with (core::ElementLink::paragraph)
///
/// The paragraph of a link is found in the text of the editor (core::LinkRoles::paragraphOf()),
/// also when paragraphs were added or taken out before it. A click on the paperclip, or its
/// item in the context menu of the paragraph, shows the material in the Properties panel.
/// The paperclips follow the links, the setting of whether links name paragraphs
/// (core::LinkRoles::PARAGRAPHS_SETTING), a chapter that opens and the book.
class ParagraphMarksCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Shows a material in the Properties panel and brings the panel to the front
    using MaterialShower = std::function<void(const QString& materialId)>;

    /// @brief Constructor
    /// @param centralTabs The tabs of the documents
    /// @param showMaterial What a chosen material does
    /// @param parent Parent object
    ParagraphMarksCoordinator(QTabWidget* centralTabs, MaterialShower showMaterial,
                              QObject* parent = nullptr);

    /// @brief Destructor
    ~ParagraphMarksCoordinator() override;

    ParagraphMarksCoordinator(const ParagraphMarksCoordinator&) = delete;
    ParagraphMarksCoordinator& operator=(const ParagraphMarksCoordinator&) = delete;

    /// @brief The paperclips of chapter @p elementId in @p editor: one for each paragraph
    /// that materials are linked with, naming them in the order of the Workshop; none when
    /// links name no paragraphs (the setting)
    static QList<editor::ParagraphMark> marksOf(const QString& elementId,
                                                const editor::BookEditor& editor);

    /// @brief Give every open chapter its paperclips again
    void refresh();

private:
    /// @brief Give the chapter of @p panel its paperclips again
    void refreshPanel(EditorPanel* panel);

    QTabWidget* m_centralTabs;      ///< Not owned
    MaterialShower m_showMaterial;  ///< What a chosen material does
    QTimer* m_refreshTimer;         ///< refresh() once for many changes
    int m_settingsListener = 0;     ///< core::SettingsManager::subscribe() id
};

}  // namespace kalahari::gui
