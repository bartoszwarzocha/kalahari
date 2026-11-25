// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_template_editor.h
/// @brief The template of a text for publishers changed in place, beside the text: its name,
///        its target length, the points of a synopsis with their hints or the pattern of a
///        sentence with its fields

#ifndef KALAHARI_GUI_WIDGETS_PUBLISHING_TEMPLATE_EDITOR_H
#define KALAHARI_GUI_WIDGETS_PUBLISHING_TEMPLATE_EDITOR_H

#include "kalahari/core/publishing_texts.h"

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QToolButton;

namespace kalahari::gui {

/// @brief The fields of a template in place of its points (Change Template)
///
/// A template of a book type does not change: what the writer changes is kept as their own
/// template, in all books (keptTemplate()). In the points: Insert adds a point after the one
/// chosen, Delete removes it, Alt+Up and Alt+Down move it. Ctrl+Enter keeps the changes, Esc
/// drops them.
class PublishingTemplateEditor : public QWidget {
    Q_OBJECT

public:
    /// @param parent Parent widget
    explicit PublishingTemplateEditor(QWidget* parent = nullptr);
    ~PublishingTemplateEditor() override;

    PublishingTemplateEditor(const PublishingTemplateEditor&) = delete;
    PublishingTemplateEditor& operator=(const PublishingTemplateEditor&) = delete;

    /// @brief Show @p templ to be changed; its name gets the keys
    /// @param words The target length in words, else in characters with spaces
    void edit(const core::PublishingTemplate& templ, bool words);

    /// @brief The template as the fields have it: a template of a book type stays a template
    ///        of its package, with the texts of the fields in the program's language
    [[nodiscard]] core::PublishingTemplate edited() const;

    /// @brief The template edited as the writer's own one among @p own (their templates): the
    ///        same one for their own template, else a new one with a new id, for all books,
    ///        named as no other of them
    [[nodiscard]] core::PublishingTemplate keptTemplate(
        const QList<core::PublishingTemplate>& own) const;

    /// @brief A new template of the writer for texts of @p part, with one point for a synopsis
    ///        and the target length of the Settings, named as no other of @p own
    [[nodiscard]] static core::PublishingTemplate newTemplate(
        const QString& part, const QList<core::PublishingTemplate>& own);

    /// @brief Add a point after the one chosen (Insert)
    void addPoint();
    /// @brief Remove the point chosen (Delete)
    void removePoint();
    /// @brief Move the point chosen by @p step places (Alt+Up, Alt+Down)
    void movePoint(int step);

    [[nodiscard]] QLineEdit* nameEdit() const { return m_nameEdit; }
    [[nodiscard]] QSpinBox* targetSpin() const { return m_targetSpin; }
    [[nodiscard]] QTableWidget* pointsTable() const { return m_pointsTable; }
    [[nodiscard]] QLineEdit* patternEdit() const { return m_patternEdit; }
    [[nodiscard]] QTableWidget* fieldsTable() const { return m_fieldsTable; }
    [[nodiscard]] QLabel* noteLabel() const { return m_note; }
    [[nodiscard]] QPushButton* keepButton() const { return m_keepButton; }
    [[nodiscard]] QPushButton* dropButton() const { return m_dropButton; }

signals:
    /// @brief Keep (Ctrl+Enter): the changes are to be kept
    void kept();
    /// @brief Drop (Esc): the changes are dropped
    void dropped();

protected:
    /// @brief Ctrl+Enter and Esc stay with the editor, whatever command has them
    bool event(QEvent* event) override;
    /// @brief Ctrl+Enter keeps the changes, Esc drops them
    void keyPressEvent(QKeyEvent* event) override;
    /// @brief The keys of the points
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The fields of the pattern, after the pattern: those it had keep their texts
    void updateFields();
    /// @brief The buttons of the points enabled for the point chosen
    void updatePointButtons();
    /// @brief A row of the points: @p name and @p hint
    void insertPointRow(int row, const QString& name, const QString& hint);

    core::PublishingTemplate m_template;  ///< As it was shown
    bool m_words = false;
    QLabel* m_note = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QSpinBox* m_targetSpin = nullptr;
    QLabel* m_targetUnit = nullptr;
    QWidget* m_pointsArea = nullptr;
    QTableWidget* m_pointsTable = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QWidget* m_patternArea = nullptr;
    QLineEdit* m_patternEdit = nullptr;
    QTableWidget* m_fieldsTable = nullptr;
    QPushButton* m_keepButton = nullptr;
    QPushButton* m_dropButton = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_PUBLISHING_TEMPLATE_EDITOR_H
