/// @file spelling_issue_list.cpp
/// @brief The list of the Spelling and Grammar panel: a card for each misspelled word and
///        grammar issue, made only for the cards in view

#include "kalahari/gui/panels/spelling_issue_list.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTextOption>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr int BAR_WIDTH = 4;             ///< The bar in the kind's color on the left of a card
constexpr qreal CORNER_RADIUS = 8.0;     ///< The rounded corners of a card
constexpr int SELECTED_FRAME = 2;        ///< The frame of the selected card
constexpr int CARD_SPACING = 6;          ///< Between the cards
constexpr int LIST_MARGIN = 2;           ///< Around the cards
constexpr int ROW_SPACING = 5;           ///< Between the buttons of a row
constexpr int ROW_TOP = 5;               ///< Above a row of buttons
constexpr int WORDS_BEFORE = 3;          ///< Words before an issue on a folded card
constexpr int WORDS_AFTER = 4;           ///< Words after it
constexpr int MAX_SUGGESTIONS = 5;       ///< Suggestions on a card: the keys 1 to 5
constexpr double LIGHT_TINT = 0.07;      ///< How much of the kind's color a card has on a light surface
constexpr double DARK_TINT = 0.14;       ///< And on a dark one
constexpr double SECONDARY_SHARE = 0.38; ///< How far the less important text is from the text, toward the card
constexpr qreal DISABLED_OPACITY = 0.45; ///< A greyed-out suggestion
constexpr int PLACE_PASSES = 6;          ///< Times the cards are placed again as their heights are measured
constexpr int HINT_CARDS = 4;            ///< The list's preferred height, in folded cards
constexpr int HINT_WIDTH = 220;          ///< The list's preferred width
constexpr int MINIMUM_WIDTH = 120;       ///< The list's least width
constexpr int MEASURE_WIDTH = 240;       ///< The width an unfolded card is first measured at

/// @brief The ellipsis that shows where a paragraph goes on
const QChar ELLIPSIS(0x2026);

/// @brief The keys that move the selection: the cards' buttons and the list leave them to the
///        panel (a button would move the keys to the next button)
bool isNavigationKey(const QKeyEvent* event) {
    if ((event->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier) {
        return false;
    }
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        return true;
    default:
        return false;
    }
}

/// @brief A text in one line of a card: the line breaks in a paragraph become spaces
QString inOneLine(QString text) {
    text.replace(QChar::LineSeparator, QLatin1Char(' '));
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    return text;
}

/// @brief Whether a character ends a sentence
bool endsSentence(QChar c) {
    return c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?') ||
           c == ELLIPSIS;
}

/// @brief Whether a character closes a quotation or a bracket after the end of a sentence
bool closesQuotation(QChar c) {
    return c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char(')') ||
           c == QLatin1Char(']') || c == QChar(0x201D) || c == QChar(0x2019) ||
           c == QChar(0x00BB) || c == QChar(0x00AB);
}

