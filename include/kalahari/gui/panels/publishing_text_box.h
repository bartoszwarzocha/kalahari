// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_text_box.h
/// @brief The group of a text for publishers in the Properties panel: its length with the
///        target, its template with the points ticked off, its state and its own target

#ifndef KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H
#define KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H

#include "kalahari/core/publishing_texts.h"

#include <QGroupBox>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class LengthMeter;

/// @brief A text for publishers in the Properties panel, beside the page of its element
///
/// Its length against the target (with the standard pages of a synopsis), where the target
/// comes from, its template; of a synopsis the points of the template, which the writer
/// ticks off (Space), saved with the book at once (core::PublishingData::done); its state,
/// whether the set has it and its own target. Hidden for another element. Esc in the points
/// gives the keys back to the text in its editor.
class PublishingTextBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit PublishingTextBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show element @p elementId when it is a text for publishers; else hide
    /// @param editor The editor of the text, whose words not yet saved it counts; nullptr:
    /// the text as the book has it
    void showText(const QString& elementId, editor::BookEditor* editor);

    /// @brief The text shown; empty: none
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief Count the text again (the editor changed it)
    void updateLength();

    /// @brief The length against the target
    [[nodiscard]] LengthMeter* meter() const { return m_meter; }

    /// @brief Where the target comes from
    [[nodiscard]] QLabel* targetSourceLabel() const { return m_targetSource; }

    /// @brief The templates of its part, "No template" last
    [[nodiscard]] QComboBox* templateBox() const { return m_templateBox; }

    /// @brief The points of its template, in order; none without a template with points
    [[nodiscard]] const QList<QCheckBox*>& points() const { return m_points; }

    /// @brief What the points or their absence mean
    [[nodiscard]] QLabel* pointsHint() const { return m_pointsHint; }

    /// @brief The state: a draft, chosen for the set, rejected
    [[nodiscard]] QComboBox* stateBox() const { return m_stateBox; }

    /// @brief Its own target length, in the unit of the cover; 0: none
    [[nodiscard]] QSpinBox* targetSpin() const { return m_targetSpin; }

    /// @brief Whether the text is the version of its part in the set
    [[nodiscard]] QLabel* inSetLabel() const { return m_inSetLabel; }

signals:
    /// @brief What the book keeps of the text changed here: its template, points, state or
    /// target
    void publishingChanged(const QString& elementId);

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
    /// @brief Save @p publishing, the data of the text, with the book; false when it was not
    ///        saved
    bool save(const core::PublishingData& publishing);
    void saveTemplate();
    void savePoint(const QString& pointId, bool done);
    void saveState();
    void saveTarget();

    core::ProjectManager& m_book;
    QString m_elementId;
    core::PublishingData m_data;  ///< What the book keeps of the text shown
    QPointer<editor::BookEditor> m_editor;  ///< The editor of the text; none: as saved
    LengthMeter* m_meter = nullptr;
    QLabel* m_targetSource = nullptr;
    QComboBox* m_templateBox = nullptr;
    QWidget* m_pointsArea = nullptr;
    QVBoxLayout* m_pointsLayout = nullptr;
    QList<QCheckBox*> m_points;
    QStringList m_pointIds;  ///< The ids of the points shown, in order
    QLabel* m_pointsHint = nullptr;
    QComboBox* m_stateBox = nullptr;
    QSpinBox* m_targetSpin = nullptr;
    QLabel* m_targetUnit = nullptr;
    QLabel* m_inSetLabel = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PUBLISHING_TEXT_BOX_H
