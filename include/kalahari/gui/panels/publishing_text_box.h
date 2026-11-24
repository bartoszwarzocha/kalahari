// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_text_box.h
/// @brief The group of a text for publishers in the Properties panel: its length with the
///        target, its template with the points ticked off, its state and its own target

#ifndef KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H
#define KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H

#include "kalahari/core/publishing_texts.h"

#include <QColor>
#include <QGroupBox>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <optional>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QKeyEvent;
class QLabel;
class QSpinBox;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class LengthMeter;
class PublishingTemplateEditor;

/// @brief A text for publishers beside its text on the card For Publishers
///
/// Its length against the target (with the standard pages of a synopsis), where the target
/// comes from, its template, changed in place of the points (Change Template, New
/// Template...; PublishingTemplateEditor); of a synopsis the points of the template, which the writer
/// ticks off (Space), saved with the book at once (core::PublishingData::done) or, while its
/// text has changes not saved, saved and dropped with them
/// (core::ProjectManager::setPublishingPoints()); its state, whether the set has it and its
/// own target. Hidden for another element. Esc in the points gives the keys back to the text
/// in its editor.
class PublishingTextBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit PublishingTextBox(core::ProjectManager& book, QWidget* parent = nullptr);
    ~PublishingTextBox() override;

    PublishingTextBox(const PublishingTextBox&) = delete;
    PublishingTextBox& operator=(const PublishingTextBox&) = delete;

    /// @brief Show element @p elementId when it is a text for publishers; else hide
    /// @param editor The editor of the text, whose words not yet saved it counts; nullptr:
    /// the text as the book has it
    void showText(const QString& elementId, editor::BookEditor* editor);

    /// @brief The text shown; empty: none
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief Count the text again (the editor changed it)
    void updateLength();

    /// @brief Show again what the book keeps of text @p elementId when it is the text shown:
    ///        its changes not saved were dropped, with the points waiting for them
    void showSaved(const QString& elementId);

    /// @brief The length against the target
    [[nodiscard]] LengthMeter* meter() const { return m_meter; }

    /// @brief Where the target comes from
    [[nodiscard]] QLabel* targetSourceLabel() const { return m_targetSource; }

    /// @brief The templates of its part, "No template" last
    [[nodiscard]] QComboBox* templateBox() const { return m_templateBox; }

    /// @brief Change Template: the template of the text changed in place of its points
    [[nodiscard]] QToolButton* changeTemplateButton() const { return m_changeTemplate; }

    /// @brief The template changed in place of the points (Change Template, New Template...)
    [[nodiscard]] PublishingTemplateEditor* templateEditor() const { return m_templateEditor; }

    /// @brief Change Template: the template of the text in place of its points; a new one
    ///        for a text without a template
    void changeTemplate();

    /// @brief New Template... on the list of the templates: a new template of the writer in
    ///        place of the points
    void startNewTemplate();

    /// @brief The points of its template, in order; none without a template with points
    [[nodiscard]] const QList<QCheckBox*>& points() const { return m_points; }

    /// @brief What the points or their absence mean
    [[nodiscard]] QLabel* pointsHint() const { return m_pointsHint; }

    /// @brief The volume of a series it is for, or the whole series; hidden in a book that
    /// is no series
    [[nodiscard]] QComboBox* volumeBox() const { return m_volumeBox; }

    /// @brief The state: a draft, chosen for the set, rejected
    [[nodiscard]] QComboBox* stateBox() const { return m_stateBox; }

    /// @brief Its own target length, in the unit of the cover; 0: none
    [[nodiscard]] QSpinBox* targetSpin() const { return m_targetSpin; }

    /// @brief Whether the text is the version of its part in the set of its volume
    [[nodiscard]] QLabel* inSetLabel() const { return m_inSetLabel; }

    /// @brief The editor of the text shown; nullptr: none (the text as the book has it)
    [[nodiscard]] editor::BookEditor* editor() const { return m_editor; }

    /// @brief What each point shows under its name: the fragments of the text linked to it
    ///        ("In the text: 2 fragments, 140 characters"), "Not in the text", or the warning
    ///        of a point ticked off without one
    [[nodiscard]] const QList<QLabel*>& pointStates() const { return m_pointStates; }

    /// @brief How many points the text has fragments of, and how many are ticked off
    [[nodiscard]] QLabel* pointsSummary() const { return m_pointsSummary; }

    /// @brief Show in the Text: a fill under the fragments of all the points while it is on
    [[nodiscard]] QToolButton* showInTextButton() const { return m_showInText; }

    /// @brief The gear: Settings > Navigator > Texts for Publishers (settingsRequested())
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

    /// @brief Tools > Texts for Publishers > Link to a Point: the points in a menu under the
    ///        cursor of the text, those of the selection ticked; a point chosen links the
    ///        selection to it, a ticked one takes the link away (without a selection: of the
    ///        fragment at the cursor)
    /// @return false when there was no menu: no editor, no points, or neither a selection
    ///         nor a fragment at the cursor (the status bar says why)
    bool showLinkMenu();

    /// @brief Link the selection of the text to point @p pointId (Insert in the point)
    bool linkSelection(const QString& pointId);

    /// @brief Take every fragment of point @p pointId away from it (Delete in the point); the
    ///        text stays
    bool unlinkPoint(const QString& pointId);

    /// @brief Select the next fragment of point @p pointId in the text (Enter in the point),
    ///        after the last one the first again
    bool goToNextFragment(const QString& pointId);

