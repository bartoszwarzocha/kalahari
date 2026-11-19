// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_templates_dialog.h
/// @brief Templates of Texts for Publishers: the templates of the book types, which do not
///        change, and the writer's own, made, copied, changed and deleted here

#ifndef KALAHARI_GUI_DIALOGS_PUBLISHING_TEMPLATES_DIALOG_H
#define KALAHARI_GUI_DIALOGS_PUBLISHING_TEMPLATES_DIALOG_H

#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QKeyCombination>
#include <QList>
#include <QString>

#include <utility>

class QCheckBox;
class QComboBox;
class QEvent;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui::dialogs {

/// @brief The templates of a book type, a group of the list of the window
struct PublishingTemplateGroup {
    QString name;  ///< The name of the book type: "Novel"
    QList<core::PublishingTemplate> templates;
};

/// @brief A book type that the writer's own template can be for
struct PublishingBookType {
    QString id;    ///< "kalahari.novel"
    QString name;  ///< "Novel"
};

/// @brief Templates of Texts for Publishers
///
/// On the left the list of the templates: those of the book type of the book (of every
/// installed type without a book), then the writer's own ("My Templates"); New Template
/// (Insert), Make a Copy and Delete (Delete) under it. On the right the template chosen: its
/// name, its part, the book types it is for, its target length and its points (a synopsis:
/// Insert, Delete, Alt+Up and Alt+Down in the table, F2 changes a cell) or its pattern and
/// the fields the pattern names (a sentence about the book). A template of a book type shows
/// all of it and changes nothing; Make a Copy makes it the writer's own.
///
/// The writer's own templates are kept at once (core::PublishingTexts::setOwnTemplates()), in
/// all the writer's books; the texts written with a template keep it.
class PublishingTemplatesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param builtIn The templates of the book types, by type
    /// @param types The book types the writer's own template can be for
    /// @param bookType The type of the open book, which a new template is for; empty: none
    /// @param texts The texts of the main part of the open book (ids and titles), the points
    ///        of a template with a point for each text
    /// @param parent Parent widget
    PublishingTemplatesDialog(QList<PublishingTemplateGroup> builtIn,
                              QList<PublishingBookType> types, QString bookType,
                              QList<std::pair<QString, QString>> texts,
                              QWidget* parent = nullptr);

    /// @brief The templates of the type of the book open in @p book; of every installed type
    ///        when no book is open (@p book nullptr or without a book)
    static QList<PublishingTemplateGroup> builtInOf(const core::ProjectManager* book);

    /// @brief The installed book types
    static QList<PublishingBookType> installedTypes();

    /// @brief Open the window over @p parent for the book open in @p book (nullptr: none)
    /// @return Whether the writer changed their templates
    static bool editTemplates(const core::ProjectManager* book, QWidget* parent);

    /// @brief The writer's own templates as the window has them
    [[nodiscard]] QList<core::PublishingTemplate> ownTemplates() const { return m_own; }

    /// @brief The template chosen (core::PublishingTemplate::reference()); empty: none
    [[nodiscard]] QString currentTemplate() const;

    /// @brief Choose template @p reference; one the window does not have changes nothing
    void setCurrentTemplate(const QString& reference);

    /// @brief Whether the writer changed their templates
    [[nodiscard]] bool changed() const { return m_changed; }

    /// @brief The list of the templates, with the names of their groups
    [[nodiscard]] QListWidget* templateList() const { return m_list; }
    [[nodiscard]] QPushButton* newButton() const { return m_newButton; }
    [[nodiscard]] QPushButton* copyButton() const { return m_copyButton; }
    [[nodiscard]] QPushButton* deleteButton() const { return m_deleteButton; }
    [[nodiscard]] QLineEdit* nameEdit() const { return m_nameEdit; }
    /// @brief The part: a synopsis or a sentence about the book
    [[nodiscard]] QComboBox* partBox() const { return m_partBox; }
    /// @brief What a template of a book type is for: "Novel – a template of the book type,
    ///        which does not change. Make a Copy to change it."
    [[nodiscard]] QLabel* typeLabel() const { return m_typeLabel; }
    /// @brief The box of book type @p id of the writer's own template; nullptr: none
    [[nodiscard]] QCheckBox* typeBox(const QString& id) const;
    [[nodiscard]] QSpinBox* charactersBox() const { return m_charactersBox; }
    [[nodiscard]] QSpinBox* wordsBox() const { return m_wordsBox; }
    /// @brief Of a template with a point for each text: the length for each text in the book
    [[nodiscard]] QLabel* lengthNote() const { return m_lengthNote; }
    /// @brief The points of a synopsis: the point and its hint
    [[nodiscard]] QTableWidget* pointsTable() const { return m_pointsTable; }
    [[nodiscard]] QPushButton* addPointButton() const { return m_addPointButton; }
    [[nodiscard]] QPushButton* removePointButton() const { return m_removePointButton; }
    [[nodiscard]] QPushButton* pointUpButton() const { return m_pointUpButton; }
    [[nodiscard]] QPushButton* pointDownButton() const { return m_pointDownButton; }
    /// @brief The pattern of a sentence: "When {event}, {hero} must {goal}, before {stakes}."
    [[nodiscard]] QLineEdit* patternEdit() const { return m_patternEdit; }
    /// @brief The fields of the pattern: {id}, its name and its hint
    [[nodiscard]] QTableWidget* fieldsTable() const { return m_fieldsTable; }

