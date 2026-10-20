/// @file spelling_panel.cpp
/// @brief The Spelling and Grammar panel: the misspelled words and grammar issues of the text
///        in front, as cards

#include "kalahari/gui/panels/spelling_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/issue_colors.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QAbstractButton>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The kinds in the order of their buttons
constexpr std::array<editor::TextIssueKind, 2> KINDS = {editor::TextIssueKind::Spelling,
                                                         editor::TextIssueKind::Grammar};

/// @brief The kinds' buttons' names in the style sheet
constexpr std::array<const char*, 2> KIND_STYLE_NAMES = {"spelling", "grammar"};

/// @brief The suggestions the keys 1 to 5 choose
constexpr int MAX_SUGGESTION_KEYS = 5;

/// @brief How far the less important texts are from the text, toward the background
constexpr double SECONDARY_SHARE = 0.38;

/// @brief How far the controls' borders are from the text, toward the background
constexpr double BORDER_SHARE = 0.55;

/// @brief The index of a kind among the buttons of the kinds
std::size_t kindIndex(editor::TextIssueKind kind) {
    return kind == editor::TextIssueKind::Spelling ? 0 : 1;
}

/// @brief The panel's look: the buttons of the kinds as rounded chips, as in the Annotations
/// panel. The colors are written in (a style sheet keeps them through every polish): %1 the
/// borders, %2 the less important texts
const char* const PANEL_STYLE = R"(
QToolButton#spellingKindButton {
    border: 1px solid %1; border-radius: 10px; padding: 2px 8px;
    color: %2; background: transparent;
}
QLabel#spellingSecondary { color: %2; }
)";

/// @brief The button of a kind that is on: in the colors of the kind's cards
const char* const KIND_STYLE = R"(
QToolButton#spellingKindButton[kind="%1"]:checked {
    background: %2; border-color: %3; color: %4;
}
)";

/// @brief A focused button of a kind: a thicker border in the highlight's color (after the
/// rules of the kinds, whose borders it takes the place of)
const char* const FOCUS_STYLE = R"(
QToolButton#spellingKindButton:focus {
    border-width: 2px; border-color: %1; padding: 1px 7px;
}
)";

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

/// @brief The suggestion a digit key chooses (0 for 1), or -1 for another key
int suggestionOfKey(int key) {
    return key >= Qt::Key_1 && key < Qt::Key_1 + MAX_SUGGESTION_KEYS ? key - Qt::Key_1 : -1;
}

}  // namespace

