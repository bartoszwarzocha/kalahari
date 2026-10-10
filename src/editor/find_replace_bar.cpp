/// @file find_replace_bar.cpp
/// @brief Inline find/replace bar widget implementation (OpenSpec #00044 Task 9.6)

#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/editor/search_engine.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QShortcut>
#include <QKeyEvent>

namespace kalahari::editor {

// =============================================================================
// Construction / Destruction
// =============================================================================

FindReplaceBar::FindReplaceBar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    createShortcuts();
    createConnections();

    // Start in find-only mode
    showFind();

    core::Logger::getInstance().debug("FindReplaceBar created");
}

FindReplaceBar::~FindReplaceBar()
{
    core::Logger::getInstance().debug("FindReplaceBar destroyed");
}

// =============================================================================
// UI Setup
// =============================================================================

void FindReplaceBar::setupUi()
{
    auto& art = core::ArtProvider::getInstance();

    // Main vertical layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);

    // =========================================================================
    // Row 1: Find row
    // =========================================================================
    QHBoxLayout* findRow = new QHBoxLayout();
    findRow->setSpacing(4);

    // Search input
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("Find..."));
    m_searchInput->setMinimumWidth(200);
    m_searchInput->setClearButtonEnabled(true);
    findRow->addWidget(m_searchInput, 1);

    // Option toggle buttons
    // The tooltips, with the keys: createShortcuts()
    m_caseSensitiveBtn = new QToolButton(this);
    m_caseSensitiveBtn->setText(tr("Aa"));
    m_caseSensitiveBtn->setCheckable(true);
    m_caseSensitiveBtn->setFixedSize(28, 24);
    findRow->addWidget(m_caseSensitiveBtn);

    m_wholeWordBtn = new QToolButton(this);
    m_wholeWordBtn->setText(tr("W"));
    m_wholeWordBtn->setCheckable(true);
    m_wholeWordBtn->setFixedSize(28, 24);
    findRow->addWidget(m_wholeWordBtn);

    m_regexBtn = new QToolButton(this);
    m_regexBtn->setText(tr(".*"));
    m_regexBtn->setCheckable(true);
    m_regexBtn->setFixedSize(28, 24);
    findRow->addWidget(m_regexBtn);

    // Separator
    findRow->addSpacing(8);

    // Navigation buttons
    m_prevBtn = new QToolButton(this);
    m_prevBtn->setIcon(art.getIcon("navigation.up", core::IconContext::Button));
    m_prevBtn->setToolTip(tr("Previous Match (Shift+Enter)"));
    m_prevBtn->setFixedSize(28, 24);
    findRow->addWidget(m_prevBtn);

    m_nextBtn = new QToolButton(this);
    m_nextBtn->setIcon(art.getIcon("navigation.down", core::IconContext::Button));
    m_nextBtn->setToolTip(tr("Next Match (Enter)"));
    m_nextBtn->setFixedSize(28, 24);
    findRow->addWidget(m_nextBtn);

    // Separator
    findRow->addSpacing(8);

    // Match count label
    m_matchCountLabel = new QLabel(tr("No results"), this);
    m_matchCountLabel->setMinimumWidth(60);
    findRow->addWidget(m_matchCountLabel);

    // Stretch to push close button to right
    findRow->addStretch(1);

    // Close button
    m_closeBtn = new QToolButton(this);
    m_closeBtn->setIcon(art.getIcon("dock.close", core::IconContext::Button));
    m_closeBtn->setToolTip(tr("Close (Escape)"));
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setAutoRaise(true);
    findRow->addWidget(m_closeBtn);

    mainLayout->addLayout(findRow);

    // =========================================================================
    // Row 2: Replace row
    // =========================================================================
    m_replaceSection = new QWidget(this);
    QHBoxLayout* replaceRow = new QHBoxLayout(m_replaceSection);
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->setSpacing(4);

    // Replace input
    m_replaceInput = new QLineEdit(m_replaceSection);
    m_replaceInput->setPlaceholderText(tr("Replace..."));
    m_replaceInput->setMinimumWidth(200);
    m_replaceInput->setClearButtonEnabled(true);
    replaceRow->addWidget(m_replaceInput, 1);

    // Replace buttons
    m_replaceBtn = new QPushButton(tr("Replace"), m_replaceSection);
    m_replaceBtn->setToolTip(tr("Replace Current Match"));
    m_replaceBtn->setFixedHeight(24);
    replaceRow->addWidget(m_replaceBtn);

    m_replaceAllBtn = new QPushButton(tr("Replace All"), m_replaceSection);
    m_replaceAllBtn->setToolTip(tr("Replace All Matches"));
    m_replaceAllBtn->setFixedHeight(24);
    replaceRow->addWidget(m_replaceAllBtn);

    // Add stretch to align with find row
    replaceRow->addStretch(1);

    mainLayout->addWidget(m_replaceSection);

    // Set fixed height for compact appearance
    setMaximumHeight(70);

    // Initial button states
    updateButtonStates();
}

