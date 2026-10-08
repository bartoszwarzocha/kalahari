/// @file annotations_panel.cpp
/// @brief The Annotations panel: the comments, TODOs and notes of a chapter or the book

#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"

#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The kinds in the order of the kind buttons
constexpr std::array<editor::AnnotationKind, 3> KINDS = {
    editor::AnnotationKind::Comment, editor::AnnotationKind::Todo, editor::AnnotationKind::Note};

/// @brief The index of a kind among the kind buttons
std::size_t kindIndex(editor::AnnotationKind kind) {
    return static_cast<std::size_t>(std::find(KINDS.begin(), KINDS.end(), kind) - KINDS.begin());
}

/// @brief The panel's look: kind buttons as rounded chips, joined buttons of a choice.
/// Colors come from the palette of the theme.
const char* const PANEL_STYLE = R"(
QToolButton#annotationKindButton {
    border: 1px solid palette(mid); border-radius: 10px; padding: 2px 8px;
    background: palette(base);
}
QToolButton#annotationKindButton:!checked { color: palette(mid); background: transparent; }
QToolButton[segment] {
    border: 1px solid palette(mid); padding: 2px 8px; background: palette(base);
}
QToolButton[segment="first"] { border-top-left-radius: 6px; border-bottom-left-radius: 6px; }
QToolButton[segment="middle"] { border-left: none; }
QToolButton[segment="last"] {
    border-left: none; border-top-right-radius: 6px; border-bottom-right-radius: 6px;
}
QToolButton[segment]:checked { background: palette(highlight); color: palette(highlighted-text); }
QToolButton[segment]:disabled { color: palette(mid); }
)";

/// @brief Joined buttons of which one is on, numbered from 0 in the group
QHBoxLayout* choiceButtons(QWidget* parent, QButtonGroup* group, const QStringList& texts) {
    auto* layout = new QHBoxLayout();
    layout->setSpacing(0);
    for (int i = 0; i < texts.size(); ++i) {
        auto* button = new QToolButton(parent);
        button->setText(texts.at(i));
        button->setCheckable(true);
        const char* segment = i == 0 ? "first" : (i == texts.size() - 1 ? "last" : "middle");
        button->setProperty("segment", QString::fromLatin1(segment));
        group->addButton(button, i);
        layout->addWidget(button);
    }
    group->setExclusive(true);
    return layout;
}

/// @brief A dot in a kind's color, for its button
QIcon kindDot(const QColor& color, qreal devicePixelRatio) {
    constexpr int SIZE = 8;
    QPixmap pixmap(QSize(SIZE, SIZE) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(0, 0, SIZE, SIZE));
    return QIcon(pixmap);
}

}  // namespace

AnnotationsPanel::AnnotationsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AnnotationsPanel"));
    setStyleSheet(QString::fromLatin1(PANEL_STYLE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Search in the text of the annotations
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search the annotations..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AnnotationsPanel::readFilters);
    layout->addWidget(m_searchEdit);

    // Kinds: on or off, with their counts
    auto* kinds = new QHBoxLayout();
    kinds->setSpacing(5);
    for (QToolButton*& button : m_kindButtons) {
        button = new QToolButton(this);
        button->setObjectName(QStringLiteral("annotationKindButton"));
        button->setCheckable(true);
        button->setChecked(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(button, &QToolButton::toggled, this, &AnnotationsPanel::readFilters);
        kinds->addWidget(button);
    }
    kinds->addStretch(1);
    layout->addLayout(kinds);

    // State
    m_stateGroup = new QButtonGroup(this);
    auto* states = choiceButtons(this, m_stateGroup, {tr("Open"), tr("Done"), tr("All")});
    m_stateGroup->button(static_cast<int>(AnnotationStateFilter::Open))->setChecked(true);
    connect(m_stateGroup, &QButtonGroup::idClicked, this, &AnnotationsPanel::readFilters);
    states->addStretch(1);
    layout->addLayout(states);

    // Scope and date
    m_scopeGroup = new QButtonGroup(this);
    auto* scopeRow = choiceButtons(this, m_scopeGroup, {tr("Chapter"), tr("Book")});
    m_scopeGroup->button(static_cast<int>(AnnotationScope::Chapter))->setChecked(true);
    m_bookScopeButton = m_scopeGroup->button(static_cast<int>(AnnotationScope::Book));
    m_bookScopeButton->setEnabled(false);
    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto scope = static_cast<AnnotationScope>(id);
        if (scope != m_scope) {
            m_scope = scope;
            emit scopeChanged(scope);
            rebuild();
        }
    });
    scopeRow->addSpacing(6);
    m_dateCombo = new QComboBox(this);
    m_dateCombo->addItems({tr("Date: any"), tr("Date: today"), tr("Date: last 7 days"),
                           tr("Date: last 30 days")});
    connect(m_dateCombo, &QComboBox::currentIndexChanged, this, &AnnotationsPanel::readFilters);
    scopeRow->addWidget(m_dateCombo, 1);
    layout->addLayout(scopeRow);

    // Order
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({tr("Sort: text order"), tr("Sort: newest first")});
    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, &AnnotationsPanel::readFilters);
    layout->addWidget(m_sortCombo);

    // The cards; a click on one gives the keys to the list
    m_listWidget = new QWidget();
    m_listWidget->setFocusPolicy(Qt::StrongFocus);
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(6);
    m_emptyLabel = new QLabel(m_listWidget);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setForegroundRole(QPalette::PlaceholderText);
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch(1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_listWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_scrollArea, 1);

    // Previous and next TODO
    auto* todoRow = new QHBoxLayout();
    todoRow->setSpacing(6);
    m_previousTodoButton = new QToolButton(this);
    m_nextTodoButton = new QToolButton(this);
    for (QToolButton* button : {m_previousTodoButton, m_nextTodoButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        todoRow->addWidget(button);
    }
    m_nextTodoButton->setLayoutDirection(Qt::RightToLeft);  // its arrow after the text
    m_previousTodoButton->hide();
    m_nextTodoButton->hide();
    layout->addLayout(todoRow);

    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &AnnotationsPanel::applyTheme);
    applyTheme(themes.getCurrentTheme());
}

