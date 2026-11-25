// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_boxes.h
/// @brief The groups of the card of a material that is a source: its bibliographic
/// description (source_description_box.cpp), how the bibliography shows it, its bookmarks and
/// the quotes from it; and the group of a quote that says where it comes from (source_boxes.cpp)

#ifndef KALAHARI_GUI_PANELS_SOURCE_BOXES_H
#define KALAHARI_GUI_PANELS_SOURCE_BOXES_H

#include "kalahari/core/source_description.h"

#include <QGroupBox>
#include <QHash>
#include <QString>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The bibliographic description of a material on its full card: the kind of the source,
/// its people and the fields of its kind
///
/// The kind changes the fields and their names (a journal for an article, a site for a web
/// page). The people are a table of names and what each did: "+" or Insert adds one with the
/// keys in its name, F2 edits a cell, "-" or Delete takes one out. A field is saved when it
/// loses the keys; the title, empty, is the title of the material. Paste Description reads a
/// record of BibTeX or RIS from the clipboard; From the Web by ISBN or DOI..., shown when the
/// Settings allow it (core::SourceCatalogs::LOOKUP_SETTING), asks a catalog on the web; Fill In
/// from File, for a PDF file, the title, the author, the year and the pages the file has. A
/// material whose description says nothing is no source; the kind alone does not make it one.
class SourceDescriptionBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the descriptions of the materials of @p book
    explicit SourceDescriptionBox(core::ProjectManager& book, QWidget* parent = nullptr);
    ~SourceDescriptionBox() override;

    SourceDescriptionBox(const SourceDescriptionBox&) = delete;
    SourceDescriptionBox& operator=(const SourceDescriptionBox&) = delete;
    SourceDescriptionBox(SourceDescriptionBox&&) = delete;
    SourceDescriptionBox& operator=(SourceDescriptionBox&&) = delete;

    /// @brief Show the description of @p element; the field with the keys keeps what is typed
    void showMaterial(const core::ProjectElement& element);

    /// @brief The description the card shows now, as it would be saved
    [[nodiscard]] core::SourceDescription shownSource() const;

    /// @brief The list of the kinds of source
    [[nodiscard]] QComboBox* kindBox() const { return m_kindBox; }
    /// @brief The table of the people: a name and what each did
    [[nodiscard]] QTableWidget* peopleTable() const { return m_peopleTable; }
    /// @brief "+" of the people
    [[nodiscard]] QToolButton* addPersonButton() const { return m_addPersonButton; }
    /// @brief "-" of the people
    [[nodiscard]] QToolButton* removePersonButton() const { return m_removePersonButton; }
    /// @brief Paste Description (BibTeX, RIS)
    [[nodiscard]] QPushButton* pasteButton() const { return m_pasteButton; }
    /// @brief From the Web by ISBN or DOI..., shown when the Settings allow it
    [[nodiscard]] QPushButton* lookupButton() const { return m_lookupButton; }
    /// @brief Fill In from File, shown for a PDF file
    [[nodiscard]] QPushButton* fillButton() const { return m_fillButton; }

    /// @brief The field of @p field (core::SourceDescription::TITLE...)
    [[nodiscard]] QLineEdit* field(const QString& field) const { return m_edits.value(field); }

    /// @brief Whether the card shows the field of @p field for the kind chosen
    [[nodiscard]] bool isFieldShown(const QString& field) const;

    /// @brief The label of the field of @p field
    [[nodiscard]] QLabel* fieldLabel(const QString& field) const { return m_labels.value(field); }

public slots:
    /// @brief Add a person at the end, with the keys in the name
    void addPerson();

    /// @brief Take out the current person
    void removePerson();

    /// @brief Take the description from a record of BibTeX or RIS on the clipboard
    void pasteDescription();

    /// @brief Fill the description from a catalog on the web, by the ISBN or the DOI the
    /// writer gives (dialogs::SourceLookupDialog): its fields take the place of those the card
    /// has, the others stay
    void lookUp();

    /// @brief Fill the empty fields with what the PDF file of the material says of itself: its
    /// title, its authors, its year and, of a book or a thesis, its pages
    void fillFromFile();

    /// @brief Save what the card shows
    void save();

