/// @file link_dialog.h
/// @brief Link: what a material is about, or the materials of an element of the book

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>
#include <QStringList>

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
/// Typing filters the tree also while the tree has the keys: the letters go to the filter
/// above it. The arrows choose, Space checks, Enter links and Esc closes. On a small screen
/// only the tree scrolls.
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

    Mode m_mode;
    QString m_link;
    QStringList m_initial;
    QLineEdit* m_filter = nullptr;
    QTreeWidget* m_tree = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