/// @brief A suggestion of a card: its number (the key that chooses it) and the word, the
///        first one filled with the color of the issue's kind. The word is never shortened:
///        on a line of the card narrower than the word, it goes on in the next line of the
///        button
class SuggestionButton : public QAbstractButton {
public:
    explicit SuggestionButton(QWidget* parent) : QAbstractButton(parent) {
        setObjectName(QStringLiteral("spellingSuggestion"));
        setAttribute(Qt::WA_Hover);
        QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);  // the lines of the word
        setSizePolicy(policy);
    }

    /// @brief The number on the button (0: none)
    void setNumber(int number) {
        if (number != m_number) {
            m_number = number;
            updateGeometry();
            update();
        }
    }

    /// @brief The first suggestion: filled with the color of the issue's kind
    void setPrimary(bool primary) {
        if (primary != m_primary) {
            m_primary = primary;
            update();
        }
    }

    /// @brief The colors: the first suggestion's fill and its text, and the frame of a button
    ///        with the keys
    void setColors(const QColor& primary, const QColor& primaryText, const QColor& focus) {
        m_primaryColor = primary;
        m_primaryText = primaryText;
        m_focusColor = focus;
        update();
    }

    /// @brief The word in one line
    QSize sizeHint() const override {
        return {besideWord() + wordWidth(), lineHeight() + 2 * paddingY()};
    }

    /// @brief Narrower than the word on a narrow card: the word then goes on in more lines
    QSize minimumSizeHint() const override {
        const QFontMetrics metrics(boldFont());
        return {besideWord() +
                    std::min(wordWidth(), metrics.horizontalAdvance(QStringLiteral("www"))),
                lineHeight() + 2 * paddingY()};
    }

    int heightForWidth(int width) const override {
        const auto lines = suggestionLines(text(), boldFont(), width - besideWord()).size();
        return static_cast<int>(std::max<qsizetype>(lines, 1)) * lineHeight() + 2 * paddingY();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) {
            painter.setOpacity(DISABLED_OPACITY);
        }
        const QPalette& colors = palette();
        const QColor primary =
            m_primaryColor.isValid() ? m_primaryColor : colors.color(QPalette::Highlight);
        const QColor primaryText =
            m_primaryText.isValid() ? m_primaryText : colors.color(QPalette::HighlightedText);
        QColor fill = m_primary ? primary : colors.color(QPalette::Button);
        const QColor border = m_primary ? primary : colors.color(QPalette::Mid);
        const QColor textColor = m_primary ? primaryText : colors.color(QPalette::ButtonText);
        if (isDown()) {
            fill = fill.darker(115);
        } else if (underMouse() && isEnabled()) {
            fill = m_primary ? fill.lighter(112) : fill.darker(105);
        }
        painter.setPen(QPen(border, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

        int x = paddingX();
        if (m_number > 0) {
            const QSize cap = keycapSize();
            const QRect keycap(x, (height() - cap.height()) / 2, cap.width(), cap.height());
            painter.setPen(QPen(m_primary ? primaryText : colors.color(QPalette::Mid), 1));
            painter.setBrush(m_primary ? QBrush(Qt::NoBrush) : colors.brush(QPalette::Base));
            painter.drawRoundedRect(QRectF(keycap).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
            painter.setFont(keycapFont());
            painter.setPen(m_primary ? primaryText : colors.color(QPalette::ButtonText));
            painter.drawText(keycap, Qt::AlignCenter, QString::number(m_number));
            x += cap.width() + paddingX() / 2;
        }

        // The whole word, in as many lines as the width of the button needs
        const QFont bold = boldFont();
        painter.setFont(bold);
        painter.setPen(textColor);
        const QStringList lines = suggestionLines(text(), bold, width() - x - paddingX());
        const int ascent = QFontMetrics(bold).ascent();
        int y = (height() - static_cast<int>(lines.size()) * lineHeight()) / 2;
        for (const QString& line : lines) {
            painter.drawText(QPoint(x, y + ascent), line);
            y += lineHeight();
        }

        // The keys are on it (Tab): a frame inside its edge
        if (hasFocus()) {
            const QColor focus =
                m_focusColor.isValid() ? m_focusColor : colors.color(QPalette::Highlight);
            painter.setOpacity(1.0);
            painter.setPen(QPen(m_primary ? primaryText : focus, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), 3, 3);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (isNavigationKey(event)) {
            event->ignore();  // for the panel
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

private:
    QFont boldFont() const {
        QFont bold = font();
        bold.setBold(true);
        return bold;
    }

    QFont keycapFont() const {
        QFont small = font();
        small.setBold(true);
        if (small.pointSizeF() > 0) {
            small.setPointSizeF(small.pointSizeF() * 0.85);
        }
        return small;
    }

    QSize keycapSize() const {
        const QFontMetrics metrics(keycapFont());
        const int height = metrics.height() + 2;
        return {std::max(height, metrics.horizontalAdvance(QStringLiteral("0")) + 8), height};
    }

    int paddingX() const { return fontMetrics().averageCharWidth() + 3; }
    int paddingY() const { return 3; }
    int lineHeight() const { return QFontMetrics(boldFont()).height(); }

    /// @brief The width of the button beside its word: the margins and the number
    int besideWord() const {
        int width = 2 * paddingX();
        if (m_number > 0) {
            width += keycapSize().width() + paddingX() / 2;
        }
        return width;
    }

    /// @brief The width of the word in one line, with the fraction of a pixel it takes and
    ///        what its last letter reaches past its advance (a width rounded down would put
    ///        the last letter in the next line), and a pixel to spare
    int wordWidth() const {
        const QString word = text();
        if (word.isEmpty()) {
            return 0;
        }
        const QFontMetricsF metrics(boldFont());
        const qreal overhang = std::max<qreal>(0, -metrics.rightBearing(word.back()));
        return static_cast<int>(std::ceil(metrics.horizontalAdvance(word) + overhang)) + 1;
    }

    int m_number = 0;
    bool m_primary = false;
    QColor m_primaryColor;
    QColor m_primaryText;
    QColor m_focusColor;
};

/// @brief An action of a card ("Ignore Word"): the keys that move the selection go on to
///        the panel
class ActionButton : public QPushButton {
public:
    explicit ActionButton(QWidget* parent) : QPushButton(parent) {
        setObjectName(QStringLiteral("spellingAction"));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (isNavigationKey(event)) {
            event->ignore();
            return;
        }
        QPushButton::keyPressEvent(event);
    }
};

/// @brief A row of buttons of a card, which goes on in the next line on a narrow card
QWidget* buttonRow(QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* layout = new FlowLayout(row, ROW_SPACING, ROW_SPACING);
    layout->setContentsMargins(0, ROW_TOP, 0, 0);
    return row;
}

}  // namespace

// =============================================================================
// The words around an issue, and the colors of the cards
// =============================================================================

IssueContext shortContext(const QString& paragraph, int start, int length) {
    const int size = static_cast<int>(paragraph.size());
    start = std::clamp(start, 0, size);
    const int end = std::clamp(start + length, start, size);

    // Before: what is joined to the issue, then up to a few words with their spaces
    int from = start;
    while (from > 0 && !paragraph.at(from - 1).isSpace()) {
        --from;
    }
    for (int words = 0; words < WORDS_BEFORE && from > 0; ++words) {
        int i = from;
        while (i > 0 && paragraph.at(i - 1).isSpace()) {
            --i;
        }
        while (i > 0 && !paragraph.at(i - 1).isSpace()) {
            --i;
        }
        from = i;
    }

    // After: what is joined to it, then up to a few words with the spaces before them
    int to = end;
    while (to < size && !paragraph.at(to).isSpace()) {
        ++to;
    }
    for (int words = 0; words < WORDS_AFTER && to < size; ++words) {
        int i = to;
        while (i < size && paragraph.at(i).isSpace()) {
            ++i;
        }
        while (i < size && !paragraph.at(i).isSpace()) {
            ++i;
        }
        to = i;
    }

    IssueContext context;
    context.before = paragraph.mid(from, start - from);
    context.after = paragraph.mid(end, to - end);
    if (from > 0) {
        context.before.prepend(ELLIPSIS);
    }
    if (to < size) {
        context.after.append(ELLIPSIS);
    }
    return context;
}

IssueContext sentenceContext(const QString& paragraph, int start, int length) {
    const int size = static_cast<int>(paragraph.size());
    start = std::clamp(start, 0, size);
    const int end = std::clamp(start + length, start, size);

    // The sentence starts after the end of the one before: its mark (and the quotation it
    // closes) and a space
    int from = 0;
    for (int i = start - 1; i > 0; --i) {
        if (!paragraph.at(i).isSpace()) {
            continue;
        }
        int mark = i - 1;
        while (mark > 0 && closesQuotation(paragraph.at(mark))) {
            --mark;
        }
        if (endsSentence(paragraph.at(mark))) {
            from = i + 1;
            break;
        }
    }
    while (from < start && paragraph.at(from).isSpace()) {
        ++from;
    }

    // It ends with its mark, with the marks and quotations right after it ("?!", "...")
    int to = size;
    for (int i = end; i < size; ++i) {
        if (endsSentence(paragraph.at(i))) {
            to = i + 1;
            while (to < size &&
                   (endsSentence(paragraph.at(to)) || closesQuotation(paragraph.at(to)))) {
                ++to;
            }
            break;
        }
    }
    return {paragraph.mid(from, start - from), paragraph.mid(end, to - end)};
}

QStringList suggestionLines(const QString& suggestion, const QFont& font, int width) {
    QStringList lines;
    QTextLayout layout(suggestion, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(std::max(width, 1));
        lines.append(suggestion.mid(line.textStart(), line.textLength()));
    }
    layout.endLayout();
    return lines;
}

IssueCardColors issueCardColors(const QColor& kindColor, const QColor& surface,
                                const QColor& text) {
    IssueCardColors colors;
    AnnotationCardColors& card = colors.card;
    card.kind = kindColor;
    card.background =
        mixedColor(surface, kindColor, surface.lightness() < 128 ? DARK_TINT : LIGHT_TINT);
    card.kindName = readableColor(kindColor, card.background, MIN_TEXT_CONTRAST);
    card.text = readableColor(text, card.background, MIN_TEXT_CONTRAST);
    card.secondary = readableColor(mixedColor(card.text, card.background, SECONDARY_SHARE),
                                   card.background, MIN_TEXT_CONTRAST);
    // The frame and the first suggestion in the color itself: the one the writer sees in the
    // settings. The suggestion's word on it in the color of the card's text or of the card,
    // whichever is seen better there, darker or lighter if it has to be to be read
    colors.border = kindColor;
    const bool textSeenBetter =
        contrastRatio(card.text, kindColor) >= contrastRatio(card.background, kindColor);
    const QColor onKind = textSeenBetter ? card.text : card.background;
    colors.onBorder = readableColor(onKind, kindColor, MIN_TEXT_CONTRAST);
    return colors;
}

// =============================================================================
// SpellingIssueCard
// =============================================================================

SpellingIssueCard::SpellingIssueCard(QWidget* parent)
    : QFrame(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(BAR_WIDTH + 8, 6, 8, 7);
    layout->setSpacing(2);

    // Header: the kind and the paragraph
    auto* header = new QHBoxLayout();
    header->setSpacing(6);
    m_kindLabel = new QLabel(this);
    m_kindLabel->setObjectName(QStringLiteral("issueKind"));
    header->addWidget(m_kindLabel);
    m_placeLabel = new QLabel(this);
    m_placeLabel->setObjectName(QStringLiteral("issueMeta"));
    m_placeLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->addWidget(m_placeLabel, 1);
    layout->addLayout(header);

    // What is wrong (a grammar issue, unfolded)
    m_messageLabel = new QLabel(this);
    m_messageLabel->setObjectName(QStringLiteral("issueText"));
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_messageLabel->hide();
    layout->addWidget(m_messageLabel);

    // The words around the issue, or its sentence
    m_contextLabel = new QLabel(this);
    m_contextLabel->setObjectName(QStringLiteral("issueText"));
    m_contextLabel->setTextFormat(Qt::RichText);
    m_contextLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_contextLabel);

    // What to put in its place
    m_suggestionRow = buttonRow(this);
    m_noSuggestionsLabel = new QLabel(tr("(No suggestions)"), m_suggestionRow);
    m_noSuggestionsLabel->setObjectName(QStringLiteral("issueMeta"));
    m_suggestionRow->layout()->addWidget(m_noSuggestionsLabel);
    m_suggestionRow->hide();
    layout->addWidget(m_suggestionRow);

    // Ignore it, or add the word to the dictionary
    m_actionRow = buttonRow(this);
    for (int i = 0; i < 2; ++i) {
        auto* button = new ActionButton(m_actionRow);
        m_actionRow->layout()->addWidget(button);
        m_actionButtons.append(button);
    }
    connect(m_actionButtons.at(0), &QAbstractButton::clicked, this, [this]() {
        if (m_issue.issue.kind == editor::TextIssueKind::Spelling) {
            emit ignoreWordClicked();
        } else {
            emit ignoreRuleClicked();
        }
    });
    connect(m_actionButtons.at(1), &QAbstractButton::clicked, this,
            &SpellingIssueCard::addToDictionaryClicked);
    m_actionRow->hide();
    layout->addWidget(m_actionRow);

    updateFonts();
}

void SpellingIssueCard::setIssue(const SpellingPanelIssue& issue) {
    if (issue == m_issue) {
        return;
    }
    m_issue = issue;
    updateTexts();
    updateButtons();
}

void SpellingIssueCard::setExpanded(bool expanded) {
    if (expanded == m_expanded) {
        return;
    }
    m_expanded = expanded;
    updateTexts();
    updateButtons();
}

void SpellingIssueCard::setSuggestions(const QStringList& suggestions) {
    const QStringList shown = suggestions.mid(0, MAX_SUGGESTIONS);
    if (shown == m_suggestions) {
        return;
    }
    m_suggestions = shown;
    updateButtons();
}

QStringList SpellingIssueCard::suggestions() const {
    return m_suggestions;
}

void SpellingIssueCard::setSelected(bool selected) {
    if (selected == m_selected) {
        return;
    }
    m_selected = selected;
    updateButtons();
    update();
}

void SpellingIssueCard::setListFocused(bool focused) {
    if (focused != m_listFocused) {
        m_listFocused = focused;
        update();
    }
}

void SpellingIssueCard::setColors(const IssueCardColors& colors, const QColor& highlight) {
    if (colors == m_colors && highlight == m_highlight) {
        return;
    }
    m_colors = colors;
    m_highlight = highlight;
    applyColors();
    updateTexts();    // the issue's word is in the kind's color
    updateButtons();  // so is the first suggestion
    update();
}

void SpellingIssueCard::setReadOnly(bool readOnly) {
    if (readOnly != m_readOnly) {
        m_readOnly = readOnly;
        updateButtons();
    }
}

QList<QAbstractButton*> SpellingIssueCard::suggestionButtons() const {
    return m_suggestionButtons.mid(0, m_suggestions.size());
}

QList<QAbstractButton*> SpellingIssueCard::actionButtons() const {
    QList<QAbstractButton*> shown;
    for (QAbstractButton* button : m_actionButtons) {
        if (!button->isHidden()) {
            shown.append(button);
        }
    }
    return shown;
}

QList<QAbstractButton*> SpellingIssueCard::buttons() const {
    if (!m_expanded) {
        return {};
    }
    return suggestionButtons() + actionButtons();
}

QString SpellingIssueCard::contextText() const {
    return QTextDocumentFragment::fromHtml(m_contextLabel->text()).toPlainText();
}

void SpellingIssueCard::updateTexts() {
    const editor::TextIssue& issue = m_issue.issue;
    const bool spelling = issue.kind == editor::TextIssueKind::Spelling;
    m_kindLabel->setText(spelling ? tr("Spelling") : tr("Grammar"));
    //: The place of a misspelled word or a grammar issue on its card: the paragraph's number
    //: among those with text. In Polish: akapit %1
    m_placeLabel->setText(tr("paragraph %1").arg(m_issue.number));
    setAccessibleName(QStringLiteral("%1, %2: %3")
                          .arg(m_kindLabel->text(), m_placeLabel->text(), inOneLine(issue.text)));

    const QString message = issue.message.isEmpty() ? issue.shortMessage : issue.message;
    m_messageLabel->setText(message);
    m_messageLabel->setVisible(m_expanded && !spelling && !message.isEmpty());

    m_contextLabel->setWordWrap(m_expanded);
    if (m_expanded) {
        const IssueContext sentence =
            sentenceContext(m_issue.paragraphText, issue.start, issue.length);
        m_contextLabel->setText(
            QStringLiteral("%1<span style=\"font-weight:600; color:%2\">%3</span>%4")
                .arg(inOneLine(sentence.before).toHtmlEscaped(), m_colors.card.kindName.name(),
                     inOneLine(issue.text).toHtmlEscaped(),
                     inOneLine(sentence.after).toHtmlEscaped()));
    } else {
        updateShortContext();
    }
}

void SpellingIssueCard::updateShortContext() {
    if (m_expanded) {
        return;
    }
    const editor::TextIssue& issue = m_issue.issue;
    const IssueContext context = shortContext(m_issue.paragraphText, issue.start, issue.length);
    QString before = inOneLine(context.before);
    QString word = inOneLine(issue.text);
    QString after = inOneLine(context.after);

    // As much as the line holds: the word whole, the words after it shortened first, then
    // those before it from their start
    const QMargins margins = layout()->contentsMargins();
    const int lineWidth = width() - margins.left() - margins.right() - 2;
    QFont bold = m_contextLabel->font();
    bold.setWeight(QFont::DemiBold);
    const QFontMetrics normal(m_contextLabel->font());
    const QFontMetrics strong(bold);
    if (lineWidth > 0) {
        const int room = lineWidth - strong.horizontalAdvance(word);
        if (room < 0) {
            word = strong.elidedText(word, Qt::ElideRight, lineWidth);
            before.clear();
            after.clear();
        } else if (normal.horizontalAdvance(before) + normal.horizontalAdvance(after) > room) {
            const int beforeWidth = normal.horizontalAdvance(before);
            const int ellipsis = normal.horizontalAdvance(ELLIPSIS);
            if (beforeWidth + ellipsis <= room) {
                after = normal.elidedText(after, Qt::ElideRight, room - beforeWidth);
            } else {
                after = after.isEmpty() ? QString() : QString(ELLIPSIS);
                before = normal.elidedText(before, Qt::ElideLeft,
                                           std::max(0, room - (after.isEmpty() ? 0 : ellipsis)));
            }
        }
    }
    m_contextLabel->setText(QStringLiteral("%1<span style=\"font-weight:600; color:%2\">%3</span>%4")
                                .arg(before.toHtmlEscaped(), m_colors.card.kindName.name(),
                                     word.toHtmlEscaped(), after.toHtmlEscaped()));
}

void SpellingIssueCard::updateButtons() {
    const bool spelling = m_issue.issue.kind == editor::TextIssueKind::Spelling;
    const Qt::FocusPolicy policy = m_selected ? Qt::StrongFocus : Qt::NoFocus;

    // The suggestions: numbered on the selected card; greyed out in a text only read
    while (m_suggestionButtons.size() < m_suggestions.size()) {
        auto* button = new SuggestionButton(m_suggestionRow);
        const auto index = static_cast<int>(m_suggestionButtons.size());
        connect(button, &QAbstractButton::clicked, this,
                [this, index]() { emit suggestionChosen(index); });
        m_suggestionRow->layout()->addWidget(button);
        m_suggestionButtons.append(button);
    }
    for (qsizetype i = 0; i < m_suggestionButtons.size(); ++i) {
        auto* button = static_cast<SuggestionButton*>(m_suggestionButtons.at(i));
        const bool shown = i < m_suggestions.size();
        button->setVisible(shown);
        if (!shown) {
            continue;
        }
        button->setText(m_suggestions.at(i));
        button->setNumber(m_selected ? static_cast<int>(i) + 1 : 0);
        button->setPrimary(i == 0);
        button->setColors(m_colors.border, m_colors.onBorder, m_highlight);
        button->setEnabled(!m_readOnly);
        button->setToolTip(m_readOnly ? readOnlyReason() : QString());
        button->setFocusPolicy(policy);
    }
    m_noSuggestionsLabel->setVisible(m_suggestions.isEmpty());
    m_suggestionRow->setVisible(m_expanded);

    // Ignoring and the dictionary: greyed out in a text only read as well, as in the menu of
    // the wave
    m_actionButtons.at(0)->setText(spelling ? tr("Ignore Word") : tr("Ignore Rule"));
    m_actionButtons.at(1)->setText(tr("Add to Dictionary"));
    m_actionButtons.at(1)->setVisible(spelling);
    for (QAbstractButton* button : std::as_const(m_actionButtons)) {
        button->setEnabled(!m_readOnly);
        button->setToolTip(m_readOnly ? readOnlyReason() : QString());
        button->setFocusPolicy(policy);
    }
    m_actionRow->setVisible(m_expanded);
}

void SpellingIssueCard::applyColors() {
    if (!m_colors.card.text.isValid()) {
        return;  // no colors yet: those of the palette
    }
    // In a style sheet, the colors stay through every polish of the panel's style
    setStyleSheet(QStringLiteral("QLabel#issueKind { color: %1; }"
                                 "QLabel#issueMeta { color: %2; }"
                                 "QLabel#issueText { color: %3; }")
                      .arg(m_colors.card.kindName.name(), m_colors.card.secondary.name(),
                           m_colors.card.text.name()));
}

void SpellingIssueCard::updateFonts() {
    QFont headerFont = font();
    if (headerFont.pointSizeF() > 0) {
        headerFont.setPointSizeF(headerFont.pointSizeF() * 0.9);
    }
    QFont kindFont = headerFont;
    kindFont.setBold(true);
    m_kindLabel->setFont(kindFont);
    m_placeLabel->setFont(headerFont);

    QFont messageFont = font();
    messageFont.setWeight(QFont::DemiBold);
    m_messageLabel->setFont(messageFont);

    QFont noneFont = font();
    noneFont.setItalic(true);
    m_noSuggestionsLabel->setFont(noneFont);
}

void SpellingIssueCard::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()), CORNER_RADIUS, CORNER_RADIUS);
    painter.fillPath(shape, m_colors.card.background.isValid() ? m_colors.card.background
                                                               : palette().color(QPalette::Base));

    // The bar in the kind's color, cut by the rounded corners
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRect(0, 0, BAR_WIDTH, height()), m_colors.card.kind.isValid()
                                                           ? m_colors.card.kind
                                                           : palette().color(QPalette::Mid));
    painter.restore();

    // The frame in the kind's color; the selected card's is stronger, in the highlight's
    // color while the keys are in the list (where they act on it)
    const int frame = m_selected ? SELECTED_FRAME : 1;
    QColor color = m_colors.border.isValid() ? m_colors.border : palette().color(QPalette::Mid);
    if (m_selected && m_listFocused && m_highlight.isValid()) {
        color = m_highlight;
    }
    const qreal inset = frame / 2.0;
    painter.setPen(QPen(color, frame));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                            CORNER_RADIUS - inset, CORNER_RADIUS - inset);
}

void SpellingIssueCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void SpellingIssueCard::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateShortContext();
}

void SpellingIssueCard::changeEvent(QEvent* event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateFonts();
    }
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateShortContext();
    }
}

// =============================================================================
// SpellingIssueList
// =============================================================================

SpellingIssueList::SpellingIssueList(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setObjectName(QStringLiteral("spellingIssueList"));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    measureFolded();
    updatePositions();
}

void SpellingIssueList::setIssues(std::vector<SpellingPanelIssue> issues) {
    // The view stays on the first card it shows, as the cards before it come and go
    QString anchorKey;
    int anchorOffset = 0;
    const int scroll = verticalScrollBar()->value();
    for (int i = 0; i < count(); ++i) {
        if (m_tops.at(i) + heightOf(i) > scroll) {
            anchorKey = m_issues.at(i).key;
            anchorOffset = m_tops.at(i) - scroll;
            break;
        }
    }

    m_issues = std::move(issues);
    QHash<QString, QStringList> suggestions;
    QHash<QString, int> heights;
    for (const SpellingPanelIssue& issue : m_issues) {
        if (const auto found = m_suggestions.constFind(issue.key); found != m_suggestions.cend()) {
            suggestions.insert(issue.key, *found);
        }
        if (const auto found = m_expandedHeights.constFind(issue.key);
            found != m_expandedHeights.cend()) {
            heights.insert(issue.key, *found);
        }
    }
    m_suggestions = std::move(suggestions);
    m_expandedHeights = std::move(heights);
    if (indexOf(m_selectedKey) < 0) {
        m_selectedKey.clear();
    }

    updatePositions();
    if (const int anchor = indexOf(anchorKey); anchor >= 0) {
        verticalScrollBar()->setValue(m_tops.at(anchor) - anchorOffset);
    }
    placeCards();
}

