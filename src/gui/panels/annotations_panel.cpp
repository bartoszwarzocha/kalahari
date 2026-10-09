/// @file annotations_panel.cpp
/// @brief The Annotations panel: the comments, to-dos and notes of a chapter or the book

#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
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
#include <functional>
#include <iterator>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The kinds in the order of the kind buttons
constexpr std::array<editor::AnnotationKind, 3> KINDS = {
    editor::AnnotationKind::Comment, editor::AnnotationKind::Todo, editor::AnnotationKind::Note};

/// @brief The kind buttons' names in the style sheet
constexpr std::array<const char*, 3> KIND_STYLE_NAMES = {"comment", "todo", "note"};

/// @brief How far the less important texts are from the text, toward the background
constexpr double SECONDARY_SHARE = 0.38;

/// @brief How far the controls' borders are from the text, toward the background
constexpr double BORDER_SHARE = 0.55;

/// @brief How far the text of a control that is off is from the text, toward the background
constexpr double DISABLED_SHARE = 0.55;

/// @brief The index of a kind among the kind buttons
std::size_t kindIndex(editor::AnnotationKind kind) {
    return static_cast<std::size_t>(std::find(KINDS.begin(), KINDS.end(), kind) - KINDS.begin());
}

/// @brief The panel's look: kind buttons as rounded chips, joined buttons of a choice. The
/// colors are written in (a style sheet keeps them through every polish): %1 the borders,
/// %2 the less important texts, %3 the text, %4 the base, %5 the highlight, %6 the text on
/// it, %7 the text of a control that is off. A focused control has a thicker border.
const char* const PANEL_STYLE = R"(
QToolButton#annotationKindButton {
    border: 1px solid %1; border-radius: 10px; padding: 2px 8px;
    color: %2; background: transparent;
}
QToolButton[segment] {
    border: 1px solid %1; padding: 2px 8px; color: %3; background: %4;
}
QToolButton[segment="first"] { border-top-left-radius: 6px; border-bottom-left-radius: 6px; }
QToolButton[segment="middle"] { border-left: none; }
QToolButton[segment="last"] {
    border-left: none; border-top-right-radius: 6px; border-bottom-right-radius: 6px;
}
QToolButton[segment]:checked { background: %5; color: %6; }
QToolButton[segment]:disabled { color: %7; }
QLabel#annotationSecondary { color: %2; }
)";

/// @brief A kind button that is on: in the colors of the kind's cards
const char* const KIND_STYLE = R"(
QToolButton#annotationKindButton[kind="%1"]:checked {
    background: %2; border-color: %3; color: %4;
}
)";

/// @brief A focused control: a thicker border in the highlight's color (after the rules of
/// the kinds, whose borders it takes the place of)
const char* const FOCUS_STYLE = R"(
QToolButton#annotationKindButton:focus, QToolButton[segment]:focus {
    border-width: 2px; border-color: %1; padding: 1px 7px;
}
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

/// @brief A theme's color, or the palette's when the theme has none
QColor themeColor(const QColor& color, const QPalette& palette, QPalette::ColorRole role) {
    return color.isValid() ? color : palette.color(role);
}

/// @brief The keys of the list: they are its own, not the window's shortcuts (F2 opens the
/// Navigator)
bool isListKey(const QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::ShiftModifier) {
        return event->key() == Qt::Key_F10;
    }
    if (modifiers != Qt::NoModifier) {
        return false;
    }
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
    case Qt::Key_Space:
    case Qt::Key_Delete:
    case Qt::Key_Escape:
        return true;
    default:
        return false;
    }
}

}  // namespace

