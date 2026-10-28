/// @file link_dialog.cpp
/// @brief Link: what a material is about, or the materials of an element of the book

#include "kalahari/gui/dialogs/link_dialog.h"

#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"

#include <QCollator>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedValueRollback>
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

/// The excerpt of a paragraph of the list of paragraphs (Qt::UserRole is its number)
constexpr int EXCERPT_ROLE = Qt::UserRole + 1;

/// Rows of the tree it asks for, and those it keeps on a small screen
constexpr int TREE_ROWS = 14;
constexpr int MINIMUM_TREE_ROWS = 4;

/// Words of a paragraph in the list of paragraphs
constexpr int SHOWN_WORDS = 5;

/// Characters the list of paragraphs is wide enough for, and of a paragraph in its tooltip
constexpr int PARAGRAPH_CHARACTERS = 24;
constexpr int PARAGRAPH_TOOLTIP_CHARACTERS = 300;

/// Characters of a quote it finds in a text at least: fewer are in nearly every text
constexpr int MINIMUM_QUOTE = 8;

/// The words of quote @p material as a text has them: its description without the quotation
/// marks around it and with single spaces; empty for another material or a short quote
QString quoteWords(const core::ProjectElement* material) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::KindRef kind = material != nullptr ? pm.kindOf(*material) : core::KindRef();
    if (!kind || kind.kind->id != QLatin1String(core::MaterialKinds::QUOTE)) {
        return {};
    }
    static const QRegularExpression marks(
        QStringLiteral("^[\\s\"'\u201E\u201C\u201D\u00AB\u00BB]+|"
                       "[\\s\"'\u201E\u201C\u201D\u00AB\u00BB]+$"));
    const QString words = material->description.simplified().remove(marks);
    return words.size() < MINIMUM_QUOTE ? QString() : words;
}

/// Whether paragraph @p paragraph has @p words, a quote (quoteWords())
bool hasQuote(const QString& paragraph, const QString& words) {
    return !words.isEmpty() && paragraph.simplified().contains(words, Qt::CaseInsensitive);
}

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
                   //: In Polish: Zaznacz elementy, których dotyczy materiał, i powiedz, w jaki
                   //: sposób.
                   tr("Check the elements the material is about, and say how."));
        //: In Polish: &Powiąż
        setAcceptText(tr("&Link"));
    } else {
        //: The window that links materials with an element of the book or a section; %1: its
        //: title. In Polish: Materiały „%1”
        setHeading(tr("Materials of \"%1\"").arg(title),
                   //: In Polish: Zaznacz materiały, które go dotyczą, i powiedz, w jaki sposób.
                   tr("Check the materials about it, and say how."));
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

    m_tree = new LinkTree(this);
    m_tree->setObjectName(QStringLiteral("linkTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setUniformRowHeights(true);
    //: In Polish: Elementy do powiązania
    m_tree->setAccessibleName(tr("Elements to link"));

    // The link of the current item: its role, the paragraph and a note, each field with its
    // label above it
    //: The role, the paragraph and the note of the link of the item chosen in the tree. In
    //: Polish: Powiązanie
    m_linkGroup = new QGroupBox(tr("Link"), this);
    m_linkGroup->setObjectName(QStringLiteral("linkDetails"));
    auto* linkLayout = new QVBoxLayout(m_linkGroup);
    linkLayout->setSpacing(4);
    const auto addLinkField = [this, linkLayout](const QString& text, QWidget* field) {
        auto* label = new QLabel(text, m_linkGroup);
        label->setBuddy(field);
        linkLayout->addWidget(label);
        linkLayout->addWidget(field);
        linkLayout->addSpacing(4);
        return label;
    };
    m_role = new QComboBox(m_linkGroup);
    m_role->setObjectName(QStringLiteral("linkRole"));
    //: The link says nothing of how. In Polish: Bez rodzaju
    m_role->addItem(tr("No role"), QString());
    for (const QString& role : core::LinkRoles::all()) {
        m_role->addItem(linkRoleName(role), role);
    }
    //: How the material is linked with the element. In Polish: &Rodzaj powiązania
    addLinkField(tr("&Role"), m_role);
    m_paragraph = new QComboBox(m_linkGroup);
    m_paragraph->setObjectName(QStringLiteral("linkParagraph"));
    m_paragraph->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_paragraph->setMinimumContentsLength(PARAGRAPH_CHARACTERS);
    //: The paragraph of the text the link names. In Polish: &Miejsce w tekście
    m_paragraphLabel = addLinkField(tr("Place in the &text"), m_paragraph);
    m_note = new QLineEdit(m_linkGroup);
    m_note->setObjectName(QStringLiteral("linkNote"));
    //: In Polish: Krótka notatka...
    m_note->setPlaceholderText(tr("A short note..."));
    m_note->setClearButtonEnabled(true);
    //: In Polish: &Notatka
    addLinkField(tr("&Note"), m_note);
    m_linkHint = new QLabel(m_linkGroup);
    m_linkHint->setObjectName(QStringLiteral("linkHint"));
    m_linkHint->setWordWrap(true);
    m_linkHint->setForegroundRole(QPalette::PlaceholderText);
    linkLayout->addWidget(m_linkHint);
    linkLayout->addStretch(1);

    // The tree with its filter at the left, the link at the right
    auto* treeColumn = new QVBoxLayout();
    treeColumn->setSpacing(6);
    treeColumn->addWidget(m_filter);
    treeColumn->addWidget(m_tree, 1);
    auto* columns = new QHBoxLayout();
    columns->setSpacing(11);
    columns->addLayout(treeColumn, 3);
    columns->addWidget(m_linkGroup, 2);
    contentLayout()->addLayout(columns, 1);

    if (mode == Mode::Targets) {
        fillTargets(link);
    } else {
        fillMaterials(link);
    }
    m_initial = checkedLinks();

    connect(m_filter, &QLineEdit::textChanged, this, &LinkDialog::filter);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column == 0) {
            itemChecked(item);
        }
    });
    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current) { showLink(current); });
    connect(m_role, &QComboBox::currentIndexChanged, this, &LinkDialog::takeLink);
    connect(m_paragraph, &QComboBox::currentIndexChanged, this, &LinkDialog::takeLink);
    connect(m_note, &QLineEdit::textEdited, this, &LinkDialog::takeLink);
    m_tree->installEventFilter(this);
    m_filter->installEventFilter(this);

    // The first item that can be checked is the current one, and the tree has the keys
    for (QTreeWidgetItem* item : allItems(m_tree)) {
        if (item->flags() & Qt::ItemIsUserCheckable) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
    showLink(m_tree->currentItem());
    m_tree->setFocus(Qt::OtherFocusReason);
}