int SpellingIssueList::indexOf(const QString& key) const {
    if (key.isEmpty()) {
        return -1;
    }
    const auto found = std::find_if(m_issues.cbegin(), m_issues.cend(),
                                    [&key](const SpellingPanelIssue& issue) { return issue.key == key; });
    return found == m_issues.cend() ? -1 : static_cast<int>(found - m_issues.cbegin());
}

void SpellingIssueList::setSelectedKey(const QString& key) {
    const QString selected = indexOf(key) >= 0 ? key : QString();
    if (selected != m_selectedKey) {
        m_selectedKey = selected;
        updatePositions();  // the selected card is unfolded, the one before folds
    }
    scrollToSelected();
}

int SpellingIssueList::selectedIndex() const {
    return indexOf(m_selectedKey);
}

void SpellingIssueList::setExpandedAll(bool all) {
    if (all != m_expandedAll) {
        m_expandedAll = all;
        updatePositions();
        placeCards();
    }
}

void SpellingIssueList::setReadOnly(bool readOnly) {
    if (readOnly != m_readOnly) {
        m_readOnly = readOnly;
        for (SpellingIssueCard* shown : std::as_const(m_shown)) {
            shown->setReadOnly(readOnly);
        }
    }
}

void SpellingIssueList::setKeysInside(bool inside) {
    if (inside != m_keysInside) {
        m_keysInside = inside;
        for (SpellingIssueCard* shown : std::as_const(m_shown)) {
            shown->setListFocused(inside);
        }
    }
}

