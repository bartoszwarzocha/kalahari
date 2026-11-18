// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_choice.h
/// @brief What a new text for publishers is in Add Element: its part and its template

#ifndef KALAHARI_GUI_DIALOGS_PUBLISHING_CHOICE_H
#define KALAHARI_GUI_DIALOGS_PUBLISHING_CHOICE_H

#include "kalahari/core/publishing_texts.h"

#include <QGroupBox>
#include <QHash>
#include <QList>
#include <QString>

class QButtonGroup;
class QComboBox;
class QLabel;

namespace kalahari::gui::dialogs {

/// @brief The part of a new text for publishers (a sentence about the book, a synopsis; a
/// description and a tagline, which go to the cover) and the template of a text
///
/// The parts are buttons that the arrows go through; the template is chosen in a list, with
/// a note under it: the pattern of a sentence or the points of a synopsis, and the target
/// length. The part and the template chosen last are those the window starts with next time
/// (keep()).
class PublishingChoice : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The setting of the part chosen last
    static constexpr const char* PART_SETTING = "materials.publishing.addPart";
    /// @brief The setting of the template chosen last of each part: {"pitch":
    /// "kalahari.novel:when_must", "synopsis": ""}
    static constexpr const char* TEMPLATE_SETTING = "materials.publishing.addTemplates";

    /// @param templates The templates of the book (core::ProjectManager::publishingTemplates())
    /// @param cover Whether the book has the cover, which a description and a tagline go to
    /// @param parent Its parent
    PublishingChoice(QList<core::PublishingTemplate> templates, bool cover,
                     QWidget* parent = nullptr);

    /// @brief The part chosen: core::PublishingTexts::PITCH, SYNOPSIS, TAGLINE or BLURB
    [[nodiscard]] QString part() const { return m_part; }

    /// @brief Whether the part chosen is a description or a tagline, a material of the cover
    [[nodiscard]] bool coverPart() const;

    /// @brief The template chosen (core::PublishingTemplate::reference()); empty for none and
    /// for a part of the cover
    [[nodiscard]] QString templateReference() const;

    /// @brief Choose part @p part; a part it does not have changes nothing
    void setPart(const QString& part);

    /// @brief Choose template @p reference of the part chosen; empty: no template
    void setTemplate(const QString& reference);

    /// @brief Keep the part and the template chosen for the next time
    void keep() const;

    /// @brief The buttons of the parts, in their order
    [[nodiscard]] QButtonGroup* partButtons() const { return m_partButtons; }

    /// @brief The list of the templates of the part
    [[nodiscard]] QComboBox* templateBox() const { return m_templateBox; }

    /// @brief The note under the list: the pattern or the points of the template and the
    /// target, or where a description or a tagline goes
    [[nodiscard]] QLabel* note() const { return m_note; }

signals:
    /// @brief Another part was chosen
    void partChanged(const QString& part);

private:
    /// @brief The parts of the buttons: the texts, then those of the cover when the book has it
    [[nodiscard]] QStringList parts() const;
    /// @brief The templates of the part chosen in the list, the template kept for it chosen
    void fillTemplates();
    /// @brief The note about the template or the part chosen
    void updateNote();
    /// @brief The template of the list at @p index; nullptr for none
    [[nodiscard]] const core::PublishingTemplate* templateAt(int index) const;

    QList<core::PublishingTemplate> m_templates;
    bool m_cover = false;
    QString m_part;
    QHash<QString, QString> m_chosen;  ///< The template chosen in the window, by part
    QButtonGroup* m_partButtons = nullptr;
    QLabel* m_templateLabel = nullptr;
    QComboBox* m_templateBox = nullptr;
    QLabel* m_note = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_PUBLISHING_CHOICE_H