    /// @brief The columns of the table of the points
    enum PointColumn { POINT_NAME_COLUMN, POINT_HINT_COLUMN };
    /// @brief The columns of the table of the fields
    enum FieldColumn { FIELD_ID_COLUMN, FIELD_NAME_COLUMN, FIELD_HINT_COLUMN };

public slots:
    /// @brief New Template: a template of the writer, of the part of the one chosen
    void newTemplate();
    /// @brief Make a Copy: the template chosen as one of the writer's own
    void copyTemplate();
    /// @brief Delete the writer's template chosen, when the writer confirms it
    void deleteTemplate();
    /// @brief A new point after the point chosen
    void addPoint();
    /// @brief The point chosen goes
    void removePoint();
    /// @brief Move the point chosen by @p step places (-1: up, 1: down)
    void movePoint(int step);

signals:
    /// @brief The writer's own templates changed and are kept
    void templatesChanged();

protected:
    /// @brief The keys of the list (Insert, Delete) and of the points (Insert, Delete,
    ///        Alt+Up, Alt+Down)
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The list and its buttons
    QWidget* createList();
    /// @brief The fields of the template chosen
    QWidget* createForm();
    /// @brief The rows of the name and the part, the types and the length in @p fields
    void createBasics(QFormLayout* fields);
    /// @brief The boxes of the book types of the writer's template, the type of another
    QWidget* createTypes(QWidget* form);
    /// @brief The target length in characters and in words, with its note
    QWidget* createLength(QWidget* form);
    /// @brief The row of the points of a synopsis and their buttons in @p fields
    void createPoints(QFormLayout* fields);
    /// @brief The rows of the pattern of a sentence and of its fields in @p fields
    void createPattern(QFormLayout* fields);
    /// @brief The rows of the list, template @p current chosen
    void fillList(const QString& current);
    /// @brief The fields after the template chosen
    void showTemplate();
    /// @brief The book types after template @p templ
    void showTypes(const core::PublishingTemplate& templ, bool own);
    /// @brief The length and its note after template @p templ
    void showLength(const core::PublishingTemplate& templ, bool own);
    /// @brief The rows of the points of template @p templ
    void fillPoints(const core::PublishingTemplate& templ, bool own);
    /// @brief The rows of the fields of template @p templ
    void fillFields(const core::PublishingTemplate& templ, bool own);
    /// @brief The buttons after the template and the point chosen
    void updateButtons();
    /// @brief The template of @p reference; nullptr: none
    [[nodiscard]] const core::PublishingTemplate* templateOf(const QString& reference) const;
    /// @brief The writer's template chosen; nullptr: none, or one of a book type
    [[nodiscard]] core::PublishingTemplate* ownCurrent();
    /// @brief Keep the writer's templates, @p listChanged: and show the list again
    void keep(bool listChanged);
    /// @brief The name of the template chosen was changed to @p name
    void onNameEdited(const QString& name);
    /// @brief The part of the template chosen was changed
    void onPartChanged(int index);
    /// @brief The types of the template chosen were changed
    void onTypesChanged();
    /// @brief The length of the template chosen was changed
    void onLengthChanged();
    /// @brief A cell of the points was changed
    void onPointChanged(int row, int column);
    /// @brief The pattern was changed to @p pattern: the fields follow it
    void onPatternEdited(const QString& pattern);
    /// @brief A cell of the fields was changed
    void onFieldChanged(int row, int column);
    /// @brief What @p keys do in the list (@p points false) or in the points; @p run: do it
    bool runKey(QKeyCombination keys, bool points, bool run);
    /// @brief A name for a new template from @p base, not taken by another of the writer's
    [[nodiscard]] QString freeName(const QString& base) const;

    QList<PublishingTemplateGroup> m_builtIn;
    QList<PublishingBookType> m_types;
    QString m_bookType;
    QList<std::pair<QString, QString>> m_texts;
    QList<core::PublishingTemplate> m_own;
    QString m_current;        ///< The reference of the template chosen
    bool m_showing = false;   ///< The fields are being filled: their changes are not the writer's
    bool m_changed = false;

    QListWidget* m_list = nullptr;
    QPushButton* m_newButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QWidget* m_form = nullptr;
    QLabel* m_empty = nullptr;  ///< In place of the fields while no template is chosen
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_partBox = nullptr;
    QLabel* m_typeLabel = nullptr;
    QWidget* m_typeBoxes = nullptr;
    QList<QCheckBox*> m_typeChecks;
    QSpinBox* m_charactersBox = nullptr;
    QSpinBox* m_wordsBox = nullptr;
    QLabel* m_lengthNote = nullptr;
    QWidget* m_pointsSection = nullptr;
    QTableWidget* m_pointsTable = nullptr;
    QLabel* m_pointsNote = nullptr;
    QWidget* m_pointButtons = nullptr;
    QPushButton* m_addPointButton = nullptr;
    QPushButton* m_removePointButton = nullptr;
    QPushButton* m_pointUpButton = nullptr;
    QPushButton* m_pointDownButton = nullptr;
    QWidget* m_patternSection = nullptr;
    QLineEdit* m_patternEdit = nullptr;
    QTableWidget* m_fieldsTable = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_PUBLISHING_TEMPLATES_DIALOG_H