void LinkDialog::fillTargets(const QString& material) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (project == nullptr) {
        return;
    }
    const core::ProjectElement* element = project->findElement(material);
    const QStringList links = element ? element->linkTargets() : QStringList();
    if (element != nullptr) {
        for (const core::ElementLink& each : element->links) {
            m_links.insert(each.to, each);
        }
    }
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
            const core::ElementLink* linked = each->linkTo(link);
            checkItem(groupNode, each->id, linked != nullptr);
            if (linked != nullptr) {
                m_links.insert(each->id, *linked);
            }
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

core::ElementLink LinkDialog::linkOf(const QString& id) const {
    core::ElementLink link = m_links.contains(id)
                                 ? m_links.value(id)
                                 : core::ProjectManager::getInstance().newLink(materialOf(id), id);
    link.to = m_mode == Mode::Targets ? id : m_link;
    return link;
}

void LinkDialog::setCurrentLink(const QString& id) {
    if (QTreeWidgetItem* item = itemOf(id)) {
        m_tree->setCurrentItem(item);
        m_tree->scrollToItem(item);
    }
}

QString LinkDialog::materialOf(const QString& id) const {
    return m_mode == Mode::Targets ? m_link : id;
}

QString LinkDialog::textOf(const QString& id) const {
    return m_mode == Mode::Targets ? id : m_link;
}