AnnotationsPanel::AnnotationsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AnnotationsPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Search in the text of the annotations; Down goes on to the list
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search the annotations..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AnnotationsPanel::readFilters);
    layout->addWidget(m_searchEdit);

    // Kinds: on or off, with their counts
    auto* kinds = new QHBoxLayout();
    kinds->setSpacing(5);
    for (std::size_t i = 0; i < m_kindButtons.size(); ++i) {
        QToolButton*& button = m_kindButtons.at(i);
        button = new QToolButton(this);
        button->setObjectName(QStringLiteral("annotationKindButton"));
        button->setProperty("kind", QString::fromLatin1(KIND_STYLE_NAMES.at(i)));
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

    // Order (in the order of AnnotationSort)
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({tr("Sort: text order"), tr("Sort: newest first"), tr("Sort: by kind")});
    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, &AnnotationsPanel::readFilters);
    layout->addWidget(m_sortCombo);

    // The cards; a click on one gives the keys to the list
    m_listWidget = new QWidget();
    m_listWidget->setFocusPolicy(Qt::StrongFocus);
    m_listWidget->setAccessibleName(tr("Annotations"));
    m_listWidget->installEventFilter(this);
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(6);
    m_emptyLabel = new QLabel(m_listWidget);
    m_emptyLabel->setObjectName(QStringLiteral("annotationSecondary"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch(1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_listWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_scrollArea, 1);

    // To do: previous and next
    auto* todoRow = new QHBoxLayout();
    todoRow->setSpacing(6);
    m_todoLabel = new QLabel(tr("To do:"), this);
    m_todoLabel->setObjectName(QStringLiteral("annotationSecondary"));
    todoRow->addWidget(m_todoLabel);
    m_previousTodoButton = new QToolButton(this);
    m_previousTodoButton->setText(tr("Previous"));
    m_nextTodoButton = new QToolButton(this);
    m_nextTodoButton->setText(tr("Next"));
    for (QToolButton* button : {m_previousTodoButton, m_nextTodoButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        todoRow->addWidget(button);
    }
    m_nextTodoButton->setLayoutDirection(Qt::RightToLeft);  // its arrow after the text
    m_todoLabel->hide();
    m_previousTodoButton->hide();
    m_nextTodoButton->hide();
    layout->addLayout(todoRow);

    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &AnnotationsPanel::applyTheme);
    applyTheme(themes.getCurrentTheme());
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
    // The buttons have their own short texts; the actions give them the rest
    const auto follow = [](QToolButton* button, QAction* action) {
        button->setVisible(action != nullptr);
        if (action == nullptr) {
            return;
        }
        const auto update = [button, action]() {
            button->setIcon(action->icon());
            button->setToolTip(action->toolTip());
            button->setEnabled(action->isEnabled());
        };
        update();
        QObject::connect(action, &QAction::changed, button, update);
        QObject::connect(button, &QToolButton::clicked, action, &QAction::trigger);
    };
    follow(m_previousTodoButton, previous);
    follow(m_nextTodoButton, next);
    m_todoLabel->setVisible(previous != nullptr || next != nullptr);
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

void AnnotationsPanel::revealAnnotation(const QString& key) {
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
}

AnnotationCard* AnnotationsPanel::card(const QString& key) const {
    return m_cards.value(key);
}

void AnnotationsPanel::focusList(const QString& preferredKey) {
    if (m_cards.contains(preferredKey)) {
        selectAnnotation(preferredKey);
    } else if (m_selectedKey.isEmpty()) {
        const std::vector<AnnotationEntry> shown = shownEntries();
        if (!shown.empty()) {
            selectAnnotation(shown.front().key());
        }
    } else {
        selectAnnotation(m_selectedKey);  // scrolled into view
    }
    m_listWidget->setFocus(Qt::OtherFocusReason);
}

bool AnnotationsPanel::hasFocusInside() const {
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == this || isAncestorOf(focus));
}

void AnnotationsPanel::applyTheme(const core::Theme& theme) {
    const QPalette current = palette();
    const QColor window = themeColor(theme.palette.window, current, QPalette::Window);
    m_base = themeColor(theme.palette.base, current, QPalette::Base);
    m_text = themeColor(theme.palette.text, current, QPalette::Text);
    m_highlight = themeColor(theme.palette.highlight, current, QPalette::Highlight);
    m_highlightedText = themeColor(theme.palette.highlightedText, current, QPalette::HighlightedText);

    // A dark window gets the colors made for dark paper
    const bool dark = window.lightness() < 128;
    for (std::size_t i = 0; i < KINDS.size(); ++i) {
        const auto color = theme.editor.find(editor::annotationColorKey(KINDS.at(i), dark));
        const QColor kind = color != theme.editor.end() && color->second.isValid() ? color->second
                                                                                   : m_highlight;
        m_cardColors.at(i) = annotationCardColors(kind, m_base, m_text);
        m_kindButtons.at(i)->setIcon(kindDot(kind, devicePixelRatioF()));
    }

    // The controls lie on the window
    const QColor windowText = themeColor(theme.palette.windowText, current, QPalette::WindowText);
    const QColor secondary = readableColor(mixedColor(windowText, window, SECONDARY_SHARE), window,
                                           MIN_TEXT_CONTRAST);
    QString style = QString::fromLatin1(PANEL_STYLE)
                        .arg(mixedColor(windowText, window, BORDER_SHARE).name(), secondary.name(),
                             m_text.name(), m_base.name(), m_highlight.name(),
                             m_highlightedText.name(),
                             mixedColor(m_text, m_base, DISABLED_SHARE).name());
    for (std::size_t i = 0; i < KINDS.size(); ++i) {
        const AnnotationCardColors& colors = m_cardColors.at(i);
        style += QString::fromLatin1(KIND_STYLE).arg(QLatin1String(KIND_STYLE_NAMES.at(i)),
                                                     colors.background.name(),
                                                     colors.kindName.name(), colors.text.name());
    }
    style += QString::fromLatin1(FOCUS_STYLE).arg(m_highlight.name());
    setStyleSheet(style);

    for (AnnotationCard* shownCard : std::as_const(m_cards)) {
        colorCard(shownCard);
    }
}

const AnnotationCardColors& AnnotationsPanel::cardColors(editor::AnnotationKind kind) const {
    return m_cardColors.at(kindIndex(kind));
}

void AnnotationsPanel::keyPressEvent(QKeyEvent* event) {
    // Esc goes back to the text from anywhere in the panel
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        emit editorFocusRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool AnnotationsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_searchEdit) {
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Down) {
            focusList();
            return true;
        }
        return false;
    }
    if (watched != m_listWidget) {
        return false;
    }

    switch (event->type()) {
    case QEvent::ShortcutOverride: {
        auto* key = static_cast<QKeyEvent*>(event);
        if (isListKey(key)) {
            key->accept();
            return true;
        }
        return false;
    }
    case QEvent::KeyPress:
        return listKeyPressed(static_cast<QKeyEvent*>(event));
    case QEvent::ContextMenu:
        return showCardMenu(static_cast<QContextMenuEvent*>(event));
    case QEvent::FocusIn:
        showListFocus(true);
        return false;
    case QEvent::FocusOut:
        // A card's menu only lends the keys for a moment
        if (static_cast<QFocusEvent*>(event)->reason() != Qt::PopupFocusReason) {
            showListFocus(false);
        }
        return false;
    default:
        return false;
    }
}