void FindReplaceBar::createShortcuts()
{
    // Escape to close
    m_escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);

    // Alt+C, Alt+W and Alt+R turn the options on and off. On macOS Option+Cmd: there Option
    // with a letter types a character (Option+C is "ć" on the Polish keyboard)
#ifdef Q_OS_MACOS
    const Qt::KeyboardModifiers toggleModifiers = Qt::ControlModifier | Qt::AltModifier;
#else
    const Qt::KeyboardModifiers toggleModifiers = Qt::AltModifier;
#endif
    m_toggleCaseShortcut =
        new QShortcut(QKeySequence(QKeyCombination(toggleModifiers, Qt::Key_C)), this);
    m_toggleWordShortcut =
        new QShortcut(QKeySequence(QKeyCombination(toggleModifiers, Qt::Key_W)), this);
    m_toggleRegexShortcut =
        new QShortcut(QKeySequence(QKeyCombination(toggleModifiers, Qt::Key_R)), this);

    // The tooltips show the keys as the system writes them
    const auto keysOf = [](const QShortcut* shortcut) {
        return shortcut->key().toString(QKeySequence::NativeText);
    };
    m_caseSensitiveBtn->setToolTip(tr("Match Case (%1)").arg(keysOf(m_toggleCaseShortcut)));
    m_wholeWordBtn->setToolTip(tr("Match Whole Word (%1)").arg(keysOf(m_toggleWordShortcut)));
    m_regexBtn->setToolTip(
        tr("Use Regular Expression (%1)").arg(keysOf(m_toggleRegexShortcut)));
}

void FindReplaceBar::createConnections()
{
    // Search input
    connect(m_searchInput, &QLineEdit::textChanged,
            this, &FindReplaceBar::onSearchTextChanged);
    // Enter: eventFilter()
    m_searchInput->installEventFilter(this);
    m_replaceInput->installEventFilter(this);

    // Option toggles
    connect(m_caseSensitiveBtn, &QToolButton::toggled,
            this, &FindReplaceBar::onOptionsChanged);
    connect(m_wholeWordBtn, &QToolButton::toggled,
            this, &FindReplaceBar::onOptionsChanged);
    connect(m_regexBtn, &QToolButton::toggled,
            this, &FindReplaceBar::onOptionsChanged);

    // Navigation
    connect(m_prevBtn, &QToolButton::clicked,
            this, &FindReplaceBar::onFindPrevious);
    connect(m_nextBtn, &QToolButton::clicked,
            this, &FindReplaceBar::onFindNext);

    // Close
    connect(m_closeBtn, &QToolButton::clicked,
            this, &FindReplaceBar::onClose);

    // Replace
    connect(m_replaceBtn, &QPushButton::clicked,
            this, &FindReplaceBar::onReplaceCurrent);
    connect(m_replaceAllBtn, &QPushButton::clicked,
            this, &FindReplaceBar::onReplaceAll);

    // Keyboard shortcuts
    connect(m_escapeShortcut, &QShortcut::activated,
            this, &FindReplaceBar::onClose);
    connect(m_toggleCaseShortcut, &QShortcut::activated,
            m_caseSensitiveBtn, &QToolButton::toggle);
    connect(m_toggleWordShortcut, &QShortcut::activated,
            m_wholeWordBtn, &QToolButton::toggle);
    connect(m_toggleRegexShortcut, &QShortcut::activated,
            m_regexBtn, &QToolButton::toggle);
}

