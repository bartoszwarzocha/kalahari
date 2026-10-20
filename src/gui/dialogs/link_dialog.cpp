/// @file link_dialog.cpp
/// @brief Link: what a material is about, or the materials of an element of the book

#include "kalahari/gui/dialogs/link_dialog.h"

#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"

#include <QCollator>
#include <QCoreApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <iterator>
#include <numeric>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The link of an item that can be checked; empty for the book, the Workshop and its groups
constexpr int LINK_ROLE = Qt::UserRole;

/// Rows of the tree it asks for, and those it keeps on a small screen
constexpr int TREE_ROWS = 14;
constexpr int MINIMUM_TREE_ROWS = 4;

/// The tree of the dialog: high enough for a book, and low on a small screen, where only it
/// scrolls
class LinkTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    QSize sizeHint() const override {
        return {QTreeWidget::sizeHint().width(), heightFor(TREE_ROWS)};
    }

    QSize minimumSizeHint() const override {
        return {QTreeWidget::minimumSizeHint().width(), heightFor(MINIMUM_TREE_ROWS)};
    }

private:
    int heightFor(int rows) const {
        const int row = std::max(fontMetrics().height() + 6, iconSize().height() + 4);
        return rows * row + 2 * frameWidth();
    }
};

/// An item that can be checked, of link @p link
QTreeWidgetItem* checkItem(QTreeWidgetItem* parent, const QString& link, bool checked) {
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, linkTitle(link));
    item->setIcon(0, linkIcon(link, core::IconContext::TreeView));
    item->setData(0, LINK_ROLE, link);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

/// An item that groups others and cannot be checked: the book, the Workshop, a group of it
QTreeWidgetItem* groupItem(QTreeWidgetItem* parent, const QString& title, const QIcon& icon) {
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, title);
    item->setIcon(0, icon);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setExpanded(true);
    return item;
}

/// The items of @p tree in its order, each before the items inside it
QList<QTreeWidgetItem*> allItems(const QTreeWidget* tree) {
    QList<QTreeWidgetItem*> items;
    const std::function<void(QTreeWidgetItem*)> add = [&](QTreeWidgetItem* item) {
        items << item;
        for (int i = 0; i < item->childCount(); ++i) {
            add(item->child(i));
        }
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        add(tree->topLevelItem(i));
    }
    return items;
}

/// The elements of the Workshop of @p project, of the materials only when @p materials, in
/// groups of their kinds in the order of the type, each in the order of their titles
struct KindGroup {
    QString name;
    QIcon icon;
    QList<const core::ProjectElement*> elements;
};

QList<KindGroup> workshopGroups(const core::BookProject& project, bool materials,
                                const QString& without) {
    const auto& pm = core::ProjectManager::getInstance();
    QList<KindGroup> groups;
    QList<QString> keys;
    for (const core::ProjectElement& element : project.workshop.elements) {
        if (element.id == without || (materials && !pm.isMaterial(element))) {
            continue;
        }
        const core::KindRef kind = pm.kindOf(element);
        const QString key = kind ? kind.reference() : element.kind.toString();
        qsizetype index = keys.indexOf(key);
        if (index < 0) {
            keys << key;
            groups.append({core::fillWords(QStringLiteral("{Kind:plural}"),
                                           {{QStringLiteral("kind"), wordsOf(element)}}),
                           NavigatorPanel::iconOf(pm.bookTypes(), element,
                                                  core::IconContext::TreeView),
                           {}});
            index = groups.size() - 1;
        }
        groups[index].elements << &element;
    }
    // The groups in the order of the kinds of the type, as the Navigator has them
    const QList<core::KindRef> order = project.kindsIn(pm.bookTypes(), core::BookPlace::Workshop);
    const auto rank = [&order](const QString& key) {
        const auto found =
            std::find_if(order.cbegin(), order.cend(),
                         [&key](const core::KindRef& kind) { return kind.reference() == key; });
        return found == order.cend() ? order.size() : std::distance(order.cbegin(), found);
    };
    QList<qsizetype> indexes(groups.size());
    std::iota(indexes.begin(), indexes.end(), 0);
    std::stable_sort(indexes.begin(), indexes.end(), [&](qsizetype a, qsizetype b) {
        return rank(keys.at(a)) < rank(keys.at(b));
    });
    QList<KindGroup> sorted;
    for (qsizetype index : indexes) {
        sorted << groups.at(index);
    }

    const core::ProjectBook* book = project.books.isEmpty() ? nullptr : &project.books.first();
    QCollator collator(QLocale(book && !book->language.isEmpty() ? book->language
                                                                 : programLanguage()));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    for (KindGroup& group : sorted) {
        std::stable_sort(group.elements.begin(), group.elements.end(),
                         [&collator](const core::ProjectElement* a, const core::ProjectElement* b) {
                             return collator.compare(a->title, b->title) < 0;
                         });
    }
    return sorted;
}

} // namespace

