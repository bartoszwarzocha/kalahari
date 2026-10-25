/// @file material_page.h
/// @brief The card of a material, short in the Properties panel and full on a tab of its own,
/// and the materials of an element of the book

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/gui/material_source.h"

#include <QGroupBox>
#include <QImage>
#include <QScrollArea>
#include <QSet>
#include <QString>
#include <QWidget>

class QFormLayout;
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

/// @brief The card of a material: short in the Properties panel, what the writer checks and
/// changes while writing; full on a tab beside the chapters (MaterialCardPanel), the place to
/// work on it
///
/// - What it comes from (materialSourceOf()):
///   - a file: its picture across the card (a click opens its preview), or the icon of its
///     kind for a file that is no picture, and a sentence when the program cannot show the
///     picture; under it the file's name, format, size in pixels and on the disk; Preview and
///     System Program open the material in the two ways, whatever the setting;
///   - a web page: its icon and host; Open in Browser and Copy Address;
///   - a picture on the web: the picture of its copy in the book, as of a file, with Preview;
///     without a copy a sentence, and Save Copy unless the setting of the downloads says
///     never (MaterialDownloader); Open in Browser either way;
///   - neither a file nor an address: a sentence, as its description and own fields tell
///     where it comes from.
/// - Its title, its file (the copy of a picture on the web) and its address.
/// - Its description, saved when the field loses the keys.
/// - The writer's own fields: a name and a value each, in the writer's order. "+" or Insert
///   adds one with the keys in its name, Tab goes to its value, F2 edits a cell, "-" or Delete
///   takes it out (asking when it has a value). The names of the fields of the other
///   elements are suggested.
/// - Its links: the elements and sections it is about. "+" or Insert opens Link, a node of the
///   Navigator dropped on the list links it, "-" or Delete takes a link out without asking
///   (nothing is deleted), and a double-click or Enter opens the element linked.
///
/// Every change is saved at once in the .klh file, and changed() tells the other cards of the
/// material. A book open read-only shows it all and changes nothing.
///
/// The full card has the same parts: its picture larger, the description beside the own
/// fields, higher, and the links under them.
class MaterialPage : public QWidget {
    Q_OBJECT

public:
    /// @brief Height of the picture at most
    static constexpr int MAXIMUM_THUMBNAIL_HEIGHT = 220;

    /// @brief Height of the picture of the full card at most
    static constexpr int MAXIMUM_FULL_THUMBNAIL_HEIGHT = 360;

    /// @brief The card; @p full: the full card, else the short one
    explicit MaterialPage(QWidget* parent = nullptr, bool full = false);
    /// @note What a field does while the card is destroyed (a title losing the keys) does not
    /// reach the card
    ~MaterialPage() override;

    /// @brief Whether it is the full card
    bool isFull() const { return m_full; }

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
    QPushButton* browserButton() const { return m_browserButton; }
    QPushButton* copyAddressButton() const { return m_copyAddressButton; }
    QPushButton* saveCopyButton() const { return m_saveCopyButton; }
    QLineEdit* titleEdit() const { return m_titleEdit; }
    QLabel* pathLabel() const { return m_pathLabel; }
    QLabel* addressLabel() const { return m_addressLabel; }

    /// @brief Something for material @p elementId is being downloaded from its address, or no
    /// longer: Save Copy of its card waits for it
    void setDownloading(const QString& elementId, bool downloading);

    /// @brief Give the keys to the description
    void focusDescription();
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

    /// @brief Open Link on the current link and link what is checked there, as it is set
    /// there
    void editLinks();

    /// @brief Link the elements and sections @p links too, after the links it has; each new
    /// one gets the role its kind suggests (core::ProjectManager::newLink())
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

    /// @brief Open the address of the material in the system's browser
    void requestOpenInBrowser(const QString& elementId);

    /// @brief Download the picture of the material's address for its copy in the book
    void requestSaveCopy(const QString& elementId);

    /// @brief Material @p elementId changed on this card: its title, description, fields or
    /// links; its other cards show it
    void changed(const QString& elementId);

    /// @brief A sentence for the status bar, such as that the address was copied
    void message(const QString& text);

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
    void saveLinks(const QList<core::ElementLink>& links);

    /// @brief The fields that the table shows
    QList<core::ElementField> tableFields() const;

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    /// @brief Show the source of @p element: its picture or icon, the sentence and the line
    /// under it, the buttons and the rows of the file and the address
    void showSource(const core::ProjectElement& element);

    /// @brief Copy the address of the material to the clipboard
    void copyAddress();

    bool m_full = false;         ///< The full card
    QString m_elementId;
    MaterialSource m_source = MaterialSource::None;  ///< Where it comes from
    QString m_url;               ///< Its web address
    QSet<QString> m_downloading;  ///< The materials with a download on its way
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
    QPushButton* m_browserButton = nullptr;
    QPushButton* m_copyAddressButton = nullptr;
    QPushButton* m_saveCopyButton = nullptr;
    QFormLayout* m_form = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLabel* m_addressLabel = nullptr;
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

/// @brief The full card of a material on a tab beside the chapters (MaterialPage): the place to
/// work on a material, such as a quotation copied out or the notes of a talk
///
/// The tab has the material's id in its "elementId" property and the id of its icon in
/// "tabIconId", as the other tabs of the book's elements.
class MaterialCardPanel : public QScrollArea {
    Q_OBJECT

public:
    /// @brief Width of the card at most; a wider tab has room around it
    static constexpr int MAXIMUM_WIDTH = 860;

    /// @brief The card of material @p elementId of the open book
    explicit MaterialCardPanel(const QString& elementId, QWidget* parent = nullptr);

    /// @brief The material it shows
    QString elementId() const;

    /// @brief The card
    MaterialPage* page() const { return m_page; }

    /// @brief Show the material again as the book has it, and its icon in "tabIconId"
    void refresh();

private:
    MaterialPage* m_page = nullptr;
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
    /// @note What the list does while the box is destroyed does not reach the box
    ~LinkedMaterialsBox() override;

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
