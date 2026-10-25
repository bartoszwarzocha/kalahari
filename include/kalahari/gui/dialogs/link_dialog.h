/// @file link_dialog.h
/// @brief Link: what a material is about, or the materials of an element of the book

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/core/book_project.h"

#include <QHash>
#include <QString>
#include <QStringList>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Link: a tree of the open book with boxes to check
///
/// For a material, the tree has the book with its sections and their elements, and the
/// Workshop with its elements in groups of their kinds; the sections and the elements can be
/// checked, the book, the Workshop and its groups cannot, and the material itself is not
/// there. For an element of the book, or a section, the tree has the materials of the
/// Workshop. What is linked now is checked.
///
/// At the right, the link of the current item, while it is checked: its role, the paragraph
/// of the text it names (when links may name one, core::LinkRoles::paragraphsShown()) and a
/// note. A new link gets the role the kind of the material suggests (ProjectManager::newLink()),
/// and a quote the first paragraph that has its words; an item checked again gets back what
/// it had.
///
/// Typing filters the tree also while the tree has the keys: the letters go to the filter
/// above it. The arrows choose, Space checks, Tab goes to the role, Enter links and Esc
/// closes. On a small screen only the tree scrolls.
class LinkDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the boxes of the tree link
    enum class Mode {
        Targets,   ///< What a material is about: sections and elements
        Materials  ///< The materials about an element or a section
    };

    /// @brief Create the dialog for the open book
    /// @param mode What the boxes link
    /// @param link The material (Targets), or the element or section (Materials,
    ///        core::BookProject::sectionLink())
    /// @param parent Parent widget
    LinkDialog(Mode mode, const QString& link, QWidget* parent = nullptr);

    /// @brief The checked links, in the order of the tree
    QStringList checkedLinks() const;

    /// @brief The links checked when the dialog opened, in the order of the tree
    const QStringList& initialLinks() const { return m_initial; }

    /// @brief The link of item @p id with the role, paragraph and note set for it: of the
    /// material with element or section @p id (Targets), or of material @p id with the
    /// element or section (Materials)
    core::ElementLink linkOf(const QString& id) const;

    /// @brief Make the item of @p id the current one, whose link the right side shows
    void setCurrentLink(const QString& id);

    /// @brief The role of the current link
    QComboBox* roleCombo() const { return m_role; }

    /// @brief The paragraph of the current link
    QComboBox* paragraphCombo() const { return m_paragraph; }

    /// @brief The note of the current link
    QLineEdit* noteEdit() const { return m_note; }

    /// @brief The tree
    QTreeWidget* tree() const { return m_tree; }

    /// @brief The filter above the tree
    QLineEdit* filterEdit() const { return m_filter; }

    /// @brief Item of link @p link, or nullptr
    QTreeWidgetItem* itemOf(const QString& link) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The book and the Workshop, for a material
    void fillTargets(const QString& material);

    /// @brief The materials of the Workshop, for an element or a section
    void fillMaterials(const QString& link);

    /// @brief Show only the items whose titles have @p text, and those above them
    void filter(const QString& text);

    /// @brief The right side: the link of @p item, or why there is none to set
    void showLink(QTreeWidgetItem* item);

    /// @brief Item @p item was checked or unchecked: a new link gets its role and paragraph
    void itemChecked(QTreeWidgetItem* item);

    /// @brief The paragraphs of the text of the link of item @p id, with the current one of
    /// @p link
    void fillParagraphs(const QString& id, const core::ElementLink& link);

    /// @brief The writer changed the right side: the link of the current item takes it
    void takeLink();

    /// @brief The material of the link of item @p id: the material of the dialog (Targets),
    /// else @p id
    QString materialOf(const QString& id) const;

    /// @brief The element whose text the link of item @p id names a paragraph of: @p id
    /// (Targets), else the element of the dialog
    QString textOf(const QString& id) const;

    /// @brief The first paragraph of @p paragraphs (from 1) with the words of quote
    /// @p material; 0 when it is no quote or no paragraph has them
    int paragraphOfQuote(const QString& material, const QStringList& paragraphs) const;

    Mode m_mode;
    QString m_link;
    QStringList m_initial;
    QHash<QString, core::ElementLink> m_links;  ///< Item -> its link, also when unchecked
    QStringList m_paragraphs;  ///< The paragraphs of the text of the link shown
    QString m_shown;          ///< The item whose link the right side shows; empty: none
    bool m_showing = false;   ///< The right side is being filled, not changed by the writer
    QLineEdit* m_filter = nullptr;
    QTreeWidget* m_tree = nullptr;
    QGroupBox* m_linkGroup = nullptr;
    QComboBox* m_role = nullptr;
    QLabel* m_paragraphLabel = nullptr;
    QComboBox* m_paragraph = nullptr;
    QLineEdit* m_note = nullptr;
    QLabel* m_linkHint = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