SpellingPanel::SpellingPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SpellingPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // The kinds: their counts, and filters; in more lines on a narrow panel
    m_kindRow = new QWidget(this);
    auto* kinds = new FlowLayout(m_kindRow, 5, 5);
    kinds->setContentsMargins(0, 0, 0, 0);
    for (std::size_t i = 0; i < m_kindButtons.size(); ++i) {
        QToolButton*& button = m_kindButtons.at(i);
        button = new QToolButton(m_kindRow);
        button->setObjectName(QStringLiteral("spellingKindButton"));
        button->setProperty("kind", QString::fromLatin1(KIND_STYLE_NAMES.at(i)));
        button->setCheckable(true);
        button->setChecked(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(button, &QToolButton::toggled, this, [this, i](bool on) {
            if (!m_updatingButtons) {
                m_kindShown.at(i) = on;
                rebuild();
            }
        });
        kinds->addWidget(button);
    }
    m_kindButtons[0]->setToolTip(tr("Show or hide the misspelled words"));
    m_kindButtons[1]->setToolTip(tr("Show or hide the grammar issues"));
    layout->addWidget(m_kindRow);

    // How far the text is checked, or why it is not
    m_statusRow = new QWidget(this);
    auto* statusLayout = new QHBoxLayout(m_statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(6);
    m_statusIcon = new QLabel(m_statusRow);
    m_statusIcon->hide();
    statusLayout->addWidget(m_statusIcon, 0, Qt::AlignTop);
    m_statusLabel = new WrappingLabel(QString(), m_statusRow);
    m_statusLabel->setObjectName(QStringLiteral("spellingSecondary"));
    m_statusLabel->setWordWrap(true);
    statusLayout->addWidget(m_statusLabel, 1);
    m_statusRow->hide();
    layout->addWidget(m_statusRow);

    // The cards; a click on one gives the keys to the list
    m_list = new SpellingIssueList(this);
    m_list->setAccessibleName(tr("Spelling and grammar issues"));
    layout->addWidget(m_list, 1);
    connect(m_list, &SpellingIssueList::cardClicked, this, [this](const QString& key) {
        m_list->setFocus(Qt::MouseFocusReason);
        selectAndReport(m_list->indexOf(key));
    });
    connect(m_list, &SpellingIssueList::suggestionChosen, this,
            &SpellingPanel::onSuggestionChosen);
    // Copies of the issues: what a receiver does may change the list. Nothing in a text only
    // read, where these buttons are greyed out
    connect(m_list, &SpellingIssueList::ignoreWordClicked, this, [this](const QString& key) {
        if (const SpellingPanelIssue* issue = shownIssue(key); issue != nullptr && !m_readOnly) {
            const SpellingPanelIssue copy = *issue;
            emit ignoreWordRequested(copy);
        }
    });
    connect(m_list, &SpellingIssueList::addToDictionaryClicked, this, [this](const QString& key) {
        if (const SpellingPanelIssue* issue = shownIssue(key); issue != nullptr && !m_readOnly) {
            const SpellingPanelIssue copy = *issue;
            emit addToDictionaryRequested(copy);
        }
    });
    connect(m_list, &SpellingIssueList::ignoreRuleClicked, this, [this](const QString& key) {
        if (const SpellingPanelIssue* issue = shownIssue(key); issue != nullptr && !m_readOnly) {
            const SpellingPanelIssue copy = *issue;
            emit ignoreRuleRequested(copy);
        }
    });

    // What the panel says without cards, and the button that turns the checking on
    m_emptyWidget = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyWidget);
    emptyLayout->setContentsMargins(0, 12, 0, 0);
    emptyLayout->setSpacing(10);
    m_emptyLabel = new WrappingLabel(QString(), m_emptyWidget);
    m_emptyLabel->setObjectName(QStringLiteral("spellingSecondary"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyLabel);
    m_turnOnButton = new QPushButton(tr("Turn On Checking"), m_emptyWidget);
    m_turnOnButton->setToolTip(
        tr("Turn on Tools > Check Spelling as You Type and Check Grammar as You Type"));
    connect(m_turnOnButton, &QPushButton::clicked, this, &SpellingPanel::turnOnCheckingRequested);
    emptyLayout->addWidget(m_turnOnButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);
    layout->addWidget(m_emptyWidget, 1);

    // Previous and next; one under the other on a narrow panel
    m_footRow = new QWidget(this);
    auto* foot = new FlowLayout(m_footRow, 6, 6);
    foot->setContentsMargins(0, 0, 0, 0);
    m_previousButton = new QToolButton(m_footRow);
    m_previousButton->setText(tr("Previous"));
    m_previousButton->setToolTip(tr("Select the previous issue (Up)"));
    m_nextButton = new QToolButton(m_footRow);
    m_nextButton->setText(tr("Next"));
    m_nextButton->setToolTip(tr("Select the next issue (Down)"));
    for (QToolButton* button : {m_previousButton, m_nextButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        foot->addWidget(button);
    }
    m_nextButton->setLayoutDirection(Qt::RightToLeft);  // its arrow after the text
    connect(m_previousButton, &QToolButton::clicked, this, [this]() { step(-1); });
    connect(m_nextButton, &QToolButton::clicked, this, [this]() { step(1); });
    layout->addWidget(m_footRow);

    // The most important keys
    m_hintLabel = new WrappingLabel(QString(), this);
    m_hintLabel->setObjectName(QStringLiteral("spellingSecondary"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    connect(qApp, &QApplication::focusChanged, this, &SpellingPanel::updateKeysInside);

    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, &SpellingPanel::applyTheme);
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            [this]() { applyTheme(core::ThemeManager::getInstance().getCurrentTheme()); });
    applyTheme(themes.getCurrentTheme());
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &SpellingPanel::updateIcons);
    updateIcons();

    rebuild();
}

SpellingPanel::~SpellingPanel() {
    // The focus leaves the cards while QWidget's destructor deletes them, after this class is
    // gone: nothing of the program reaches a panel being destroyed
    disconnect(qApp, &QApplication::focusChanged, this, &SpellingPanel::updateKeysInside);
}

void SpellingPanel::setContent(Content content) {
    if (content != m_content) {
        m_content = content;
        rebuild();
    }
}

void SpellingPanel::setKindNames(const std::optional<core::KindWords>& inFront,
                                 const std::optional<core::KindWords>& bookTexts, bool bookOpen) {
    m_inFront = inFront;
    m_bookTexts = bookTexts;
    m_bookOpen = bookOpen;
    m_emptyLabel->setText(emptyText());
}

void SpellingPanel::setIssues(std::vector<SpellingPanelIssue> issues) {
    m_issues = std::move(issues);
    rebuild();
}

const std::vector<SpellingPanelIssue>& SpellingPanel::shownIssues() const {
    return m_list->issues();
}

void SpellingPanel::setKindChecked(editor::TextIssueKind kind, bool checked) {
    bool& current = m_kindChecked.at(kindIndex(kind));
    if (checked != current) {
        current = checked;
        rebuild();
    }
}

bool SpellingPanel::isKindChecked(editor::TextIssueKind kind) const {
    return m_kindChecked.at(kindIndex(kind));
}

void SpellingPanel::setKindShown(editor::TextIssueKind kind, bool shown) {
    bool& current = m_kindShown.at(kindIndex(kind));
    if (shown != current) {
        current = shown;
        rebuild();
    }
}

bool SpellingPanel::isKindShown(editor::TextIssueKind kind) const {
    return m_kindShown.at(kindIndex(kind));
}

void SpellingPanel::setStatus(const QString& text, bool problem) {
    m_statusLabel->setText(text);
    m_statusIcon->setVisible(problem);
    updateParts();
}

QString SpellingPanel::statusText() const {
    return m_statusLabel->text();
}

void SpellingPanel::setComplete(bool complete) {
    if (complete != m_complete) {
        m_complete = complete;
        m_emptyLabel->setText(emptyText());
    }
}

void SpellingPanel::setReadOnly(bool readOnly) {
    if (readOnly != m_readOnly) {
        m_readOnly = readOnly;
        m_list->setReadOnly(readOnly);
        updateHint();
    }
}

void SpellingPanel::setButtonsOnEveryCard(bool every) {
    m_list->setExpandedAll(every);
}

void SpellingPanel::setSuggestionProvider(SpellingIssueList::SuggestionProvider provider) {
    m_list->setSuggestionProvider(std::move(provider));
}

void SpellingPanel::forgetSuggestions() {
    m_list->forgetSuggestions();
}

void SpellingPanel::setNextIssueKeys(const QString& keys, bool toPanel) {
    m_nextKeys = keys;
    m_nextToPanel = toPanel;
    updateHint();
}

void SpellingPanel::selectIssue(const QString& key) {
    m_list->setSelectedKey(m_list->indexOf(key) >= 0 ? key : QString());
    updateFootButtons();
    updateHint();
}

QString SpellingPanel::selectedKey() const {
    return m_list->selectedKey();
}

std::optional<SpellingPanelIssue> SpellingPanel::selectedIssue() const {
    const int index = m_list->selectedIndex();
    if (index < 0) {
        return std::nullopt;
    }
    return m_list->issues().at(static_cast<std::size_t>(index));
}

std::optional<SpellingPanelIssue> SpellingPanel::issueFrom(int position) const {
    const std::vector<SpellingPanelIssue>& shown = m_list->issues();
    const auto found = std::find_if(shown.cbegin(), shown.cend(),
                                    [position](const SpellingPanelIssue& issue) {
                                        return issue.position >= position;
                                    });
    if (found == shown.cend()) {
        return std::nullopt;
    }
    return *found;
}

void SpellingPanel::focusList() {
    if (!m_list->isHidden()) {
        m_list->setFocus(Qt::OtherFocusReason);
    } else if (!m_emptyWidget->isHidden() && !m_turnOnButton->isHidden()) {
        m_turnOnButton->setFocus(Qt::OtherFocusReason);
    } else {
        setFocus(Qt::OtherFocusReason);  // Esc still goes back to the text
    }
}

bool SpellingPanel::hasFocusInside() const {
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == this || isAncestorOf(focus));
}