// =============================================================================
// Public Interface
// =============================================================================

void FindReplaceBar::setSearchEngine(editor::SearchEngine* engine)
{
    // Disconnect from old engine
    if (m_searchEngine) {
        disconnect(m_searchEngine, nullptr, this, nullptr);
    }

    m_searchEngine = engine;

    // Connect to new engine
    if (m_searchEngine) {
        connect(m_searchEngine, &editor::SearchEngine::matchesChanged,
                this, &FindReplaceBar::onMatchesChanged);
        connect(m_searchEngine, &editor::SearchEngine::currentMatchChanged,
                this, [this](const editor::SearchMatch& match) {
                    updateMatchCountLabel();
                    emit navigateToMatch(match);
                });
    }

    updateButtonStates();
}

void FindReplaceBar::showFind()
{
    m_replaceSection->setVisible(false);
    setMaximumHeight(40);
    adjustSize();
    searchAgain();
}

void FindReplaceBar::showFindReplace()
{
    m_replaceSection->setVisible(true);
    setMaximumHeight(70);
    adjustSize();
    searchAgain();
}

void FindReplaceBar::searchAgain()
{
    // Closing the bar clears the search engine, while the field keeps its text: search
    // for it again, so the matches show and Enter finds them
    if (m_searchEngine && m_searchEngine->searchText() != m_searchInput->text()) {
        onSearchTextChanged(m_searchInput->text());
    }
}

void FindReplaceBar::setSearchText(const QString& text)
{
    m_searchInput->setText(text);
}

QString FindReplaceBar::searchText() const
{
    return m_searchInput->text();
}

void FindReplaceBar::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

bool FindReplaceBar::isReplaceMode() const
{
    // isVisible() would be false while the bar itself is hidden
    return !m_replaceSection->isHidden();
}

// =============================================================================
// Event Handlers
// =============================================================================

void FindReplaceBar::keyPressEvent(QKeyEvent* event)
{
    // Keys the fields and buttons leave unused (the arrows, Page Up) would go on to the
    // editor under the bar and move or change its text
    event->accept();
}

