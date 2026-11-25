// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file element_choice_dialog.cpp
/// @brief Choose an element of the book

#include "kalahari/gui/dialogs/element_choice_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int ID_ROLE = Qt::UserRole + 1;
constexpr int LEAST_WIDTH = 420;
constexpr int LEAST_HEIGHT = 300;

}  // namespace

ElementChoiceDialog::ElementChoiceDialog(const core::ProjectManager& book, const QString& title,
                                         const QString& description, const QString& current,
                                         const QStringList& excluded, QWidget* parent)
    : KalahariDialog(parent), m_book(book), m_excluded(excluded) {
    setHeading(title, description);
    setHeadingIcon(QStringLiteral("common.link"));

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("elementChoiceFilter"));
    m_filter->setClearButtonEnabled(true);
    //: The field above the list of the window that chooses an element. In Polish: Szukaj w
    //: tytułach
    m_filter->setPlaceholderText(tr("Search the titles"));
    //: In Polish: Filtr
    m_filter->setAccessibleName(tr("Filter"));
    contentLayout()->addWidget(m_filter);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("elementChoiceTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumSize(LEAST_WIDTH, LEAST_HEIGHT);
    m_tree->header()->setStretchLastSection(true);
    //: In Polish: Elementy książki
    m_tree->setAccessibleName(tr("Elements of the book"));
    contentLayout()->addWidget(m_tree, 1);

    if (const core::BookProject* project = book.project(); project != nullptr) {
        addBooks(*project);
        auto* workshop = new QTreeWidgetItem(m_tree);
        //: The node of the Workshop in the window that chooses an element. In Polish: Warsztat
        workshop->setText(0, project->workshop.name.isEmpty() ? tr("Workshop")
                                                              : project->workshop.name);
        workshop->setFlags(Qt::ItemIsEnabled);
        addElements(workshop, project->workshop.elements);
    }
    m_tree->expandAll();

    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it) {
        if ((*it)->data(0, ID_ROLE).toString() == current && !current.isEmpty()) {
            m_tree->setCurrentItem(*it);
            break;
        }
    }

    connect(m_filter, &QLineEdit::textChanged, this, &ElementChoiceDialog::applyFilter);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &ElementChoiceDialog::updateAccept);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (item != nullptr && (item->flags() & Qt::ItemIsSelectable)) {
            accept();
        }
    });
    m_tree->installEventFilter(this);
    m_filter->installEventFilter(this);
    //: The button of the window that chooses an element. In Polish: Wybierz
    setAcceptText(tr("Choose"));
    updateAccept();
    m_filter->setFocus();
}

QString ElementChoiceDialog::chosen() const {
    const QTreeWidgetItem* item = m_tree->currentItem();
    return item != nullptr && (item->flags() & Qt::ItemIsSelectable)
               ? item->data(0, ID_ROLE).toString()
               : QString();
}

void ElementChoiceDialog::addBooks(const core::BookProject& project) {
    // The book, or each volume of a series, as the Navigator names them
    const bool series = project.books.size() > 1;
    for (qsizetype index = 0; index < project.books.size(); ++index) {
        const core::ProjectBook& volume = project.books.at(index);
        auto* node = new QTreeWidgetItem(m_tree);
        //: The node of the book in the window that chooses an element. In Polish: Książka
        const QString bookName = volume.name.isEmpty() ? tr("Book") : volume.name;
        node->setText(0, series ? NavigatorPanel::volumeName(volume, index) : bookName);
        node->setFlags(Qt::ItemIsEnabled);
        for (const core::BookPlace place :
             {core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back}) {
            addElements(node, project.elementsIn(place, index));
        }
    }
}

void ElementChoiceDialog::addElements(QTreeWidgetItem* parent,
                                      const QList<core::ProjectElement>& elements) {
    for (const core::ProjectElement& element : elements) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, element.title);
        item->setData(0, ID_ROLE, element.id);
        item->setIcon(0, NavigatorPanel::iconOf(m_book, element, core::IconContext::TreeView));
        const bool choosable = !m_excluded.contains(element.id);
        item->setFlags(choosable ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);
        addElements(item, element.elements);
    }
}

void ElementChoiceDialog::applyFilter(const QString& text) {
    const QString wanted = text.trimmed();
    // An item shows when its title has the text, or one of the items inside it does
    std::function<bool(QTreeWidgetItem*)> show = [&show, &wanted](QTreeWidgetItem* item) {
        bool any = wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive);
        for (int i = 0; i < item->childCount(); ++i) {
            any = show(item->child(i)) || any;
        }
        item->setHidden(!any);
        return any;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = m_tree->topLevelItem(i);
        show(top);
        top->setHidden(false);
    }
    // The first element that shows is chosen, unless the chosen one shows
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current == nullptr || current->isHidden() || !(current->flags() & Qt::ItemIsSelectable)) {
        for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it) {
            if (!(*it)->isHidden() && ((*it)->flags() & Qt::ItemIsSelectable)) {
                m_tree->setCurrentItem(*it);
                break;
            }
        }
    }
    updateAccept();
}

void ElementChoiceDialog::updateAccept() {
    acceptButton()->setEnabled(!chosen().isEmpty());
}

bool ElementChoiceDialog::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (watched == m_filter && (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up)) {
            // The arrows choose in the list while the filter has the keys
            QCoreApplication::sendEvent(m_tree, event);
            return true;
        }
        if (watched == m_tree && !key->text().isEmpty() && key->text().at(0).isPrint() &&
            !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            // Typing in the list goes to the filter
            m_filter->setFocus();
            QCoreApplication::sendEvent(m_filter, event);
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