void SpellingIssueList::setColors(const IssueCardColors& spelling, const IssueCardColors& grammar,
                                  const QColor& highlight) {
    m_spellingColors = spelling;
    m_grammarColors = grammar;
    m_highlight = highlight;
    for (SpellingIssueCard* shown : std::as_const(m_shown)) {
        shown->setColors(shown->issue().issue.kind == editor::TextIssueKind::Spelling
                             ? m_spellingColors
                             : m_grammarColors,
                         m_highlight);
    }
}

void SpellingIssueList::setSuggestionProvider(SuggestionProvider provider) {
    m_suggestionProvider = std::move(provider);
    m_suggestions.clear();
    m_expandedHeights.clear();
    updatePositions();
    placeCards();
}

void SpellingIssueList::forgetSuggestions() {
    m_suggestions.clear();
    m_expandedHeights.clear();
    updatePositions();
    placeCards();
}

QStringList SpellingIssueList::suggestionsFor(const SpellingPanelIssue& issue) {
    if (issue.issue.kind == editor::TextIssueKind::Grammar) {
        return issue.issue.suggestions.mid(0, MAX_SUGGESTIONS);
    }
    if (const auto found = m_suggestions.constFind(issue.key); found != m_suggestions.cend()) {
        return *found;
    }
    const QStringList suggestions =
        m_suggestionProvider ? m_suggestionProvider(issue).mid(0, MAX_SUGGESTIONS) : QStringList();
    m_suggestions.insert(issue.key, suggestions);
    return suggestions;
}