bool FindReplaceBar::eventFilter(QObject* watched, QEvent* event)
{
    // Enter is taken before the field gets it: QLineEdit leaves it unused, so it went on
    // to the editor, which replaced the match just found with a new paragraph
    if (event->type() == QEvent::KeyPress &&
        (watched == m_searchInput || watched == m_replaceInput)) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (watched == m_replaceInput) {
                onReplaceCurrent();
            } else if (keyEvent->modifiers() & Qt::ShiftModifier) {
                onFindPrevious();
            } else {
                onFindNext();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// Private Slots
// =============================================================================

void FindReplaceBar::onSearchTextChanged(const QString& text)
{
    if (m_searchEngine) {
        m_searchEngine->setSearchText(text);
        applySearchOptions();
        m_searchEngine->findAll();  // Trigger search
    }

    emit searchTextChanged(text);
    updateButtonStates();
}

void FindReplaceBar::onFindNext()
{
    if (!m_searchEngine || m_searchInput->text().isEmpty()) {
        return;
    }

    editor::SearchMatch match = m_searchEngine->nextMatch();
    if (match.isValid()) {
        emit navigateToMatch(match);
    }

    updateMatchCountLabel();
}

void FindReplaceBar::onFindPrevious()
{
    if (!m_searchEngine || m_searchInput->text().isEmpty()) {
        return;
    }

    editor::SearchMatch match = m_searchEngine->previousMatch();
    if (match.isValid()) {
        emit navigateToMatch(match);
    }

    updateMatchCountLabel();
}

void FindReplaceBar::onReplaceCurrent()
{
    if (!m_searchEngine) {
        return;
    }

    if (m_searchEngine->currentMatchIndex() < 0) {
        // No occurrence selected yet, or the text changed since: select one first, so
        // the user sees what the next click replaces
        onFindNext();
        return;
    }

    m_searchEngine->setReplaceText(m_replaceInput->text());

    // The document's native undo records the edit
    if (m_searchEngine->replaceCurrent()) {
        // replaceCurrent() has already moved to the match after the replaced text
        // (nextMatch() would skip it). Taken before textReplaced(), which clears the
        // editor's selection and with it the current match.
        const editor::SearchMatch next = m_searchEngine->currentMatch();
        emit textReplaced(1);
        if (next.isValid()) {
            emit navigateToMatch(next);
        }
        updateMatchCountLabel();
    }
}

void FindReplaceBar::onReplaceAll()
{
    if (!m_searchEngine) {
        return;
    }

    m_searchEngine->setReplaceText(m_replaceInput->text());
    int count = m_searchEngine->replaceAll();  // one undo step of the document
    if (count > 0) {
        emit textReplaced(count);
    }

    core::Logger::getInstance().info("Replaced {} occurrences", count);
    updateMatchCountLabel();
    updateButtonStates();
}

void FindReplaceBar::onMatchesChanged()
{
    updateMatchCountLabel();
    updateButtonStates();
}

void FindReplaceBar::onOptionsChanged()
{
    applySearchOptions();

    // Re-run search with new options
    if (m_searchEngine && !m_searchInput->text().isEmpty()) {
        m_searchEngine->findAll();
    }
}

void FindReplaceBar::onClose()
{
    // Clear search highlights
    if (m_searchEngine) {
        m_searchEngine->clear();
    }

    hide();
    emit closed();
}

// =============================================================================
// Private Methods
// =============================================================================

void FindReplaceBar::updateMatchCountLabel()
{
    if (!m_searchEngine || m_searchInput->text().isEmpty()) {
        m_matchCountLabel->setText(tr("No results"));
        return;
    }

    int total = m_searchEngine->totalMatchCount();
    int current = m_searchEngine->currentMatchIndex();

    if (total == 0) {
        m_matchCountLabel->setText(tr("No results"));
    } else if (current >= 0) {
        // Display as 1-based index: "1 of 5"
        m_matchCountLabel->setText(tr("%1 of %2").arg(current + 1).arg(total));
    } else {
        // No current match selected
        m_matchCountLabel->setText(tr("%n result(s)", "", total));
    }
}

void FindReplaceBar::updateButtonStates()
{
    bool hasMatches = m_searchEngine && m_searchEngine->totalMatchCount() > 0;
    bool canReplace = hasMatches;

    // Navigation buttons
    m_prevBtn->setEnabled(hasMatches);
    m_nextBtn->setEnabled(hasMatches);

    // Replace buttons
    m_replaceBtn->setEnabled(canReplace);
    m_replaceAllBtn->setEnabled(canReplace);
}

void FindReplaceBar::applySearchOptions()
{
    if (!m_searchEngine) {
        return;
    }

    editor::SearchOptions options;
    options.caseSensitive = m_caseSensitiveBtn->isChecked();
    options.wholeWord = m_wholeWordBtn->isChecked();
    options.useRegex = m_regexBtn->isChecked();
    options.wrapAround = true;  // Always wrap

    m_searchEngine->setOptions(options);
}

}  // namespace kalahari::editor