void AnnotationsPanel::showListFocus(bool focused) {
    for (AnnotationCard* card : std::as_const(m_cards)) {
        card->setListFocused(focused);
    }
}

bool AnnotationsPanel::listKeyPressed(const QKeyEvent* event) {
    if (!isListKey(event)) {
        return false;
    }
    AnnotationCard* selected = m_cards.value(m_selectedKey);
    switch (event->key()) {
    case Qt::Key_Up:
        selectNeighbour(-1);
        break;
    case Qt::Key_Down:
        selectNeighbour(1);
        break;
    case Qt::Key_Home:
        selectEdge(false);
        break;
    case Qt::Key_End:
        selectEdge(true);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (selected != nullptr) {
            const AnnotationEntry entry = selected->entry();
            emit editRequested(entry);
        }
        break;
    case Qt::Key_Space:
        // A to-do done, a comment resolved, or either brought back; a note has no state
        if (selected != nullptr && selected->entry().annotation.kind != editor::AnnotationKind::Note) {
            actOnSelected([this](const AnnotationEntry& entry) {
                emit doneToggled(entry, !entry.annotation.done);
            });
        }
        break;
    case Qt::Key_Delete:
        actOnSelected([this](const AnnotationEntry& entry) { emit deleteRequested(entry); });
        break;
    case Qt::Key_F10:  // with Shift: the menu
        if (selected != nullptr) {
            selected->showMenu();
        }
        break;
    case Qt::Key_Escape:
        emit editorFocusRequested();
        break;
    default:
        break;
    }
    return true;
}