AnnotationsPanel::~AnnotationsPanel() {
    // The cards say nothing while they go (an edited one would report its end)
    for (AnnotationCard* card : std::as_const(m_cards)) {
        card->disconnect(this);
    }
}

void AnnotationsPanel::setEntries(const std::vector<AnnotationEntry>& entries) {
    m_entries = entries;
    rebuild();
}

std::vector<AnnotationEntry> AnnotationsPanel::shownEntries() const {
    std::vector<AnnotationEntry> shown;
    for (int i = 0; i < m_listLayout->count(); ++i) {
        if (const auto* shownCard = qobject_cast<AnnotationCard*>(m_listLayout->itemAt(i)->widget())) {
            shown.push_back(shownCard->entry());
        }
    }
    return shown;
}

void AnnotationsPanel::setScope(AnnotationScope scope) {
    m_scope = scope;
    m_scopeGroup->button(static_cast<int>(scope))->setChecked(true);
}

void AnnotationsPanel::setBookScopeAvailable(bool available) {
    m_bookScopeButton->setEnabled(available);
    if (!available && m_scope == AnnotationScope::Book) {
        setScope(AnnotationScope::Chapter);
    }
}

void AnnotationsPanel::setDocumentAvailable(bool available) {
    if (available != m_documentAvailable) {
        m_documentAvailable = available;
        updateEmptyText(!m_cards.isEmpty());
    }
}

void AnnotationsPanel::setFilter(const AnnotationFilter& filter) {
    {
        const QScopedValueRollback<bool> updating(m_updatingControls, true);
        m_searchEdit->setText(filter.text);
        m_kindButtons[0]->setChecked(filter.comments);
        m_kindButtons[1]->setChecked(filter.todos);
        m_kindButtons[2]->setChecked(filter.notes);
        m_stateGroup->button(static_cast<int>(filter.state))->setChecked(true);
        m_dateCombo->setCurrentIndex(static_cast<int>(filter.date));
    }
    m_filter = filter;
    rebuild();
}

void AnnotationsPanel::setSort(AnnotationSort sort) {
    {
        const QScopedValueRollback<bool> updating(m_updatingControls, true);
        m_sortCombo->setCurrentIndex(static_cast<int>(sort));
    }
    m_sort = sort;
    rebuild();
}

void AnnotationsPanel::setTodoActions(QAction* previous, QAction* next) {
    m_previousTodoButton->setDefaultAction(previous);
    m_nextTodoButton->setDefaultAction(next);
    m_previousTodoButton->setVisible(previous != nullptr);
    m_nextTodoButton->setVisible(next != nullptr);
}

void AnnotationsPanel::selectAnnotation(const QString& key) {
    m_selectedKey = m_cards.contains(key) ? key : QString();
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        it.value()->setSelected(it.key() == m_selectedKey);
    }
    if (AnnotationCard* selected = m_cards.value(m_selectedKey)) {
        m_scrollArea->ensureWidgetVisible(selected);
    }
}