signals:
    /// @brief The description of material @p elementId changed
    void changed(const QString& elementId);

    /// @brief A sentence for the status bar
    void message(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The kinds of source in their list, that of the description chosen
    void fillKinds();

    /// @brief The people of the description in the table
    void fillPeople();

    /// @brief The fields of the kind chosen, with their names
    void showKind();

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    core::ProjectManager& m_book;
    QString m_elementId;
    QString m_materialKind;           ///< core::MaterialKinds::PUBLICATION...
    QString m_shownKind;              ///< The kind whose fields the form has
    core::SourceDescription m_source;  ///< As saved
    bool m_updating = false;
    QComboBox* m_kindBox = nullptr;
    QTableWidget* m_peopleTable = nullptr;
    QLabel* m_noPeopleLabel = nullptr;
    QToolButton* m_addPersonButton = nullptr;
    QToolButton* m_removePersonButton = nullptr;
    QFormLayout* m_form = nullptr;
    QHash<QString, QLineEdit*> m_edits;
    QHash<QString, QLabel*> m_labels;
    QPushButton* m_pasteButton = nullptr;
    QPushButton* m_lookupButton = nullptr;
    QPushButton* m_fillButton = nullptr;
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id
    bool m_pdf = false;  ///< The file of the material is a PDF the program reads
};

/// @brief How the bibliography of the book shows a material: its entry in a style, whether the
/// bibliography has it, and Copy Entry
///
/// The full card offers every style, that of the book first chosen; the choice only shows the
/// entry in another style. The short card of the Properties panel shows the entry in the style
/// of the book alone, for a material that is a source. The gear opens Settings > Materials.
class BibliographyEntryBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param compact The short card: the entry in the style of the book and Copy Entry
    /// @brief The group of the entries of the materials of @p book
    explicit BibliographyEntryBox(core::ProjectManager& book, QWidget* parent = nullptr,
                                  bool compact = false);

    /// @brief Show the entry of @p element
    void showMaterial(const core::ProjectElement& element);

    /// @brief The style the entry is shown in
    [[nodiscard]] QString style() const;

    /// @brief The entry shown, as plain text
    [[nodiscard]] QString entryText() const;

    /// @brief The list of the styles, hidden on the short card
    [[nodiscard]] QComboBox* styleBox() const { return m_styleBox; }
    /// @brief The entry, with its italics
    [[nodiscard]] QLabel* entryLabel() const { return m_entryLabel; }
    /// @brief In the bibliography: when linked with the text, always or never
    [[nodiscard]] QComboBox* inclusionBox() const { return m_inclusionBox; }
    /// @brief Copy Entry
    [[nodiscard]] QPushButton* copyButton() const { return m_copyButton; }
    /// @brief The gear, which opens Settings > Materials
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

public slots:
    /// @brief Copy the entry to the clipboard, with its italics where the clipboard takes them
    void copyEntry();

signals:
    /// @brief The description of material @p elementId changed: whether the bibliography has it
    void changed(const QString& elementId);

    /// @brief Open Settings > Materials
    void requestSettings();

    /// @brief A sentence for the status bar
    void message(const QString& text);

private:
    /// @brief Save whether the bibliography has the source, as its list says
    void saveInclusion();

    /// @brief The styles, that of the book marked and chosen when @p chooseBook
    void fillStyles(bool chooseBook);

    /// @brief The entry in the style chosen
    void updateEntry();

    core::ProjectManager& m_book;
    bool m_compact = false;
    QString m_elementId;
    QString m_title;
    core::SourceDescription m_source;
    QString m_bookStyle;  ///< The style of the book the styles were filled for
    QComboBox* m_styleBox = nullptr;
    QLabel* m_entryLabel = nullptr;
    QComboBox* m_inclusionBox = nullptr;
    QPushButton* m_copyButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
};

/// @brief The bookmarks of the file of a material: a page and a note each
///
/// "+" or Insert adds one at the page shown last with the keys in its page, F2 edits a cell,
/// "-" or Delete takes one out, the eye shows its page in the preview of a PDF file. Every
/// change is saved at once.
class BookmarksBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the bookmarks of the materials of @p book
    explicit BookmarksBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show the bookmarks of @p element
    void showMaterial(const core::ProjectElement& element);

    /// @brief The table of the bookmarks: a page and a note each
    [[nodiscard]] QTableWidget* table() const { return m_table; }
    /// @brief "+" of the bookmarks
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }
    /// @brief "-" of the bookmarks
    [[nodiscard]] QToolButton* removeButton() const { return m_removeButton; }
    /// @brief The eye: the page in the preview
    [[nodiscard]] QToolButton* showButton() const { return m_showButton; }

public slots:
    /// @brief Add a bookmark at the end, with the keys in its page
    void addBookmark();

    /// @brief Take out the current bookmark
    void removeBookmark();

    /// @brief Show the page of the current bookmark in the preview of the file
    void showPage();