bool AnnotationsPanel::showCardMenu(const QContextMenuEvent* event) {
    AnnotationCard* target = nullptr;
    if (event->reason() == QContextMenuEvent::Keyboard) {
        target = m_cards.value(m_selectedKey);
    } else {
        for (QWidget* child = m_listWidget->childAt(event->pos());
             child != nullptr && child != m_listWidget; child = child->parentWidget()) {
            target = qobject_cast<AnnotationCard*>(child);
            if (target != nullptr) {
                break;
            }
        }
    }
    if (target == nullptr) {
        return false;
    }
    selectAnnotation(target->entry().key());
    m_listWidget->setFocus(Qt::PopupFocusReason);
    target->showMenu();
    return true;
}

void AnnotationsPanel::rebuild() {
    const QDateTime now = QDateTime::currentDateTime();

    // What passes the filters
    std::vector<AnnotationEntry> shown;
    for (const AnnotationEntry& entry : m_entries) {
        if (matches(entry, m_filter, now)) {
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
    m_kindButtons[1]->setText(tr("To do %1").arg(counts[1]));
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
        // The keys of Add Annotation
        const QString addKeys = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M)
                                    .toString(QKeySequence::NativeText);
        m_emptyLabel->setText(m_scope == AnnotationScope::Book
                                  ? tr("The book has no annotations.")
                                  : tr("This chapter has no annotations. Select a fragment "
                                       "or put the cursor in the text and add a comment, a "
                                       "to-do or a note with %1 or from the context menu.")
                                        .arg(addKeys));
    } else {
        m_emptyLabel->setText(tr("No annotation matches the filters."));
    }
}

void AnnotationsPanel::colorCard(AnnotationCard* card) const {
    card->setColors(m_cardColors.at(kindIndex(card->entry().annotation.kind)));
}

AnnotationCard* AnnotationsPanel::createCard() {
    auto* created = new AnnotationCard(m_listWidget);
    created->setListFocused(m_listWidget->hasFocus());

    // Copies of the entry: what a receiver does may change the card
    connect(created, &AnnotationCard::clicked, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        selectAnnotation(entry.key());
        m_listWidget->setFocus(Qt::MouseFocusReason);
        emit annotationActivated(entry);
    });
    connect(created, &AnnotationCard::editRequested, this, [this, created]() {
        const AnnotationEntry entry = created->entry();
        selectAnnotation(entry.key());
        emit editRequested(entry);
    });
    connect(created, &AnnotationCard::doneToggled, this, [this, created](bool done) {
        selectAnnotation(created->entry().key());
        actOnSelected([this, done](const AnnotationEntry& entry) { emit doneToggled(entry, done); });
    });
    connect(created, &AnnotationCard::deleteRequested, this, [this, created]() {
        selectAnnotation(created->entry().key());
        actOnSelected([this](const AnnotationEntry& entry) { emit deleteRequested(entry); });
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
    if (entry.key() == m_selectedKey) {
        return;  // the first or the last one already
    }
    selectAnnotation(entry.key());
    emit annotationActivated(entry);
}

void AnnotationsPanel::selectEdge(bool last) {
    const std::vector<AnnotationEntry> shown = shownEntries();
    if (shown.empty()) {
        return;
    }
    const AnnotationEntry& entry = last ? shown.back() : shown.front();
    if (entry.key() != m_selectedKey) {
        selectAnnotation(entry.key());
        emit annotationActivated(entry);
    }
}

void AnnotationsPanel::actOnSelected(const std::function<void(const AnnotationEntry&)>& action) {
    const AnnotationCard* selected = m_cards.value(m_selectedKey);
    if (selected == nullptr) {
        return;
    }

    // The card after it (or before it, for the last one) is selected when it goes
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
    action(entry);
    if (!m_cards.contains(entry.key()) && m_cards.contains(neighbour)) {
        selectAnnotation(neighbour);
    }
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