signals:
    /// @brief What the book keeps of the text changed here: its template, points, state,
    /// target or volume; also of another text, chosen before, that is a draft again
    void publishingChanged(const QString& elementId);

    /// @brief The gear: open Settings > Navigator > Texts for Publishers
    void settingsRequested();

    /// @brief Something to say in the status bar (Link to a Point without a selection)
    void statusMessage(const QString& text);

protected:
    /// @brief Esc in a point gives the keys back to the text
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The template of the text; nullopt for none
    [[nodiscard]] std::optional<core::PublishingTemplate> currentTemplate() const;
    /// @brief The fields after what the book keeps of the text
    void showData();
    /// @brief The templates of its part in the template box, the template of the text chosen
    void fillTemplates();
    /// @brief The points of its template, ticked off as the book has them; built again only
    /// for other points
    void showPoints();
    /// @brief The check boxes of @p points with their hints; of a template of each text
    /// (@p eachText) its hint once over them
    void buildPoints(const QList<core::PublishingTemplatePoint>& points, bool eachText);
    /// @brief The colors of the points, from the theme, in their order
    [[nodiscard]] QList<QColor> pointColors() const;
    /// @brief The editor of the text shows the fragments of its points (and the editor shown
    ///        before them no more); follows its text and its cursor
    void followEditor(editor::BookEditor* editor);
    /// @brief The fills and the bars of the fragments in the editor, after the points and the
    ///        point chosen
    void applyLook();
    /// @brief The states of the points and the summary, after the fragments in the text
    void updatePointStates();
    /// @brief The points of the cursor of the text in bold
    void showPointsAtCursor();
    /// @brief The point chosen in the list (its fragments filled stronger); empty: none
    void choosePoint(const QString& pointId);
    /// @brief The keys of a point: arrows, Home, End, Enter, Insert, Delete, Esc
    bool pointKey(qsizetype index, const QKeyEvent& event);
    /// @brief Save @p publishing, the data of the text, with the book; false when it was not
    ///        saved
    bool save(const core::PublishingData& publishing);
    void saveTemplate();
    /// @brief The template edited in place of the points: @p templ
    void editTemplate(const core::PublishingTemplate& templ);
    /// @brief Keep the template edited as the writer's own and give it to the text
    void keepTemplate();
    /// @brief The points again in place of the template edited
    void stopEditingTemplate();
    void savePoint(const QString& pointId, bool done);
    void saveState();
    void saveTarget();
    /// @brief The volume of the text in its box, shown in a series
    void showVolume(const core::ProjectElement& element);
    /// @brief Save the volume chosen in its box (core::ProjectManager::setMaterialBook())
    void saveVolume();

    core::ProjectManager& m_book;
    QString m_elementId;
    core::PublishingData m_data;  ///< What the book keeps of the text shown
    QPointer<editor::BookEditor> m_editor;  ///< The editor of the text; none: as saved
    LengthMeter* m_meter = nullptr;
    QLabel* m_targetSource = nullptr;
    QComboBox* m_templateBox = nullptr;
    QToolButton* m_changeTemplate = nullptr;  ///< The template in place of the points
    PublishingTemplateEditor* m_templateEditor = nullptr;
    QWidget* m_pointsArea = nullptr;
    QVBoxLayout* m_pointsLayout = nullptr;
    QList<QCheckBox*> m_points;
    QStringList m_pointIds;  ///< The ids of the points shown, in order
    QList<core::PublishingTemplatePoint> m_templatePoints;  ///< The points shown
    QList<QLabel*> m_pointStates;     ///< Under each point: its fragments in the text
    QList<QLabel*> m_pointSwatches;   ///< Before each point: its number in its color
    QList<QToolButton*> m_linkButtons;    ///< Of each point: link the selection to it
    QList<QToolButton*> m_unlinkButtons;  ///< Of each point: take its fragments away
    QWidget* m_pointsHeader = nullptr;    ///< The summary, Show in the Text and the gear
    QLabel* m_pointsSummary = nullptr;
    QToolButton* m_showInText = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QString m_chosenPoint;  ///< The point chosen in the list: its fragments filled stronger
    QHash<QString, int> m_nextFragment;  ///< Of each point: the fragment Enter selects next
    QPointer<editor::BookEditor> m_lookEditor;  ///< The editor that shows the fragments
    QList<QMetaObject::Connection> m_editorConnections;  ///< To m_lookEditor
    QLabel* m_pointsHint = nullptr;
    QFormLayout* m_fields = nullptr;  ///< The volume, the state, the target and the set
    QComboBox* m_volumeBox = nullptr;
    QComboBox* m_stateBox = nullptr;
    QSpinBox* m_targetSpin = nullptr;
    QLabel* m_targetUnit = nullptr;
    QLabel* m_inSetLabel = nullptr;
    int m_settingsListener = -1;  ///< The targets and the writer's templates changed
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H