void AnnotationsPanel::editAnnotation(const QString& key) {
    const auto entry = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                    [&key](const AnnotationEntry& e) { return e.key() == key; });
    if (entry == m_entries.cend()) {
        return;
    }

    // Ease the filters that would hide it
    if (!matches(*entry, m_filter, QDateTime::currentDateTime())) {
        AnnotationFilter eased = m_filter;
        if (!entry->annotation.text.contains(eased.text.trimmed(), Qt::CaseInsensitive)) {
            eased.text.clear();
        }
        eased.comments = eased.comments || entry->annotation.kind == editor::AnnotationKind::Comment;
        eased.todos = eased.todos || entry->annotation.kind == editor::AnnotationKind::Todo;
        eased.notes = eased.notes || entry->annotation.kind == editor::AnnotationKind::Note;
        AnnotationFilter stateAndDate;  // what passes for any state and date
        stateAndDate.state = eased.state;
        stateAndDate.date = eased.date;
        if (!matchesExceptKind(*entry, stateAndDate, QDateTime::currentDateTime())) {
            eased.state = AnnotationStateFilter::All;
            eased.date = AnnotationDateFilter::Any;
        }
        setFilter(eased);
    }

    selectAnnotation(key);
    if (AnnotationCard* edited = m_cards.value(key)) {
        edited->startEditing();
    }
}

AnnotationCard* AnnotationsPanel::card(const QString& key) const {
    return m_cards.value(key);
}

void AnnotationsPanel::focusList() {
    m_listWidget->setFocus(Qt::OtherFocusReason);
}

void AnnotationsPanel::applyTheme(const core::Theme& theme) {
    // A dark window gets the colors made for dark paper, and a stronger tint
    const QColor window = theme.palette.window.isValid() ? theme.palette.window
                                                         : palette().color(QPalette::Window);
    const bool dark = window.lightness() < 128;
    const QColor fallback = palette().color(QPalette::Highlight);
    auto& themes = core::ThemeManager::getInstance();
    for (std::size_t i = 0; i < KINDS.size(); ++i) {
        m_kindColors.at(i) = themes.editorColor(editor::annotationColorKey(KINDS.at(i), dark), fallback);
        m_kindButtons.at(i)->setIcon(kindDot(m_kindColors.at(i), devicePixelRatioF()));
    }
    m_cardBase = theme.palette.base.isValid() ? theme.palette.base : palette().color(QPalette::Base);
    m_cardTint = dark ? 0.12 : 0.06;

    for (AnnotationCard* shownCard : std::as_const(m_cards)) {
        colorCard(shownCard);
    }
}

