/// @file quick_character_palette.cpp
/// @brief Insert > Quick Character: the favorite characters and those used last at the cursor
///        of the text, and a search of all of them

#include "kalahari/gui/widgets/quick_character_palette.h"
#include "kalahari/gui/special_character_choices.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

constexpr int MARGIN = 8;         ///< Around the content of the palette
constexpr int SPACING = 6;        ///< Between its parts
constexpr int CURSOR_GAP = 6;     ///< Between the cursor and the palette
constexpr int LEFT_OF_CURSOR = 16;  ///< The palette starts a little to the left of the cursor

} // namespace

QuickCharacterPalette::QuickCharacterPalette(const QString& fontFamily, KeysOf keysOf,
                                             AutoCorrectOf autoCorrectOf, QWidget* parent)
    : QFrame(parent, Qt::Popup)
    , m_keysOf(std::move(keysOf))
    , m_autoCorrectOf(std::move(autoCorrectOf))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setAccessibleName(tr("Quick Character"));
    m_fonts.setFamily(fontFamily, font());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    layout->setSpacing(SPACING);
    // As large as its content: the same with nothing typed and with the characters found
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Find a character: a name or a code"));
    m_search->setAccessibleName(tr("Find a character"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textEdited, this, &QuickCharacterPalette::setSearchText);
    layout->addWidget(m_search);

    // With nothing typed: the favorites and those used last; typed: the characters found.
    // The stack is as high as the higher of the two, so the palette keeps its size.
    auto* stack = new QStackedWidget(this);
    auto* rows = new QWidget(stack);
    auto* rowsLayout = new QVBoxLayout(rows);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(SPACING);
    m_favoritesLabel = newRowLabel(tr("&Favorites"));
    m_favorites = newGrid(1, tr("Favorites"));
    m_favoritesLabel->setBuddy(m_favorites);
    m_noFavorites = new QLabel(
        tr("No favorites yet. Add them in the window Insert > Special Character..."),
        rows);
    m_noFavorites->setWordWrap(true);
    m_noFavorites->setForegroundRole(QPalette::PlaceholderText);
    m_recentLabel = newRowLabel(tr("Recently &used"));
    m_recent = newGrid(1, tr("Recently used"));
    m_recentLabel->setBuddy(m_recent);
    m_noRecent = new QLabel(tr("The characters you insert appear here."), rows);
    m_noRecent->setForegroundRole(QPalette::PlaceholderText);
    for (QWidget* widget : std::initializer_list<QWidget*>{
             m_favoritesLabel, m_favorites, m_noFavorites, m_recentLabel, m_recent,
             m_noRecent}) {
        rowsLayout->addWidget(widget);
    }
    rowsLayout->addStretch(1);
    stack->addWidget(rows);

    auto* found = new QWidget(stack);
    auto* foundLayout = new QVBoxLayout(found);
    foundLayout->setContentsMargins(0, 0, 0, 0);
    foundLayout->setSpacing(SPACING);
    m_foundLabel = newRowLabel(QString());
    m_found = newGrid(FOUND_ROWS, tr("Characters found"));
    m_found->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nothingFound = new QLabel(found);
    m_nothingFound->setWordWrap(true);
    m_nothingFound->setForegroundRole(QPalette::PlaceholderText);
    foundLayout->addWidget(m_foundLabel);
    foundLayout->addWidget(m_found);
    foundLayout->addWidget(m_nothingFound);
    foundLayout->addStretch(1);
    stack->addWidget(found);
    layout->addWidget(stack);
    connect(m_search, &QLineEdit::textChanged, stack, [stack](const QString& text) {
        stack->setCurrentIndex(text.trimmed().isEmpty() ? 0 : 1);
    });

    // The character chosen, and the window of all of them
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    auto* footer = new QHBoxLayout();
    footer->setSpacing(SPACING);
    m_details = new QLabel(this);
    m_details->setTextFormat(Qt::RichText);
    m_details->setWordWrap(true);
    footer->addWidget(m_details, 1);
    m_allButton = new QPushButton(tr("&All Characters..."), this);
    m_allButton->setAutoDefault(false);
    m_allButton->setToolTip(tr("The window Insert > Special Character... (%1)")
                                .arg(QKeySequence(Qt::ControlModifier | Qt::Key_Return)
                                         .toString(QKeySequence::NativeText)));
    connect(m_allButton, &QPushButton::clicked, this,
            &QuickCharacterPalette::showAllCharacters);
    footer->addWidget(m_allButton, 0, Qt::AlignTop);
    layout->addLayout(footer);

    // The cells of the font of the text
    for (CharacterGrid* grid : {m_favorites, m_recent, m_found}) {
        grid->setCellSide(m_fonts.cellSide());
    }
    // As wide as the rows, whatever the characters chosen say
    m_details->setFixedWidth(m_found->sizeHint().width() - m_allButton->sizeHint().width() -
                             SPACING);
    fill();
}