LinkDialog::LinkDialog(Mode mode, const QString& link, QWidget* parent)
    : KalahariDialog(parent), m_mode(mode), m_link(link) {
    setObjectName(QStringLiteral("linkDialog"));
    const QString title = linkTitle(link);
    if (mode == Mode::Targets) {
        //: The window that links a material with what it is about. In Polish: Powiąż
        //: materiał „%1”
        setHeading(tr("Link Material \"%1\"").arg(title),
                   //: In Polish: Zaznacz elementy, których dotyczy materiał.
                   tr("Check the elements the material is about."));
        //: In Polish: &Powiąż
        setAcceptText(tr("&Link"));
    } else {
        //: The window that links materials with an element of the book or a section; %1: its
        //: title. In Polish: Materiały „%1”
        setHeading(tr("Materials of \"%1\"").arg(title),
                   //: In Polish: Zaznacz materiały, które go dotyczą.
                   tr("Check the materials about it."));
        setAcceptText(tr("&Link"));
    }
    setHeadingIcon(QStringLiteral("common.link"));
    setCompactHeading(true);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("linkFilter"));
    //: In Polish: Filtruj...
    m_filter->setPlaceholderText(tr("Filter..."));
    m_filter->setClearButtonEnabled(true);
    //: In Polish: Filtr
    m_filter->setAccessibleName(tr("Filter"));
    contentLayout()->addWidget(m_filter);

    m_tree = new LinkTree(this);
    m_tree->setObjectName(QStringLiteral("linkTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setUniformRowHeights(true);
    //: In Polish: Elementy do powiązania
    m_tree->setAccessibleName(tr("Elements to link"));
    contentLayout()->addWidget(m_tree, 1);

    if (mode == Mode::Targets) {
        fillTargets(link);
    } else {
        fillMaterials(link);
    }
    m_initial = checkedLinks();

    connect(m_filter, &QLineEdit::textChanged, this, &LinkDialog::filter);
    m_tree->installEventFilter(this);
    m_filter->installEventFilter(this);

    // The first item that can be checked is the current one, and the tree has the keys
    for (QTreeWidgetItem* item : allItems(m_tree)) {
        if (item->flags() & Qt::ItemIsUserCheckable) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
    m_tree->setFocus(Qt::OtherFocusReason);
}

void LinkDialog::fillTargets(const QString& material) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return;
    }
    const core::ProjectElement* element = project->findElement(material);
    const QStringList links = element ? element->links : QStringList();
    auto& art = core::ArtProvider::getInstance();

    // The book: its sections and their elements, in reading order
    for (qsizetype index = 0; index < project->books.size(); ++index) {
        const core::ProjectBook& book = project->books.at(index);
        QTreeWidgetItem* bookItem = groupItem(
            m_tree->invisibleRootItem(), NavigatorPanel::bookName(book),
            art.getIcon(QStringLiteral("structure.book"), core::IconContext::TreeView));
        for (core::BookPlace place :
             {core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back}) {
            const QString section = project->sectionLink(place, index);
            QTreeWidgetItem* sectionItem =
                checkItem(bookItem, section, links.contains(section));
            const std::function<void(QTreeWidgetItem*, const QList<core::ProjectElement>&)> add =
                [&](QTreeWidgetItem* parent, const QList<core::ProjectElement>& elements) {
                    for (const core::ProjectElement& each : elements) {
                        QTreeWidgetItem* item = checkItem(parent, each.id, links.contains(each.id));
                        add(item, each.elements);
                        item->setExpanded(true);
                    }
                };
            add(sectionItem, project->elementsIn(place, index));
            sectionItem->setExpanded(true);
        }
        bookItem->setExpanded(true);
    }

    // The Workshop: its elements in groups of their kinds, without the material itself
    QTreeWidgetItem* workshopItem = groupItem(
        m_tree->invisibleRootItem(), NavigatorPanel::workshopName(*project),
        art.getIcon(QStringLiteral("structure.workshop"), core::IconContext::TreeView));
    for (const KindGroup& group : workshopGroups(*project, false, material)) {
        QTreeWidgetItem* groupNode = groupItem(workshopItem, group.name, group.icon);
        for (const core::ProjectElement* each : group.elements) {
            checkItem(groupNode, each->id, links.contains(each->id));
        }
    }
    workshopItem->setExpanded(true);
}