void AnnotationsPanel::keyPressEvent(QKeyEvent* event) {
    AnnotationCard* selected = m_cards.value(m_selectedKey);
    switch (event->key()) {
    case Qt::Key_Up:
        selectNeighbour(-1);
        return;
    case Qt::Key_Down:
        selectNeighbour(1);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (selected != nullptr) {
            selected->startEditing();
            return;
        }
        break;
    case Qt::Key_Delete:
        if (selected != nullptr) {
            // The card after it (or before it, for the last one) is selected next
            const std::vector<AnnotationEntry> shown = shownEntries();
            const auto it = std::find_if(shown.cbegin(), shown.cend(), [this](const AnnotationEntry& e) {
                return e.key() == m_selectedKey;
            });
            QString neighbour;
            if (it != shown.cend() && std::next(it) != shown.cend()) {
                neighbour = std::next(it)->key();
            } else if (it != shown.cend() && it != shown.cbegin()) {
                neighbour = std::prev(it)->key();
            }
            const AnnotationEntry entry = selected->entry();
            emit deleteRequested(entry);
            if (!m_cards.contains(entry.key()) && m_cards.contains(neighbour)) {
                selectAnnotation(neighbour);
            }
            return;
        }
        break;
    case Qt::Key_Escape:
        emit editorFocusRequested();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void AnnotationsPanel::rebuild() {
    const QDateTime now = QDateTime::currentDateTime();

    // What passes the filters; the card being edited stays
    std::vector<AnnotationEntry> shown;
    for (const AnnotationEntry& entry : m_entries) {
        const AnnotationCard* existing = m_cards.value(entry.key());
        if ((existing != nullptr && existing->isEditing()) || matches(entry, m_filter, now)) {
            shown.push_back(entry);
        }
    }
    sortEntries(shown, m_sort);

    // Cards of the entries shown again stay as they are
    QHash<QString, AnnotationCard*> cards;
    for (const AnnotationEntry& entry : shown) {
        AnnotationCard* entryCard = m_cards.take(entry.key());
        if (entryCard == nullptr) {
            entryCard = createCard();
        }
        entryCard->setEntry(entry);
        colorCard(entryCard);
        cards.insert(entry.key(), entryCard);
    }
    for (AnnotationCard* gone : std::as_const(m_cards)) {
        gone->disconnect(this);  // its annotation is gone, or hidden: nothing to report
        gone->hide();
        gone->deleteLater();
    }
    m_cards = cards;

    // The cards in order, the empty text and the room below them
    while (QLayoutItem* item = m_listLayout->takeAt(0)) {
        delete item;  // the widgets stay
    }
    for (const AnnotationEntry& entry : shown) {
        AnnotationCard* entryCard = m_cards.value(entry.key());
        m_listLayout->addWidget(entryCard);
        entryCard->show();
    }
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch(1);

    if (!m_cards.contains(m_selectedKey)) {
        m_selectedKey.clear();
    }
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        it.value()->setSelected(it.key() == m_selectedKey);
    }

    updateKindCounts();
    updateEmptyText(!shown.empty());
}

void AnnotationsPanel::updateKindCounts() {
    const QDateTime now = QDateTime::currentDateTime();
    std::array<int, 3> counts{};
    for (const AnnotationEntry& entry : m_entries) {
        if (matchesExceptKind(entry, m_filter, now)) {
            ++counts.at(kindIndex(entry.annotation.kind));
        }
    }
    m_kindButtons[0]->setText(tr("Comments %1").arg(counts[0]));
    m_kindButtons[1]->setText(tr("TODO %1").arg(counts[1]));
    m_kindButtons[2]->setText(tr("Notes %1").arg(counts[2]));
}

void AnnotationsPanel::updateEmptyText(bool anyShown) {
    m_emptyLabel->setVisible(!anyShown);
    if (anyShown) {
        return;
    }
    if (m_scope == AnnotationScope::Chapter && !m_documentAvailable) {
        m_emptyLabel->setText(tr("Open a chapter to see its annotations."));
    } else if (m_entries.empty()) {
        m_emptyLabel->setText(m_scope == AnnotationScope::Book
                                  ? tr("The book has no annotations.")
                                  : tr("This chapter has no annotations. Select a fragment "
                                       "or put the cursor in the text and add a comment, a "
                                       "TODO or a note from the context menu."));
    } else {
        m_emptyLabel->setText(tr("No annotation matches the filters."));
    }
}

void AnnotationsPanel::colorCard(AnnotationCard* card) const {
    card->setColors(m_kindColors.at(kindIndex(card->entry().annotation.kind)), m_cardBase,
                    m_cardTint);
}

AnnotationCard* AnnotationsPanel::createCard() {
    auto* created = new AnnotationCard(m_listWidget);

    // Copies of the entry: what a receiver does may change the card
    connect(created, &AnnotationCard::clicked, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        selectAnnotation(entry.key());
        m_listWidget->setFocus(Qt::MouseFocusReason);
        emit annotationActivated(entry);
    });
    connect(created, &AnnotationCard::editingStarted, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        selectAnnotation(entry.key());
        emit editingStarted(entry);
    });
    connect(created, &AnnotationCard::textEdited, this, [this, created](const QString& text) {
        const AnnotationEntry entry = created->entry();
        emit textEdited(entry, text);
    });
    connect(created, &AnnotationCard::editingDismissed, this,
            &AnnotationsPanel::editorFocusRequested);
    connect(created, &AnnotationCard::editingFinished, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        emit editingFinished(entry);
    });
    connect(created, &AnnotationCard::doneToggled, this, [this, created](bool done) {
        const AnnotationEntry entry = created->entry();
        emit doneToggled(entry, done);
    });
    connect(created, &AnnotationCard::deleteRequested, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        emit deleteRequested(entry);
    });
    return created;
}

void AnnotationsPanel::selectNeighbour(int step) {
    const std::vector<AnnotationEntry> shown = shownEntries();
    if (shown.empty()) {
        return;
    }
    const auto selected = std::find_if(shown.cbegin(), shown.cend(), [this](const AnnotationEntry& e) {
        return e.key() == m_selectedKey;
    });
    int index = 0;
    if (selected != shown.cend()) {
        index = std::clamp(static_cast<int>(selected - shown.cbegin()) + step, 0,
                           static_cast<int>(shown.size()) - 1);
    } else if (step < 0) {
        index = static_cast<int>(shown.size()) - 1;
    }
    const AnnotationEntry& entry = shown.at(static_cast<std::size_t>(index));
    selectAnnotation(entry.key());
    emit annotationActivated(entry);
}

void AnnotationsPanel::readFilters() {
    if (m_updatingControls) {
        return;
    }
    m_filter.text = m_searchEdit->text();
    m_filter.comments = m_kindButtons[0]->isChecked();
    m_filter.todos = m_kindButtons[1]->isChecked();
    m_filter.notes = m_kindButtons[2]->isChecked();
    m_filter.state = static_cast<AnnotationStateFilter>(std::max(0, m_stateGroup->checkedId()));
    m_filter.date = static_cast<AnnotationDateFilter>(std::max(0, m_dateCombo->currentIndex()));
    m_sort = static_cast<AnnotationSort>(std::max(0, m_sortCombo->currentIndex()));
    rebuild();
}

}  // namespace kalahari::gui