QLabel* QuickCharacterPalette::newRowLabel(const QString& text)
{
    auto* label = new QLabel(text, this);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

CharacterGrid* QuickCharacterPalette::newGrid(int rows, const QString& accessibleName)
{
    auto* grid = new CharacterGrid(COLUMNS, rows, this);
    grid->setAccessibleName(accessibleName);
    grid->setMinimumCells(COLUMNS, rows);
    grid->installEventFilter(this);
    // The arrows in a row choose a character of it; a click inserts it
    connect(grid, &QListWidget::currentRowChanged, this, [this, grid](int row) {
        if (row < 0) {
            return;
        }
        m_current = grid == m_recent ? m_favoriteCount + row : row;
        showCurrent();
    });
    connect(grid, &QListWidget::itemClicked, this, [this, grid](QListWidgetItem* item) {
        const int row = grid->row(item);
        m_current = grid == m_recent ? m_favoriteCount + row : row;
        insertCurrent();
    });
    return grid;
}

// ============================================================================
// Showing
// ============================================================================

QRect QuickCharacterPalette::placeFor(const QRect& cursor, const QSize& size,
                                      const QRect& screen)
{
    QRect placed(QPoint(cursor.left() - LEFT_OF_CURSOR, cursor.bottom() + 1 + CURSOR_GAP), size);
    if (placed.bottom() > screen.bottom()) {
        placed.moveBottom(cursor.top() - 1 - CURSOR_GAP);
    }
    placed.moveLeft(std::clamp(placed.left(), screen.left(),
                               std::max(screen.left(), screen.right() - size.width() + 1)));
    placed.moveTop(std::clamp(placed.top(), screen.top(),
                              std::max(screen.top(), screen.bottom() - size.height() + 1)));
    return placed;
}

void QuickCharacterPalette::showAt(const QRect& cursor)
{
    const QScreen* screen = QGuiApplication::screenAt(cursor.center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    adjustSize();
    const QRect available = screen != nullptr ? screen->availableGeometry() : QRect();
    move(available.isValid() ? placeFor(cursor, size(), available).topLeft()
                             : cursor.bottomLeft());
    show();
}

void QuickCharacterPalette::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    m_search->setFocus(Qt::PopupFocusReason);
}

void QuickCharacterPalette::setSearchText(const QString& text)
{
    if (m_search->text() != text) {
        m_search->setText(text);
    }
    fill();
}

void QuickCharacterPalette::fill()
{
    const QString search = m_search->text().trimmed();
    const auto toolTipOf = [this](char32_t code) { return this->toolTipOf(code); };
    if (search.isEmpty()) {
        const QList<char32_t> favorites = favoriteCharacters().mid(0, COLUMNS);
        const QList<char32_t> recent = recentCharacters().mid(0, COLUMNS);
        m_shown = favorites + recent;
        m_favoriteCount = static_cast<int>(favorites.size());
        m_favorites->setCharacters(favorites, m_characters, m_fonts, toolTipOf);
        m_recent->setCharacters(recent, m_characters, m_fonts, toolTipOf);
        m_favorites->setVisible(!favorites.isEmpty());
        m_noFavorites->setVisible(favorites.isEmpty());
        m_recent->setVisible(!recent.isEmpty());
        m_noRecent->setVisible(recent.isEmpty());
    } else {
        const QList<char32_t> found = m_characters.find(search);
        m_shown = found.mid(0, COLUMNS * FOUND_ROWS);
        m_favoriteCount = 0;
        m_found->setCharacters(m_shown, m_characters, m_fonts, toolTipOf);
        //: %1 characters of the %2 found are shown
        m_foundLabel->setText(found.size() > m_shown.size()
                                  ? tr("Found: the first %1 of %2").arg(m_shown.size())
                                        .arg(found.size())
                                  : tr("Found"));
        m_nothingFound->setText(tr("Nothing matches “%1”.").arg(search));
        m_found->setVisible(!found.isEmpty());
        m_nothingFound->setVisible(found.isEmpty());
    }
    m_current = m_shown.isEmpty() ? -1 : 0;
    showCurrent();
}

char32_t QuickCharacterPalette::currentCharacter() const
{
    return m_current >= 0 && m_current < m_shown.size() ? m_shown.at(m_current) : 0;
}

void QuickCharacterPalette::moveSelection(int step)
{
    if (m_shown.isEmpty()) {
        return;
    }
    m_current = std::clamp(m_current + step, 0, static_cast<int>(m_shown.size()) - 1);
    showCurrent();
}

void QuickCharacterPalette::showCurrent()
{
    // Selected in its row; a character both a favorite and used last, in the row chosen
    const bool searching = !m_search->text().trimmed().isEmpty();
    CharacterGrid* grid = searching                    ? m_found
                          : m_current < m_favoriteCount ? m_favorites
                                                        : m_recent;
    const int row = grid == m_recent ? m_current - m_favoriteCount : m_current;
    for (CharacterGrid* each : {m_favorites, m_recent, m_found}) {
        const QSignalBlocker blocker(each);
        if (each == grid && m_current >= 0) {
            each->setCurrentRow(row);
        } else {
            each->setCurrentItem(nullptr);
            each->clearSelection();
        }
    }

    const char32_t code = currentCharacter();
    if (code == 0) {
        m_details->setText(QStringLiteral("&nbsp;"));
        return;
    }
    const SpecialCharacter character = m_characters.character(code);
    QStringList parts{QStringLiteral("<b>%1</b>").arg(character.name.toHtmlEscaped()),
                      SpecialCharacters::codeText(code)};
    const QString keys = m_keysOf ? m_keysOf(code) : QString();
    if (!keys.isEmpty()) {
        parts.append(keys.toHtmlEscaped());
    }
    if (m_autoCorrectOf) {
        for (const QString& typed : m_autoCorrectOf(code)) {
            // In a font of the same widths: "->" stays two characters
            //: What auto-correct replaces with the character, as "(c)" or "--"
            parts.append(tr("auto-correct %1")
                             .toHtmlEscaped()
                             .arg(QStringLiteral("<code>%1</code>").arg(typed.toHtmlEscaped())));
        }
    }
    m_details->setText(parts.join(QStringLiteral(" · ")));
}

QString QuickCharacterPalette::toolTipOf(char32_t code) const
{
    const SpecialCharacter character = m_characters.character(code);
    QString tip =
        QStringLiteral("%1 (%2)").arg(character.name, SpecialCharacters::codeText(code));
    if (character.mark.isEmpty() && !m_fonts.has(code)) {
        //: %1 is the name of a font
        tip += QLatin1Char('\n') +
               tr("Not in the font %1: in the text another font shows it.").arg(m_fonts.family());
    }
    return tip;
}

// ============================================================================
// Insert, keys
// ============================================================================

void QuickCharacterPalette::insertCurrent()
{
    const char32_t code = currentCharacter();
    if (code == 0) {
        return;
    }
    // Closed first: the text the palette was over takes the focus back, then the character
    close();
    emit insertRequested(code);
}

void QuickCharacterPalette::showAllCharacters()
{
    close();
    emit allCharactersRequested();
}

bool QuickCharacterPalette::handleKey(const QKeyEvent* key)
{
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    if (enter && key->modifiers().testFlag(Qt::ControlModifier)) {
        showAllCharacters();
        return true;
    }
    if (enter) {
        insertCurrent();
        return true;
    }
    if (key->key() == Qt::Key_Escape) {
        close();
        return true;
    }
    return false;
}

bool QuickCharacterPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return QFrame::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    if (handleKey(key)) {
        return true;
    }
    // In the search: Up and Down choose the previous and the next character, and Left and
    // Right while nothing is typed (with a text they move in it)
    if (watched == m_search && key->modifiers() == Qt::NoModifier) {
        const bool empty = m_search->text().isEmpty();
        switch (key->key()) {
            case Qt::Key_Up:
                moveSelection(-1);
                return true;
            case Qt::Key_Down:
                moveSelection(1);
                return true;
            case Qt::Key_Left:
            case Qt::Key_Right:
                if (empty) {
                    moveSelection(key->key() == Qt::Key_Left ? -1 : 1);
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