void LinkDialog::fillMaterials(const QString& link) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return;
    }
    auto& art = core::ArtProvider::getInstance();
    QTreeWidgetItem* workshopItem = groupItem(
        m_tree->invisibleRootItem(), NavigatorPanel::workshopName(*project),
        art.getIcon(QStringLiteral("structure.workshop"), core::IconContext::TreeView));
    for (const KindGroup& group : workshopGroups(*project, true, QString())) {
        QTreeWidgetItem* groupNode = groupItem(workshopItem, group.name, group.icon);
        for (const core::ProjectElement* each : group.elements) {
            checkItem(groupNode, each->id, each->links.contains(link));
        }
    }
    workshopItem->setExpanded(true);
}

QStringList LinkDialog::checkedLinks() const {
    QStringList links;
    for (const QTreeWidgetItem* item : allItems(m_tree)) {
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState(0) == Qt::Checked) {
            links << item->data(0, LINK_ROLE).toString();
        }
    }
    return links;
}

QTreeWidgetItem* LinkDialog::itemOf(const QString& link) const {
    for (QTreeWidgetItem* item : allItems(m_tree)) {
        if (!link.isEmpty() && item->data(0, LINK_ROLE).toString() == link) {
            return item;
        }
    }
    return nullptr;
}

void LinkDialog::filter(const QString& text) {
    const QString wanted = text.trimmed();
    // An item is shown when its title has the text, or an item inside it has it; the items
    // inside one that has it are shown too
    const std::function<bool(QTreeWidgetItem*, bool)> show = [&](QTreeWidgetItem* item,
                                                                 bool shownAbove) {
        const bool matches =
            wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive);
        bool inside = false;
        for (int i = 0; i < item->childCount(); ++i) {
            inside = show(item->child(i), shownAbove || matches) || inside;
        }
        const bool shown = matches || inside || shownAbove;
        item->setHidden(!shown);
        return matches || inside;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        show(m_tree->topLevelItem(i), false);
    }
    // The current item is one that is shown and matches
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current == nullptr || current->isHidden() ||
        (!wanted.isEmpty() && !current->text(0).contains(wanted, Qt::CaseInsensitive))) {
        for (QTreeWidgetItem* item : allItems(m_tree)) {
            if (!item->isHidden() && (item->flags() & Qt::ItemIsUserCheckable) &&
                (wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive))) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }
}

bool LinkDialog::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress) {
        return KalahariDialog::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    // In the tree, the letters go to the filter and Backspace takes the last one back; Space
    // checks
    if (watched == m_tree) {
        if (key->key() == Qt::Key_Backspace && modifiers == Qt::NoModifier) {
            m_filter->backspace();
            return true;
        }
        const QString text = key->text();
        const bool letter = !text.isEmpty() && text.at(0).isPrint() && !text.at(0).isSpace() &&
                            (modifiers & ~Qt::ShiftModifier) == Qt::NoModifier;
        if (letter) {
            m_filter->insert(text);
            return true;
        }
    }
    // In the filter, the arrows choose in the tree
    if (watched == m_filter && modifiers == Qt::NoModifier &&
        (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up ||
         key->key() == Qt::Key_PageDown || key->key() == Qt::Key_PageUp)) {
        m_tree->setFocus(Qt::OtherFocusReason);
        QKeyEvent copy(QEvent::KeyPress, key->key(), key->modifiers());
        QCoreApplication::sendEvent(m_tree, &copy);
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
