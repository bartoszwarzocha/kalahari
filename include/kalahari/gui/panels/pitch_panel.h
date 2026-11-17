// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pitch_panel.h
/// @brief The card of a sentence about the book on a tab of its own: its template and the
///        fields of its pattern, the sentence in the editor under them, its length and state

#ifndef KALAHARI_GUI_PANELS_PITCH_PANEL_H
#define KALAHARI_GUI_PANELS_PITCH_PANEL_H

#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace kalahari::gui {

class FlowLayout;
class LengthMeter;

/// @brief The card of a sentence about the book, a text for publishers, on a tab beside the
/// chapters
///
/// It is the editor of the sentence (EditorPanel): the tab and the Navigator show a "*" while
/// the sentence is not saved, and Save saves it. Above the sentence are the template and the
/// fields of its pattern: the program composes the sentence of them until the writer changes
/// it by hand (core::PublishingData::manual); Compose from the Fields composes it again. Under
/// the sentence are its length with the target, its own target and its state. The template,
/// the fields, the target and the state are saved with the book at once.
class PitchPanel : public EditorPanel {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param elementId The sentence
    /// @param parent Parent widget
    PitchPanel(core::ProjectManager& book, CommandRegistry& commands, const QString& elementId,
               QWidget* parent = nullptr);

    /// @brief The sentence
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief The title of the sentence
    [[nodiscard]] QLineEdit* titleEdit() const { return m_titleEdit; }

    /// @brief The templates of a sentence, "No template" last
    [[nodiscard]] QComboBox* templateBox() const { return m_templateBox; }

    /// @brief The field of the pattern @p fieldId; nullptr when the pattern has none
    [[nodiscard]] QLineEdit* field(const QString& fieldId) const;

    /// @brief The note of a sentence changed by hand, with Compose from the Fields
    [[nodiscard]] QWidget* manualNote() const { return m_manualNote; }

    /// @brief Compose from the Fields
    [[nodiscard]] QPushButton* composeButton() const { return m_composeButton; }

    /// @brief The length against the target
    [[nodiscard]] LengthMeter* meter() const { return m_meter; }

    /// @brief The own target length of the sentence, in the unit of the cover; 0: none
    [[nodiscard]] QSpinBox* targetSpin() const { return m_targetSpin; }

    /// @brief The state: a draft, chosen for the set, rejected
    [[nodiscard]] QComboBox* stateBox() const { return m_stateBox; }

    /// @brief Whether the sentence is the version of its part in the set
    [[nodiscard]] QLabel* inSetLabel() const { return m_inSetLabel; }

    /// @brief Save the fields typed and not yet saved (they are saved a moment after typing)
    void saveFields();

signals:
    /// @brief The title of the sentence changed
    void titleChanged(const QString& elementId);

    /// @brief What the book keeps of the sentence changed: its template, its state, its target
    void publishingChanged(const QString& elementId);

private:
    /// @brief The header, the pattern, the sentence with its length, and the data
    void buildCard();
    QWidget* createPattern(QWidget* parent);
    QWidget* createSentence(QWidget* parent);
    QWidget* createData(QWidget* parent);
    /// @brief The fields of the pattern of the template, with the words between them
    void buildFields();
    /// @brief The widgets after what the book keeps of the sentence, the field with the keys
    /// left as it is
    void showData();
    /// @brief The length of the sentence, the note of a sentence changed by hand
    void updateLength();
    /// @brief The template of the sentence; nullopt for none
    [[nodiscard]] std::optional<core::PublishingTemplate> currentTemplate() const;
    /// @brief The sentence of the pattern and the fields; empty while no field has a value
    [[nodiscard]] QString composed() const;
    /// @brief Put the sentence composed of the fields in the editor
    void compose();
    /// @brief The text of the editor changed: changed by hand, unless it is the sentence
    /// composed
    void onTextChanged();
    /// @brief Save @p data of the sentence with the book; false when it was not saved
    bool save(const core::PublishingData& data);
    void saveTitle();
    void saveTemplate();
    void saveTarget();
    void saveState();

    core::ProjectManager& m_book;
    QString m_elementId;
    core::PublishingData m_data;  ///< What the book keeps, with the fields typed
    bool m_composing = false;     ///< The program puts the sentence in the editor
    QLineEdit* m_titleEdit = nullptr;
    QComboBox* m_templateBox = nullptr;
    QWidget* m_patternArea = nullptr;
    FlowLayout* m_patternLayout = nullptr;
    QLabel* m_patternHint = nullptr;
    QHash<QString, QLineEdit*> m_fields;
    QVBoxLayout* m_sentenceLayout = nullptr;  ///< The heading, the editor, the length...
    QWidget* m_manualNote = nullptr;
    QPushButton* m_composeButton = nullptr;
    LengthMeter* m_meter = nullptr;
    QLabel* m_targetSource = nullptr;
    QSpinBox* m_targetSpin = nullptr;
    QLabel* m_targetUnit = nullptr;
    QComboBox* m_stateBox = nullptr;
    QLabel* m_inSetLabel = nullptr;
    QTimer* m_saveTimer = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PITCH_PANEL_H