QToolButton* SpellingPanel::kindButton(editor::TextIssueKind kind) const {
    return m_kindButtons.at(kindIndex(kind));
}

void SpellingPanel::applyTheme(const core::Theme& theme) {
    const QPalette current = palette();
    const QColor window = themeColor(theme.palette.window, current, QPalette::Window);
    const QColor base = themeColor(theme.palette.base, current, QPalette::Base);
    const QColor text = themeColor(theme.palette.text, current, QPalette::Text);
    const QColor highlight = themeColor(theme.palette.highlight, current, QPalette::Highlight);

    // The cards' frames in the colors of the waves, as "Optimize colors in themes" shows them
    // on the panel
    std::array<IssueCardColors, 2> colors;
    for (std::size_t i = 0; i < KINDS.size(); ++i) {
        const QColor kind = issueColorOn(KINDS.at(i), base);
        colors.at(i) = issueCardColors(kind, base, text);
        m_kindButtons.at(i)->setIcon(kindDot(kind, devicePixelRatioF()));
    }
    m_list->setColors(colors[0], colors[1], highlight);

    // The controls lie on the window
    const QColor windowText = themeColor(theme.palette.windowText, current, QPalette::WindowText);
    const QColor secondary = readableColor(mixedColor(windowText, window, SECONDARY_SHARE), window,
                                           MIN_TEXT_CONTRAST);
    QString style = QString::fromLatin1(PANEL_STYLE)
                        .arg(mixedColor(windowText, window, BORDER_SHARE).name(), secondary.name());
    for (std::size_t i = 0; i < KINDS.size(); ++i) {
        const IssueCardColors& kindColors = colors.at(i);
        style += QString::fromLatin1(KIND_STYLE).arg(QLatin1String(KIND_STYLE_NAMES.at(i)),
                                                     kindColors.card.background.name(),
                                                     kindColors.border.name(),
                                                     kindColors.card.text.name());
    }
    style += QString::fromLatin1(FOCUS_STYLE).arg(highlight.name());
    setStyleSheet(style);
}