SpellingIssueCard* SpellingIssueList::card(const QString& key) const {
    return m_shown.value(key, nullptr);
}

SpellingIssueCard* SpellingIssueList::selectedCard() const {
    return card(m_selectedKey);
}

QString SpellingIssueList::keyAt(const QPoint& viewportPoint) const {
    for (auto it = m_shown.cbegin(); it != m_shown.cend(); ++it) {
        if (it.value()->geometry().contains(viewportPoint)) {
            return it.key();
        }
    }
    return {};
}

int SpellingIssueList::pageStepFrom(int index, int pages) const {
    if (count() == 0) {
        return -1;
    }
    if (index < 0) {
        return pages > 0 ? 0 : count() - 1;
    }
    const int target = m_tops.at(index) + pages * std::max(1, viewport()->height());
    int step = index;
    if (pages > 0) {
        while (step + 1 < count() && m_tops.at(step + 1) <= target) {
            ++step;
        }
        if (step == index && index + 1 < count()) {
            ++step;
        }
    } else {
        while (step > 0 && m_tops.at(step - 1) >= target) {
            --step;
        }
        if (step == index && index > 0) {
            --step;
        }
    }
    return step;
}

QSize SpellingIssueList::sizeHint() const {
    return {HINT_WIDTH, HINT_CARDS * (m_foldedHeight + CARD_SPACING) + 2 * LIST_MARGIN};
}