signals:
    /// @brief The bookmarks of material @p elementId changed
    void changed(const QString& elementId);

    /// @brief Show page @p page of the file of material @p elementId in its preview
    void requestShowPage(const QString& elementId, const QString& page);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Save the bookmarks of the table, in its order
    void save();

    /// @brief The buttons follow the rows and a book open read-only
    void updateButtons();

    core::ProjectManager& m_book;
    QString m_elementId;
    int m_lastPage = 0;
    QList<core::PageBookmark> m_bookmarks;  ///< As saved, with what the table does not show
    bool m_updating = false;
    QTableWidget* m_table = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
    QToolButton* m_removeButton = nullptr;
    QToolButton* m_showButton = nullptr;
    bool m_preview = false;  ///< Whether the file is shown in a preview with pages
};

/// @brief The quotes whose source is a material, with their pages
///
/// "+" or Insert asks for a new quote from the source; a double-click or Enter opens a quote.
class SourceQuotesBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the quotes from the materials of @p book
    explicit SourceQuotesBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show the quotes from @p element
    void showMaterial(const core::ProjectElement& element);

    /// @brief The table of the quotes: a page and the words each
    [[nodiscard]] QTableWidget* table() const { return m_table; }
    /// @brief "+": a new quote from the source
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }

signals:
    /// @brief Make a new quote from source @p sourceId
    void requestNewQuote(const QString& sourceId);

    /// @brief Open quote @p elementId
    void requestOpenQuote(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Open the quote of row @p row, when there is one
    void openQuote(int row);

    core::ProjectManager& m_book;
    QString m_elementId;
    QTableWidget* m_table = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QToolButton* m_addButton = nullptr;
};

/// @brief Where a quote comes from: a material that is a source and its page, or an author and
/// a work written in, and the reference to it in the text
///
/// The list of the sources has the materials with a bibliographic description, by title, and
/// "No source" first, which shows the fields of the author and the work. Open Source opens the
/// card of the source. Every change is saved at once, a field when it loses the keys.
class QuoteSourceBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of where the quotes of @p book come from
    explicit QuoteSourceBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show where quote @p element comes from; the field with the keys keeps what is
    /// typed
    void showMaterial(const core::ProjectElement& element);

    /// @brief Where the quote comes from, as the card shows it
    [[nodiscard]] core::QuoteSource shownQuote() const;

    /// @brief The list of the sources, "No source" first
    [[nodiscard]] QComboBox* sourceBox() const { return m_sourceBox; }
    /// @brief The page of the quote
    [[nodiscard]] QLineEdit* pageEdit() const { return m_pageEdit; }
    /// @brief The author of a quote without a source
    [[nodiscard]] QLineEdit* authorEdit() const { return m_authorEdit; }
    /// @brief The work of a quote without a source
    [[nodiscard]] QLineEdit* workEdit() const { return m_workEdit; }
    /// @brief Open Source: the card of the source
    [[nodiscard]] QPushButton* openSourceButton() const { return m_openSourceButton; }
    /// @brief Show Page: the page in the preview of the file of the source
    [[nodiscard]] QPushButton* showPageButton() const { return m_showPageButton; }
    /// @brief The author and the work from the card of the source
    [[nodiscard]] QLabel* fromSourceLabel() const { return m_fromSourceLabel; }
    /// @brief The reference to the quote in the text
    [[nodiscard]] QLabel* referenceLabel() const { return m_referenceLabel; }

public slots:
    /// @brief Save what the card shows
    void save();

signals:
    /// @brief Where quote @p elementId comes from changed
    void changed(const QString& elementId);

    /// @brief Open source @p sourceId at page @p page (empty: its card)
    void requestOpenSource(const QString& sourceId, const QString& page);

private:
    /// @brief The sources of the book in the list, with that of the quote chosen
    void fillSources(const QString& chosen);

    /// @brief The name of a source of the quote that is no source any more: @p material
    /// without its description, or nullptr for one that is gone
    [[nodiscard]] static QString lostSourceName(const core::ProjectElement* material);

    /// @brief The fields that the source chosen asks for, the sentence and the reference
    void updateShown();

    core::ProjectManager& m_book;
    QString m_elementId;
    core::QuoteSource m_quote;  ///< As saved
    bool m_updating = false;
    QFormLayout* m_form = nullptr;
    QComboBox* m_sourceBox = nullptr;
    QLineEdit* m_pageEdit = nullptr;
    QPushButton* m_openSourceButton = nullptr;
    QPushButton* m_showPageButton = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_workEdit = nullptr;
    QLabel* m_fromSourceLabel = nullptr;
    QLabel* m_referenceLabel = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_SOURCE_BOXES_H
