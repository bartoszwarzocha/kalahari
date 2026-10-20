/// @file material_page.h
/// @brief The short card of a material in the Properties panel, and the materials of an
/// element of the book

#pragma once

#include "kalahari/core/book_project.h"

#include <QGroupBox>
#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {

/// @brief The short card of a material in the Properties panel: what the writer checks and
/// changes while writing
///
/// - Its picture across the panel (a click opens its preview), or the icon of its kind for a
///   file that is no picture, and a sentence when the program cannot show the picture; under
///   it the file's name, format, size in pixels and on the disk; Preview and System Program
///   open the material in the two ways, whatever the setting.
/// - Its title and its file.
/// - Its description, saved when the field loses the keys.
/// - The writer's own fields: a name and a value each, in the writer's order. "+" or Insert
///   adds one with the keys in its name, Tab goes to its value, F2 edits a cell, "-" or Delete
///   takes it out (asking when it has a value). The names of the fields of the other
///   elements are suggested.
/// - Its links: the elements and sections it is about. "+" or Insert opens Link, a node of the
///   Navigator dropped on the list links it, "-" or Delete takes a link out without asking
///   (nothing is deleted), and a double-click or Enter opens the element linked.
///
/// Every change is saved at once in the .klh file. A book open read-only shows it all and
/// changes nothing.
class MaterialPage : public QWidget {
    Q_OBJECT

public:
    /// @brief Height of the picture at most
    static constexpr int MAXIMUM_THUMBNAIL_HEIGHT = 220;

    explicit MaterialPage(QWidget* parent = nullptr);

    /// @brief Show material @p elementId of the open book
    void showMaterial(const QString& elementId);

    /// @brief The material it shows
    QString elementId() const { return m_elementId; }

    /// @brief Show the material again as the book has it; the field with the keys keeps what
    /// is typed in it
    void refresh();

    /// @brief Give the keys to the title
    void focusFirstField();

    QToolButton* thumbnail() const { return m_thumbnail; }
    QLabel* thumbnailProblem() const { return m_thumbnailProblem; }
    QLabel* fileLabel() const { return m_fileLabel; }
    QPushButton* previewButton() const { return m_previewButton; }
    QPushButton* systemButton() const { return m_systemButton; }
    QLineEdit* titleEdit() const { return m_titleEdit; }
    QLabel* pathLabel() const { return m_pathLabel; }
    QPlainTextEdit* descriptionEdit() const { return m_descriptionEdit; }
    QTableWidget* fieldsTable() const { return m_fieldsTable; }
    QToolButton* addFieldButton() const { return m_addFieldButton; }
    QToolButton* removeFieldButton() const { return m_removeFieldButton; }
    QTreeWidget* linksList() const { return m_linksList; }
    QToolButton* addLinkButton() const { return m_addLinkButton; }
    QToolButton* removeLinkButton() const { return m_removeLinkButton; }

public slots:
    /// @brief Add a field at the end, with the keys in its name
    void addField();

    /// @brief Take out the current field, asking when it has a value
    void removeField();

    /// @brief Open Link and link what is checked there
    void editLinks();

    /// @brief Link @p links too, after the links it has
    void addLinks(const QStringList& links);

    /// @brief Take out the current link
    void removeLink();

signals:
    /// @brief Open the material's preview
    void requestPreview(const QString& elementId);

    /// @brief Open the material in the program the system gives its file
    void requestOpenWithSystem(const QString& elementId);

    /// @brief Open what link @p link names: an element, or the section in the Navigator
    void requestOpenLink(const QString& link);

    /// @brief The title of material @p elementId changed: the Navigator shows it
    void titleChanged(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The picture at the width of the panel, or the icon of the file's kind
    /// @return Why the picture cannot be read; empty when it was read or is no picture. The
    /// program does not show a picture it cannot read.
    QString updateThumbnail();

    /// @brief The fields of the material in the table
    void fillFields(const core::ProjectElement& element);

    /// @brief The links of the material in the list
    void fillLinks(const core::ProjectElement& element);

    /// @brief Save the title from its field
    void saveTitle();

    /// @brief Save the description from its field
    void saveDescription();

    /// @brief Save the fields of the table, in its order
    void saveFields();

    /// @brief Save @p links as the links of the material
    void saveLinks(const QStringList& links);

    /// @brief The fields that the table shows
    QList<core::ElementField> tableFields() const;

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    QString m_elementId;
    QString m_path;              ///< The file of the material
    int m_rotation = 0;          ///< The turn of its picture
    bool m_picture = false;      ///< Whether the program shows its picture
    QImage m_thumbnailPicture;   ///< The picture as read for the card, before it is fitted
    QString m_thumbnailKey;      ///< The file, turn and time of change it was read for
    QString m_thumbnailError;    ///< Why it could not be read
    bool m_updating = false;     ///< The fields are being filled: no saving
    QList<core::ElementField> m_fields;  ///< The fields as saved, with what the table hides

    QGroupBox* m_infoGroup = nullptr;
    QToolButton* m_thumbnail = nullptr;
    QLabel* m_thumbnailProblem = nullptr;
    QLabel* m_fileLabel = nullptr;
    QPushButton* m_previewButton = nullptr;
    QPushButton* m_systemButton = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLabel* m_pathLabel = nullptr;
    QPlainTextEdit* m_descriptionEdit = nullptr;
    QTableWidget* m_fieldsTable = nullptr;
    QLabel* m_noFieldsLabel = nullptr;
    QToolButton* m_addFieldButton = nullptr;
    QToolButton* m_removeFieldButton = nullptr;
    QTreeWidget* m_linksList = nullptr;
    QLabel* m_noLinksLabel = nullptr;
    QToolButton* m_addLinkButton = nullptr;
    QToolButton* m_removeLinkButton = nullptr;
};

/// @brief The materials linked with an element of the book or a section, in its page of the
/// Properties panel: each with its picture, its title and its first field
///
/// A click chooses a material, a double-click or Enter opens it, "+" or Insert opens Link with
/// the materials of the Workshop, and "-" or Delete takes the link out of the material. The
/// link is saved once, with the material.
class LinkedMaterialsBox : public QGroupBox {
    Q_OBJECT

public:
    explicit LinkedMaterialsBox(QWidget* parent = nullptr);

    /// @brief Show the materials linked with @p link, an element or a section
    /// (core::BookProject::sectionLink()); @p emptyText says that it has none
    void showLinked(const QString& link, const QString& emptyText);

    /// @brief Show again the materials linked with the same element or section
    void refresh();

    /// @brief The element or section it shows the materials of
    QString link() const { return m_link; }

    QListWidget* list() const { return m_list; }
    QToolButton* addButton() const { return m_addButton; }
    QToolButton* removeButton() const { return m_removeButton; }
    QLabel* emptyLabel() const { return m_emptyLabel; }

public slots:
    /// @brief Open Link with the materials of the Workshop and link what is checked there
    void editMaterials();

    /// @brief Take the link out of the current material
    void removeMaterial();

signals:
    /// @brief Open material @p elementId
    void requestOpenMaterial(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    QString m_link;
    QString m_emptyText;
    QListWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
};

} // namespace gui
} // namespace kalahari