int LinkDialog::paragraphOfQuote(const QString& material, const QStringList& paragraphs) const {
    const QString words = quoteWords(core::ProjectManager::getInstance().findElement(material));
    for (qsizetype i = 0; i < paragraphs.size(); ++i) {
        if (hasQuote(paragraphs.at(i), words)) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

void LinkDialog::itemChecked(QTreeWidgetItem* item) {
    if (item == nullptr || !(item->flags() & Qt::ItemIsUserCheckable)) {
        return;
    }
    const QString id = item->data(0, LINK_ROLE).toString();
    if (item->checkState(0) == Qt::Checked && !m_links.contains(id)) {
        // A new link: the role its kind suggests, and a quote the paragraph with its words
        const auto& pm = core::ProjectManager::getInstance();
        core::ElementLink link =
            pm.newLink(materialOf(id), m_mode == Mode::Targets ? id : m_link);
        if (core::LinkRoles::paragraphsShown()) {
            const QStringList paragraphs = pm.paragraphsOf(textOf(id));
            link.paragraph = paragraphOfQuote(materialOf(id), paragraphs);
            if (link.paragraph > 0) {
                link.excerpt = core::LinkRoles::excerptOf(paragraphs.at(link.paragraph - 1));
            }
        }
        m_links.insert(id, link);
    }
    if (item == m_tree->currentItem()) {
        showLink(item);
    }
}

void LinkDialog::showLink(QTreeWidgetItem* item) {
    const QScopedValueRollback<bool> showing(m_showing, true);
    const bool checkable = item != nullptr && (item->flags() & Qt::ItemIsUserCheckable);
    const bool checked = checkable && item->checkState(0) == Qt::Checked;
    const QString id = checked ? item->data(0, LINK_ROLE).toString() : QString();
    m_shown = id;
    const core::ElementLink link = checked ? linkOf(id) : core::ElementLink();

    // A role of a later version: shown as it is, and kept, while its link is shown
    const int known = static_cast<int>(core::LinkRoles::all().size()) + 1;
    while (m_role->count() > known) {
        m_role->removeItem(known);
    }
    int role = m_role->findData(link.role);
    if (role < 0) {
        m_role->addItem(link.role, link.role);
        role = known;
    }
    m_role->setCurrentIndex(role);
    fillParagraphs(id, link);
    m_note->setText(link.note);

    // The paragraph found by its excerpt, after paragraphs before it were added or taken out
    const int paragraph = m_paragraph->currentData().toInt();
    if (checked && link.paragraph > 0 && paragraph > 0 && paragraph != link.paragraph) {
        m_links[id].paragraph = paragraph;
        m_links[id].excerpt = m_paragraph->currentData(EXCERPT_ROLE).toString();
    }

    const bool places = core::LinkRoles::paragraphsShown();
    m_paragraphLabel->setVisible(places);
    m_paragraph->setVisible(places);
    m_role->setEnabled(checked);
    m_paragraph->setEnabled(checked && m_paragraph->count() > 1);
    m_note->setEnabled(checked);

    QString hint;
    if (!checkable) {
        //: In Polish: Wybierz element w drzewie.
        hint = tr("Choose an element in the tree.");
    } else if (!checked) {
        //: In Polish: Zaznacz pole przy elemencie, żeby ustawić powiązanie.
        hint = tr("Check the box of the element to set its link.");
    } else if (places) {
        const int quote = paragraphOfQuote(materialOf(id), m_paragraphs);
        if (quote > 0) {
            //: In Polish: Ten cytat jest w akapicie %1.
            hint = tr("The quote is in paragraph %1.").arg(quote);
        }
    }
    m_linkHint->setText(hint);
    m_linkHint->setVisible(!hint.isEmpty());
}

void LinkDialog::fillParagraphs(const QString& id, const core::ElementLink& link) {
    m_paragraph->clear();
    m_paragraphs.clear();
    //: The link names no paragraph of the text. In Polish: Cały element
    m_paragraph->addItem(tr("The whole element"), 0);
    if (id.isEmpty() || !core::LinkRoles::paragraphsShown()) {
        m_paragraph->setCurrentIndex(0);
        return;
    }
    const auto& pm = core::ProjectManager::getInstance();
    m_paragraphs = pm.paragraphsOf(textOf(id));
    const QString words = quoteWords(pm.findElement(materialOf(id)));
    for (qsizetype i = 0; i < m_paragraphs.size(); ++i) {
        const QString text = m_paragraphs.at(i).simplified();
        if (text.isEmpty()) {
            continue;  // An empty paragraph has nothing to link
        }
        const QString excerpt = core::LinkRoles::excerptOf(text);
        //: A paragraph of a text in the Link window: %1 its number, %2 its first words. In
        //: Polish: Akapit %1: „%2…”
        QString label = tr("Paragraph %1: \"%2...\"")
                            .arg(i + 1)
                            .arg(excerpt.section(QLatin1Char(' '), 0, SHOWN_WORDS - 1));
        if (hasQuote(text, words)) {
            //: A paragraph with the words of the quote being linked. In Polish: %1 – jest w
            //: nim ten cytat
            label = tr("%1 – the quote is in it").arg(label);
        }
        m_paragraph->addItem(label, static_cast<int>(i) + 1);
        m_paragraph->setItemData(m_paragraph->count() - 1, excerpt, EXCERPT_ROLE);
        m_paragraph->setItemData(m_paragraph->count() - 1,
                                 text.left(PARAGRAPH_TOOLTIP_CHARACTERS), Qt::ToolTipRole);
    }
    const int index = m_paragraph->findData(core::LinkRoles::paragraphOf(m_paragraphs, link));
    m_paragraph->setCurrentIndex(std::max(index, 0));
}

void LinkDialog::takeLink() {
    if (m_showing || m_shown.isEmpty()) {
        return;
    }
    core::ElementLink link = linkOf(m_shown);
    link.role = m_role->currentData().toString();
    if (core::LinkRoles::paragraphsShown()) {
        link.paragraph = m_paragraph->currentData().toInt();
        link.excerpt = m_paragraph->currentData(EXCERPT_ROLE).toString();
    }
    link.note = m_note->text().trimmed();
    m_links.insert(m_shown, link);
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

std::optional<QList<core::ElementLink>> chooseLinks(const QString& elementId,
                                                    const QString& currentLink, QWidget* parent) {
    LinkDialog dialog(LinkDialog::Mode::Targets, elementId, parent);
    if (!currentLink.isEmpty()) {
        dialog.setCurrentLink(currentLink);
    }
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    if (element == nullptr) {
        return std::nullopt;
    }
    const QStringList checked = dialog.checkedLinks();
    QStringList targets;
    for (const QString& target : element->linkTargets()) {
        if (checked.contains(target)) {
            targets << target;
        }
    }
    for (const QString& target : checked) {
        if (!targets.contains(target)) {
            targets << target;
        }
    }
    QList<core::ElementLink> links;
    for (const QString& target : targets) {
        links << dialog.linkOf(target);
    }
    return links;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