bool SpellingPanel::event(QEvent* event) {
    // The keys of the panel are its own while it has them, not the window's shortcuts
    if (event->type() == QEvent::ShortcutOverride && isPanelKey(static_cast<QKeyEvent*>(event))) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void SpellingPanel::keyPressEvent(QKeyEvent* event) {
    if (!isPanelKey(event)) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int count = m_list->count();
    const int selected = m_list->selectedIndex();
    switch (event->key()) {
    case Qt::Key_Escape:
        emit editorFocusRequested();
        break;
    case Qt::Key_Up:
        step(-1);
        break;
    case Qt::Key_Down:
        step(1);
        break;
    case Qt::Key_Home:
        if (count > 0 && selected != 0) {
            selectAndReport(0);
        }
        break;
    case Qt::Key_End:
        if (count > 0 && selected != count - 1) {
            selectAndReport(count - 1);
        }
        break;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown: {
        const int target = m_list->pageStepFrom(selected, event->key() == Qt::Key_PageUp ? -1 : 1);
        if (target >= 0 && target != selected) {
            selectAndReport(target);
        }
        break;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        // The button with the keys (Tab), else the first suggestion
        auto* button = qobject_cast<QAbstractButton*>(QApplication::focusWidget());
        if (button != nullptr && isAncestorOf(button)) {
            button->click();
        } else {
            chooseSuggestion(0);
        }
        break;
    }
    case Qt::Key_F10:  // with Shift: the menu
        requestMenuOfSelected();
        break;
    default:
        chooseSuggestion(suggestionOfKey(event->key()));
        break;
    }
    event->accept();
}

void SpellingPanel::contextMenuEvent(QContextMenuEvent* event) {
    if (m_content != Content::Issues) {
        QWidget::contextMenuEvent(event);
        return;
    }
    // From the keyboard: the selected issue's
    if (event->reason() == QContextMenuEvent::Keyboard) {
        requestMenuOfSelected();
        return;
    }
    // A right click on a card selects it and gives the keys to the list, as a click does
    const QPoint inList = m_list->viewport()->mapFromGlobal(event->globalPos());
    const int index = m_list->viewport()->rect().contains(inList)
                          ? m_list->indexOf(m_list->keyAt(inList))
                          : -1;
    if (index < 0) {
        QWidget::contextMenuEvent(event);
        return;
    }
    m_list->setFocus(Qt::PopupFocusReason);
    if (index != m_list->selectedIndex()) {
        selectAndReport(index);
    }
    const SpellingPanelIssue issue = m_list->issues().at(static_cast<std::size_t>(index));
    emit menuRequested(issue, event->globalPos());
}

void SpellingPanel::rebuild() {
    // The place of the selected issue, for the one after it when it is gone
    const QString selected = m_list->selectedKey();
    const SpellingPanelIssue* old = shownIssue(selected);
    const int oldPosition = old != nullptr ? old->position : 0;

    std::vector<SpellingPanelIssue> shown;
    for (const SpellingPanelIssue& issue : m_issues) {
        const std::size_t kind = kindIndex(issue.issue.kind);
        if (m_content == Content::Issues && m_kindChecked.at(kind) && m_kindShown.at(kind)) {
            shown.push_back(issue);
        }
    }
    const bool stays = std::any_of(shown.cbegin(), shown.cend(),
                                   [&selected](const SpellingPanelIssue& issue) {
                                       return issue.key == selected;
                                   });
    m_list->setIssues(std::move(shown));
    if (!selected.isEmpty() && !stays) {
        const std::optional<SpellingPanelIssue> next = issueFrom(oldPosition);
        m_list->setSelectedKey(next ? next->key : QString());
    }

    updateKindButtons();
    updateParts();
    updateFootButtons();
    updateHint();
}

void SpellingPanel::updateKindButtons() {
    std::array<int, 2> counts{};
    for (const SpellingPanelIssue& issue : m_issues) {
        ++counts.at(kindIndex(issue.issue.kind));
    }
    //: %1 is the number of the misspelled words, or – when the spelling is not checked
    const QString spelling = tr("Spelling %1");
    //: %1 is the number of the grammar issues, or – when the grammar is not checked
    const QString grammar = tr("Grammar %1");
    const std::array<QString, 2> names = {spelling, grammar};

    const QScopedValueRollback<bool> updating(m_updatingButtons, true);
    for (std::size_t i = 0; i < m_kindButtons.size(); ++i) {
        QToolButton* button = m_kindButtons.at(i);
        const bool checked = m_kindChecked.at(i);
        button->setEnabled(checked);
        button->setChecked(checked && m_kindShown.at(i));
        button->setText(names.at(i).arg(checked ? QString::number(counts.at(i))
                                                : QStringLiteral("–")));
    }
}

void SpellingPanel::updateParts() {
    const bool issues = m_content == Content::Issues;
    const bool listed = issues && m_list->count() > 0;
    // The keys stay in the panel when the list they were in goes
    const bool keysInList = m_list->hasFocus();

    m_kindRow->setVisible(issues);
    m_statusRow->setVisible(issues && !m_statusLabel->text().isEmpty());
    m_list->setVisible(listed);
    m_emptyWidget->setVisible(!listed);
    m_emptyLabel->setText(emptyText());
    m_turnOnButton->setVisible(m_content == Content::NotChecked);
    m_footRow->setVisible(listed);

    if (keysInList && !listed) {
        focusList();
    }
}

QString SpellingPanel::emptyText() const {
    switch (m_content) {
    case Content::NoText:
        // What the writer opens: a text of the book ("Open a story..."), or a book or a file
        if (m_bookTexts) {
            //: In Polish: Otwórz {kind:accusative} albo plik, aby zobaczyć jego błędy pisowni i
            //: gramatyki.
            return core::fillWords(
                tr("Open {kind:indefinite} or a file to see its spelling and grammar issues."),
                {{QStringLiteral("kind"), *m_bookTexts}});
        }
        if (m_bookOpen) {
            return tr("Open an element of the book or a file to see its spelling and grammar "
                      "issues.");
        }
        return tr("Open a book or a file to see the spelling and grammar issues.");
    case Content::NotChecked:
        return tr("Spelling and grammar are not checked.");
    case Content::Issues:
        break;
    }

    const bool spelling = m_kindChecked[0];
    const bool grammar = m_kindChecked[1];
    const bool anyChecked = std::any_of(m_issues.cbegin(), m_issues.cend(),
                                        [this](const SpellingPanelIssue& issue) {
                                            return m_kindChecked.at(kindIndex(issue.issue.kind));
                                        });
    if (anyChecked) {
        return tr("No issue matches the filters.");
    }
    if (!spelling && !grammar) {
        return tr("Spelling and grammar are not checked.");
    }
    if (!m_complete) {
        return tr("No issues found yet.");
    }
    if (!m_inFront) {
        return tr("No spelling or grammar issues.");
    }
    QString text;
    if (spelling && grammar) {
        //: In Polish: Brak błędów pisowni i gramatyki w {kind:m=tym|f=tej|n=tym|p=tych}
        //: {kind:locative}.
        text = tr("No spelling or grammar issues in {kind:s=this|p=these} {kind}.");
    } else if (spelling) {
        //: In Polish: Brak błędów pisowni w {kind:m=tym|f=tej|n=tym|p=tych} {kind:locative}.
        text = tr("No misspelled words in {kind:s=this|p=these} {kind}.");
    } else {
        //: In Polish: Brak błędów gramatyki w {kind:m=tym|f=tej|n=tym|p=tych} {kind:locative}.
        text = tr("No grammar issues in {kind:s=this|p=these} {kind}.");
    }
    return core::fillWords(text, {{QStringLiteral("kind"), *m_inFront}});
}

void SpellingPanel::updateFootButtons() {
    const int count = m_list->count();
    const int selected = m_list->selectedIndex();
    m_previousButton->setEnabled(count > 0 && selected != 0);
    m_nextButton->setEnabled(count > 0 && selected != count - 1);
}

void SpellingPanel::updateHint() {
    QString hint;
    if (m_keysInside) {
        // What the keys do here: fix the selected issue, or go back to the text
        hint = m_content == Content::Issues && !m_readOnly && m_list->selectedIndex() >= 0
                   ? tr("Enter, 1–5 – fix · Esc – back to the text")
                   : tr("Esc – back to the text");
    } else if (m_content == Content::Issues && !m_nextKeys.isEmpty()) {
        //: %1 is the key of Tools > Next Spelling or Grammar Issue, for example F7
        hint = m_nextToPanel ? tr("%1 – next issue (the keys go to the panel)").arg(m_nextKeys)
                             //: %1 is the key of Tools > Next Spelling or Grammar Issue
                             : tr("%1 – next issue, with its menu at the word").arg(m_nextKeys);
    }
    m_hintLabel->setText(hint);
    m_hintLabel->setVisible(!hint.isEmpty());
}

void SpellingPanel::updateKeysInside() {
    // A menu only lends the keys for a moment
    if (QApplication::activePopupWidget() != nullptr) {
        return;
    }
    const bool inside = hasFocusInside();
    if (inside != m_keysInside) {
        m_keysInside = inside;
        m_list->setKeysInside(inside);
        updateHint();
    }
}

void SpellingPanel::updateIcons() {
    auto& art = core::ArtProvider::getInstance();
    m_previousButton->setIcon(art.getIcon(QStringLiteral("common.arrowUpward")));
    m_nextButton->setIcon(art.getIcon(QStringLiteral("common.arrowDownward")));
    const int size = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_statusIcon->setPixmap(
        art.getIcon(QStringLiteral("common.warning")).pixmap(QSize(size, size), devicePixelRatioF()));
}

void SpellingPanel::selectAndReport(int index) {
    if (index < 0 || index >= m_list->count()) {
        return;
    }
    const SpellingPanelIssue issue = m_list->issues().at(static_cast<std::size_t>(index));
    m_list->setSelectedKey(issue.key);
    updateFootButtons();
    updateHint();
    emit issueSelected(issue);
}

void SpellingPanel::step(int steps) {
    const int count = m_list->count();
    if (count == 0) {
        return;
    }
    const int selected = m_list->selectedIndex();
    const int index = selected < 0 ? (steps > 0 ? 0 : count - 1)
                                   : std::clamp(selected + steps, 0, count - 1);
    if (index != selected) {
        selectAndReport(index);
    }
}

void SpellingPanel::chooseSuggestion(int index) {
    // Also in a read-only text, which keeps its words and says why (BookEditor::replaceIssue())
    const std::optional<SpellingPanelIssue> issue = selectedIssue();
    if (!issue || index < 0) {
        return;
    }
    const QStringList suggestions = m_list->suggestionsFor(*issue);
    if (index < std::min<int>(static_cast<int>(suggestions.size()), MAX_SUGGESTION_KEYS)) {
        emit suggestionChosen(*issue, suggestions.at(index));
    }
}

void SpellingPanel::requestMenuOfSelected() {
    const std::optional<SpellingPanelIssue> issue = selectedIssue();
    if (!issue) {
        return;
    }
    // Under the selected card's words, so the issue stays in sight
    const SpellingIssueCard* card = m_list->selectedCard();
    const QPoint where = card != nullptr ? card->mapToGlobal(card->rect().bottomLeft())
                                         : m_list->viewport()->mapToGlobal(QPoint(0, 0));
    emit menuRequested(*issue, where);
}

void SpellingPanel::onSuggestionChosen(const QString& key, int index) {
    const SpellingPanelIssue* issue = shownIssue(key);
    if (issue == nullptr || m_readOnly) {
        return;
    }
    const SpellingPanelIssue copy = *issue;
    const QStringList suggestions = m_list->suggestionsFor(copy);
    if (index >= 0 && index < suggestions.size()) {
        emit suggestionChosen(copy, suggestions.at(index));
    }
}

const SpellingPanelIssue* SpellingPanel::shownIssue(const QString& key) const {
    const int index = key.isEmpty() ? -1 : m_list->indexOf(key);
    return index >= 0 ? &m_list->issues().at(static_cast<std::size_t>(index)) : nullptr;
}

bool SpellingPanel::isPanelKey(const QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::ShiftModifier) {
        return event->key() == Qt::Key_F10;
    }
    if (modifiers != Qt::NoModifier) {
        return false;
    }
    switch (event->key()) {
    case Qt::Key_Escape:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return true;
    default:
        return suggestionOfKey(event->key()) >= 0;
    }
}

}  // namespace kalahari::gui