QSize SpellingIssueList::minimumSizeHint() const {
    return {MINIMUM_WIDTH, m_foldedHeight + 2 * LIST_MARGIN};
}

void SpellingIssueList::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    updatePositions();
    placeCards();
}

void SpellingIssueList::scrollContentsBy(int /*dx*/, int /*dy*/) {
    // The cards are placed again where they are now, not moved with the view
    placeCards();
}

void SpellingIssueList::keyPressEvent(QKeyEvent* event) {
    // The panel moves the selection and acts on the selected card
    event->ignore();
}

void SpellingIssueList::changeEvent(QEvent* event) {
    QAbstractScrollArea::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        measureFolded();
        m_expandedHeights.clear();
        updatePositions();
        placeCards();
    }
}

bool SpellingIssueList::viewportEvent(QEvent* event) {
    // A right click on a card selects it first: the panel shows its menu
    if (event->type() == QEvent::ContextMenu) {
        event->ignore();
        return false;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void SpellingIssueList::measureFolded() {
    // A card with a sample issue, in the viewport: the cards' font and style
    SpellingIssueCard probe(viewport());
    probe.setFont(font());
    SpellingPanelIssue sample;
    sample.issue.text = QStringLiteral("Wg");
    sample.issue.length = 2;
    sample.paragraphText = sample.issue.text;
    sample.number = 1;
    probe.setIssue(sample);
    probe.ensurePolished();
    m_foldedHeight = probe.sizeHint().height();

    sample.issue.kind = editor::TextIssueKind::Grammar;
    sample.issue.message = QStringLiteral("Wg");
    probe.setIssue(sample);
    probe.setSuggestions({QStringLiteral("Wg"), QStringLiteral("Wg")});
    probe.setSelected(true);
    probe.setExpanded(true);
    const int expanded = probe.heightForWidth(MEASURE_WIDTH);
    m_expandedEstimate = std::max(m_foldedHeight, expanded > 0 ? expanded : probe.sizeHint().height());
}

int SpellingIssueList::heightOf(int index) const {
    if (!isExpanded(index)) {
        return m_foldedHeight;
    }
    return m_expandedHeights.value(m_issues.at(index).key, m_expandedEstimate);
}

bool SpellingIssueList::isExpanded(int index) const {
    return m_expandedAll || m_issues.at(index).key == m_selectedKey;
}

void SpellingIssueList::updatePositions() {
    m_tops.resize(m_issues.size() + 1);
    int y = LIST_MARGIN;
    for (int i = 0; i < count(); ++i) {
        m_tops.at(i) = y;
        y += heightOf(i) + CARD_SPACING;
    }
    m_tops.back() = count() > 0 ? y - CARD_SPACING + LIST_MARGIN : 0;

    QScrollBar* bar = verticalScrollBar();
    const int viewHeight = viewport()->height();
    bar->setSingleStep(std::max(1, m_foldedHeight / 2));
    bar->setPageStep(std::max(1, viewHeight));
    bar->setRange(0, std::max(0, m_tops.back() - viewHeight));
}

std::vector<int> SpellingIssueList::indexesToShow() const {
    std::vector<int> indexes;
    if (m_issues.empty()) {
        return indexes;
    }
    const int top = verticalScrollBar()->value();
    const int bottom = top + viewport()->height();
    const auto first = std::upper_bound(m_tops.cbegin(), m_tops.cbegin() + count(), top);
    for (int i = std::max(0, static_cast<int>(first - m_tops.cbegin()) - 1);
         i < count() && m_tops.at(i) < bottom; ++i) {
        indexes.push_back(i);
    }
    // The selected card also out of view: the keys may be on its buttons
    const int selected = selectedIndex();
    if (selected >= 0 && std::find(indexes.cbegin(), indexes.cend(), selected) == indexes.cend()) {
        indexes.push_back(selected);
    }
    return indexes;
}

void SpellingIssueList::placeCards() {
    if (m_placing) {
        return;
    }
    const QScopedValueRollback<bool> placing(m_placing, true);
    if (m_tops.size() != m_issues.size() + 1) {
        updatePositions();
    }

    // The width changed: the unfolded cards are measured again
    const int width = cardWidth();
    if (width != m_measuredWidth) {
        m_measuredWidth = width;
        m_expandedHeights.clear();
        updatePositions();
    }

    // The unfolded cards are measured as they are made. Those above the top of the view move
    // the view with them, so the cards in view stay where they are
    QHash<QString, SpellingIssueCard*> placed;
    QScrollBar* bar = verticalScrollBar();
    std::vector<int> indexes = indexesToShow();
    for (int pass = 0; pass < PLACE_PASSES; ++pass) {
        const int top = bar->value();
        int shift = 0;
        bool moved = false;
        for (const int i : indexes) {
            const QString& key = m_issues.at(i).key;
            SpellingIssueCard* shown = placed.value(key, nullptr);
            if (shown == nullptr) {
                shown = cardFor(i);
                placed.insert(key, shown);
            }
            fillCard(shown, i);
            if (!isExpanded(i) || width <= 0) {
                continue;
            }
            int height = shown->heightForWidth(width);
            if (height <= 0) {
                height = shown->sizeHint().height();
            }
            const int before = heightOf(i);
            m_expandedHeights.insert(key, height);
            if (height != before) {
                moved = true;
                if (m_tops.at(i) < top) {
                    shift += height - before;
                }
            }
        }
        if (!moved) {
            break;
        }
        updatePositions();
        if (shift != 0) {
            bar->setValue(top + shift);
        }
        indexes = indexesToShow();
    }

    // The cards in their places; the others put away for other issues
    const int scroll = bar->value();
    QHash<QString, SpellingIssueCard*> kept;
    for (const int i : indexes) {
        const QString& key = m_issues.at(i).key;
        SpellingIssueCard* shown = placed.take(key);
        if (shown == nullptr) {
            shown = cardFor(i);
            fillCard(shown, i);
        }
        shown->setGeometry(LIST_MARGIN, m_tops.at(i) - scroll, width, heightOf(i));
        shown->show();
        kept.insert(key, shown);
    }
    const auto putAway = [this](SpellingIssueCard* away) {
        // The keys on one of its buttons go back to the list, not out of the panel
        if (away->isAncestorOf(QApplication::focusWidget())) {
            setFocus(Qt::OtherFocusReason);
        }
        away->hide();
        m_spare.append(away);
    };
    for (SpellingIssueCard* away : std::as_const(placed)) {
        putAway(away);
    }
    for (auto it = m_shown.cbegin(); it != m_shown.cend(); ++it) {
        if (!kept.contains(it.key())) {
            putAway(it.value());
        }
    }
    m_shown = std::move(kept);

    SpellingIssueCard* selected = selectedCard();
    if (selected != m_lastSelectedCard) {
        m_lastSelectedCard = selected;
        emit selectedCardChanged();
    }
}

SpellingIssueCard* SpellingIssueList::cardFor(int index) {
    const QString& key = m_issues.at(index).key;
    if (SpellingIssueCard* shown = m_shown.take(key)) {
        return shown;
    }
    if (!m_spare.isEmpty()) {
        return m_spare.takeLast();
    }
    auto* made = new SpellingIssueCard(viewport());
    connect(made, &SpellingIssueCard::clicked, this,
            [this, made]() { emit cardClicked(made->issue().key); });
    connect(made, &SpellingIssueCard::suggestionChosen, this,
            [this, made](int suggestion) { emit suggestionChosen(made->issue().key, suggestion); });
    connect(made, &SpellingIssueCard::ignoreWordClicked, this,
            [this, made]() { emit ignoreWordClicked(made->issue().key); });
    connect(made, &SpellingIssueCard::addToDictionaryClicked, this,
            [this, made]() { emit addToDictionaryClicked(made->issue().key); });
    connect(made, &SpellingIssueCard::ignoreRuleClicked, this,
            [this, made]() { emit ignoreRuleClicked(made->issue().key); });
    return made;
}

void SpellingIssueList::fillCard(SpellingIssueCard* card, int index) {
    const SpellingPanelIssue& issue = m_issues.at(index);
    const bool expanded = isExpanded(index);
    card->setColors(issue.issue.kind == editor::TextIssueKind::Spelling ? m_spellingColors
                                                                        : m_grammarColors,
                    m_highlight);
    card->setReadOnly(m_readOnly);
    card->setIssue(issue);
    if (expanded) {
        card->setSuggestions(suggestionsFor(issue));
    }
    card->setSelected(issue.key == m_selectedKey);
    card->setListFocused(m_keysInside);
    card->setExpanded(expanded);
}

void SpellingIssueList::scrollToSelected() {
    // The selected card made and measured first, so the whole of it comes into view
    placeCards();
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    QScrollBar* bar = verticalScrollBar();
    const int top = m_tops.at(index);
    const int bottom = top + heightOf(index);
    const int viewTop = bar->value();
    const int viewHeight = viewport()->height();
    if (top - LIST_MARGIN < viewTop || bottom - top + 2 * LIST_MARGIN > viewHeight) {
        bar->setValue(top - LIST_MARGIN);
    } else if (bottom + LIST_MARGIN > viewTop + viewHeight) {
        bar->setValue(bottom + LIST_MARGIN - viewHeight);
    }
    placeCards();
}

int SpellingIssueList::cardWidth() const {
    return std::max(0, viewport()->width() - 2 * LIST_MARGIN);
}

}  // namespace kalahari::gui
