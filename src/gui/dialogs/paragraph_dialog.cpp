/// @file paragraph_dialog.cpp
/// @brief Format > Paragraph...

#include "kalahari/gui/dialogs/paragraph_dialog.h"

#include "kalahari/gui/widgets/length_spin_box.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>

namespace kalahari {
namespace gui {
namespace dialogs {

using editor::LineSpacing;
using editor::LineSpacingRule;
using editor::ParagraphFormat;

namespace {

/// @brief Room between the columns of a tab and between its fields
constexpr int COLUMN_SPACING = 16;
constexpr int FIELD_SPACING = 6;

/// @brief Lengths the same to a hundredth of a point
constexpr qreal SAME_LENGTH = 0.01;

/// @brief The step of the space before and after, in points, as in Word
constexpr double SPACING_STEP = 6.0;

/// @brief What the line spacing fields start with for a rule chosen anew: points for At
///        least and Exactly, lines for Multiple
constexpr double FIRST_LINE_POINTS = 12.0;
constexpr double FIRST_LINE_MULTIPLE = 1.15;
constexpr double LINE_MULTIPLE_STEP = 0.25;

/// @brief The width of the text on paper the preview shows, in points: 16 cm
constexpr qreal PREVIEW_TEXT_POINTS = 16.0 / 2.54 * 72.0;

/// @brief The preview: its margin around the page and the page's margin, the height of a
///        line, the room of a single line and of the space of a point, in pixels
constexpr int PREVIEW_MARGIN = 6;
constexpr int PAGE_MARGIN = 10;
constexpr qreal LINE_HEIGHT = 2.5;
constexpr qreal SINGLE_LINE = 7.0;
constexpr qreal SINGLE_LINE_POINTS = 14.0;  ///< Of a 12 pt font
constexpr qreal MIN_LINE = 3.5;
constexpr qreal MAX_LINE = 20.0;
constexpr qreal MAX_SPACE = 24.0;
constexpr int PREVIEW_WIDTH = 190;
constexpr int PREVIEW_HEIGHT = 200;

/// @brief The lengths of the lines of the paragraph, as shares of the width, and of the
///        paragraphs around it
constexpr std::array<qreal, 5> PARAGRAPH_LINES = {1.0, 0.93, 0.97, 0.9, 0.5};
constexpr std::array<qreal, 3> OTHER_LINES = {1.0, 1.0, 0.6};

/// @brief Whether two lengths are the same
bool sameLength(qreal a, qreal b)
{
    return std::abs(a - b) < SAME_LENGTH;
}

/// @brief A field of points: spacing before and after, a line spacing in points
QDoubleSpinBox* pointsField(double maximum, QWidget* parent)
{
    auto* field = new QDoubleSpinBox(parent);
    field->setDecimals(1);
    field->setRange(0.0, maximum);
    field->setSingleStep(SPACING_STEP);
    //: The unit of the spacing of paragraphs, points. In Polish: pt
    field->setSuffix(ParagraphDialog::tr(" pt"));
    return field;
}

/// @brief The row of the list of line spacings for a line spacing
int lineSpacingRow(const LineSpacing& spacing)
{
    switch (spacing.rule) {
        case LineSpacingRule::AtLeast:
            return ParagraphDialog::AtLeast;
        case LineSpacingRule::Exactly:
            return ParagraphDialog::Exactly;
        case LineSpacingRule::Multiple:
            break;
    }
    if (sameLength(spacing.value, 1.0)) {
        return ParagraphDialog::Single;
    }
    if (sameLength(spacing.value, 1.5)) {
        return ParagraphDialog::OneAndHalf;
    }
    if (sameLength(spacing.value, 2.0)) {
        return ParagraphDialog::Double;
    }
    return ParagraphDialog::Multiple;
}

}  // namespace

// =============================================================================
// ParagraphPreview
// =============================================================================

ParagraphPreview::ParagraphPreview(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
}

void ParagraphPreview::showParagraph(const ParagraphFormat& format, Qt::Alignment alignment)
{
    m_format = format;
    m_alignment = alignment & Qt::AlignHorizontal_Mask;
    update();
}

QSize ParagraphPreview::sizeHint() const
{
    return {PREVIEW_WIDTH, PREVIEW_HEIGHT};
}

QRectF ParagraphPreview::pageRect() const
{
    return QRectF(rect()).adjusted(PREVIEW_MARGIN, PREVIEW_MARGIN, -PREVIEW_MARGIN,
                                   -PREVIEW_MARGIN);
}

QList<QRectF> ParagraphPreview::paragraphLines() const
{
    const QRectF page = pageRect();
    const QRectF text = page.adjusted(PAGE_MARGIN, PAGE_MARGIN, -PAGE_MARGIN, -PAGE_MARGIN);
    const qreal scale = text.width() / PREVIEW_TEXT_POINTS;
    const auto space = [scale](std::optional<qreal> points) {
        return std::clamp(points.value_or(0.0) * scale, 0.0, MAX_SPACE);
    };

    // The room of a line: a multiple of a single one, or its points
    const LineSpacing spacing = m_format.lineSpacing.value_or(LineSpacing{});
    const qreal pitch = std::clamp(spacing.rule == LineSpacingRule::Multiple
                                       ? SINGLE_LINE * spacing.value
                                       : SINGLE_LINE * spacing.value / SINGLE_LINE_POINTS,
                                   MIN_LINE, MAX_LINE);

    // Under the three lines of the paragraph before, and its space after: the one of this one
    qreal top = text.top() + OTHER_LINES.size() * SINGLE_LINE + space(m_format.spaceBefore);
    const qreal left = text.left() + std::clamp(m_format.leftIndent.value_or(0.0) * scale,
                                                -PAGE_MARGIN * 1.0, text.width() / 2);
    const qreal right = text.right() - std::clamp(m_format.rightIndent.value_or(0.0) * scale,
                                                  -PAGE_MARGIN * 1.0, text.width() / 2);
    const qreal firstLine = m_format.firstLineIndent.value_or(0.0) * scale;
    QList<QRectF> lines;
    for (size_t i = 0; i < PARAGRAPH_LINES.size(); ++i) {
        const qreal start = i == 0 ? std::clamp(left + firstLine, text.left() - PAGE_MARGIN,
                                                right - LINE_HEIGHT)
                                   : left;
        const qreal room = std::max(right - start, LINE_HEIGHT);
        const bool last = i + 1 == PARAGRAPH_LINES.size();
        const qreal share = m_alignment == Qt::AlignJustify && !last ? 1.0 : PARAGRAPH_LINES[i];
        const qreal width = room * share;
        qreal x = start;
        if (m_alignment == Qt::AlignHCenter) {
            x = start + (room - width) / 2;
        } else if (m_alignment == Qt::AlignRight) {
            x = right - width;
        }
        lines.append(QRectF(x, top, width, LINE_HEIGHT));
        top += pitch;
    }
    return lines;
}

void ParagraphPreview::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const QRectF page = pageRect();
    painter.fillRect(page, palette().color(QPalette::Base));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(page.adjusted(0, 0, -1, -1));

    const QRectF text = page.adjusted(PAGE_MARGIN, PAGE_MARGIN, -PAGE_MARGIN, -PAGE_MARGIN);
    const qreal scale = text.width() / PREVIEW_TEXT_POINTS;
    QColor grey = palette().color(QPalette::Text);
    grey.setAlphaF(0.25);
    const QColor dark = palette().color(QPalette::Text);

    // The paragraph before, the paragraph and the one after
    qreal top = text.top();
    for (const qreal share : OTHER_LINES) {
        painter.fillRect(QRectF(text.left(), top, text.width() * share, LINE_HEIGHT), grey);
        top += SINGLE_LINE;
    }
    const QList<QRectF> lines = paragraphLines();
    for (const QRectF& line : lines) {
        painter.fillRect(line, dark);
    }
    top = lines.last().top() + SINGLE_LINE +
          std::clamp(m_format.spaceAfter.value_or(0.0) * scale, 0.0, MAX_SPACE);
    for (const qreal share : OTHER_LINES) {
        if (top + LINE_HEIGHT > text.bottom()) {
            break;
        }
        painter.fillRect(QRectF(text.left(), top, text.width() * share, LINE_HEIGHT), grey);
        top += SINGLE_LINE;
    }
}

// =============================================================================
// ParagraphDialog
// =============================================================================

ParagraphDialog::ParagraphDialog(const ParagraphFormat& shown, Qt::Alignment alignment,
                                 bool listItem, QWidget* parent)
    : KalahariDialog(parent)
    , m_shown(shown)
    , m_alignment(alignment & Qt::AlignHorizontal_Mask)
    , m_listItem(listItem)
{
    setHeading(tr("Paragraph"),
               tr("Indents, spacing and breaks of the selected paragraphs; the paragraph style "
                  "stays as it is."));
    setHeadingIcon(QStringLiteral("format.paragraph"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createIndentsTab(), tr("&Indents and Spacing"));
    m_tabs->addTab(createBreaksTab(), tr("Line and Page &Breaks"));
    contentLayout()->addWidget(m_tabs, 1);

    updatePreviews();
    m_alignmentBox->setFocus();
}

QWidget* ParagraphDialog::createIndentsTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* columns = new QHBoxLayout(tab);
    columns->setSpacing(COLUMN_SPACING);
    auto* fields = new QVBoxLayout();
    fields->setSpacing(FIELD_SPACING);

    // The alignment
    auto* alignmentRow = new QHBoxLayout();
    auto* alignmentLabel = new QLabel(tr("Ali&gnment"), tab);
    m_alignmentBox = new QComboBox(tab);
    m_alignmentBox->addItem(tr("Align Left"), static_cast<int>(Qt::AlignLeft));
    m_alignmentBox->addItem(tr("Align Center"), static_cast<int>(Qt::AlignHCenter));
    m_alignmentBox->addItem(tr("Align Right"), static_cast<int>(Qt::AlignRight));
    m_alignmentBox->addItem(tr("Justify"), static_cast<int>(Qt::AlignJustify));
    const int alignmentIndex = m_alignmentBox->findData(static_cast<int>(m_alignment));
    m_alignmentBox->setCurrentIndex(std::max(alignmentIndex, 0));
    alignmentLabel->setBuddy(m_alignmentBox);
    alignmentRow->addWidget(alignmentLabel);
    alignmentRow->addWidget(m_alignmentBox);
    alignmentRow->addStretch(1);
    fields->addLayout(alignmentRow);

    // The indents: left and right, and the first line's from the left one
    auto* indents = new QGroupBox(tr("Indentation"), tab);
    auto* indentGrid = new QGridLayout(indents);
    indentGrid->setHorizontalSpacing(FIELD_SPACING * 2);
    indentGrid->setVerticalSpacing(FIELD_SPACING);
    m_left = new LengthSpinBox(LengthUnit::Points, -editor::MAX_PARAGRAPH_INDENT,
                               editor::MAX_PARAGRAPH_INDENT, indents);
    m_right = new LengthSpinBox(LengthUnit::Points, -editor::MAX_PARAGRAPH_INDENT,
                                editor::MAX_PARAGRAPH_INDENT, indents);
    m_specialBox = new QComboBox(indents);
    //: Format > Paragraph, the special indent: none
    m_specialBox->addItems({tr("(none)"), tr("First line"), tr("Hanging")});
    m_special = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_PARAGRAPH_INDENT, indents);
    auto* leftLabel = new QLabel(tr("&Left"), indents);
    auto* rightLabel = new QLabel(tr("&Right"), indents);
    auto* specialLabel = new QLabel(tr("&Special"), indents);
    //: Format > Paragraph: a special indent "by" 0.63 cm. In Polish: o
    auto* byLabel = new QLabel(tr("B&y"), indents);
    leftLabel->setBuddy(m_left);
    rightLabel->setBuddy(m_right);
    specialLabel->setBuddy(m_specialBox);
    byLabel->setBuddy(m_special);
    indentGrid->addWidget(leftLabel, 0, 0);
    indentGrid->addWidget(m_left, 0, 1);
    indentGrid->addWidget(specialLabel, 0, 2);
    indentGrid->addWidget(m_specialBox, 0, 3);
    indentGrid->addWidget(rightLabel, 1, 0);
    indentGrid->addWidget(m_right, 1, 1);
    indentGrid->addWidget(byLabel, 1, 2);
    indentGrid->addWidget(m_special, 1, 3);
    fields->addWidget(indents);

    m_left->setStoredValue(m_shown.leftIndent.value_or(0.0));
    m_right->setStoredValue(m_shown.rightIndent.value_or(0.0));
    const qreal firstLine = m_shown.firstLineIndent.value_or(0.0);
    m_specialBox->setCurrentIndex(firstLine > SAME_LENGTH    ? FirstLine
                                  : firstLine < -SAME_LENGTH ? Hanging
                                                             : NoSpecial);
    m_special->setStoredValue(std::abs(firstLine));
    showSpecialRow(m_specialBox->currentIndex());
    // A list item's are those of its level, set in Bullets and Numbering
    if (m_listItem) {
        for (QWidget* field : {static_cast<QWidget*>(m_left), static_cast<QWidget*>(m_specialBox),
                               static_cast<QWidget*>(m_special)}) {
            field->setEnabled(false);
            field->setToolTip(tr("A list item has the indents of its level: Format > Bullets "
                                 "and Numbering..."));
        }
    }

    // The space before and after, and the line spacing
    auto* spacing = new QGroupBox(tr("Spacing"), tab);
    auto* spacingGrid = new QGridLayout(spacing);
    spacingGrid->setHorizontalSpacing(FIELD_SPACING * 2);
    spacingGrid->setVerticalSpacing(FIELD_SPACING);
    m_before = pointsField(editor::MAX_PARAGRAPH_SPACING, spacing);
    m_after = pointsField(editor::MAX_PARAGRAPH_SPACING, spacing);
    m_lineSpacingBox = new QComboBox(spacing);
    m_lineSpacingBox->addItems({tr("Single"), tr("1.5 lines"), tr("Double"), tr("At least"),
                                tr("Exactly"), tr("Multiple")});
    m_lineSpacing = new QDoubleSpinBox(spacing);
    auto* beforeLabel = new QLabel(tr("Be&fore"), spacing);
    auto* afterLabel = new QLabel(tr("Aft&er"), spacing);
    auto* lineLabel = new QLabel(tr("Li&ne spacing"), spacing);
    //: Format > Paragraph: line spacing At least "at" 12 pt. In Polish: co
    auto* atLabel = new QLabel(tr("&At"), spacing);
    beforeLabel->setBuddy(m_before);
    afterLabel->setBuddy(m_after);
    lineLabel->setBuddy(m_lineSpacingBox);
    atLabel->setBuddy(m_lineSpacing);
    spacingGrid->addWidget(beforeLabel, 0, 0);
    spacingGrid->addWidget(m_before, 0, 1);
    spacingGrid->addWidget(lineLabel, 0, 2);
    spacingGrid->addWidget(m_lineSpacingBox, 0, 3);
    spacingGrid->addWidget(afterLabel, 1, 0);
    spacingGrid->addWidget(m_after, 1, 1);
    spacingGrid->addWidget(atLabel, 1, 2);
    spacingGrid->addWidget(m_lineSpacing, 1, 3);
    fields->addWidget(spacing);

    m_before->setValue(m_shown.spaceBefore.value_or(0.0));
    m_after->setValue(m_shown.spaceAfter.value_or(0.0));
    const LineSpacing lineSpacing = m_shown.lineSpacing.value_or(LineSpacing{});
    m_updating = true;
    m_lineSpacingBox->setCurrentIndex(lineSpacingRow(lineSpacing));
    showLineSpacingRow(m_lineSpacingBox->currentIndex(), false);
    m_lineSpacing->setValue(lineSpacing.value);
    m_updating = false;

    auto* note = new QLabel(tr("At least and Exactly are in points, Multiple in lines."), tab);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    fields->addWidget(note);
    fields->addStretch(1);
    columns->addLayout(fields, 1);

    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(FIELD_SPACING);
    previewColumn->addWidget(new QLabel(tr("Preview"), tab));
    m_preview = new ParagraphPreview(tab);
    previewColumn->addWidget(m_preview, 1);
    columns->addLayout(previewColumn);

    // Every change shows in the preview
    const auto changed = [this]() {
        if (!m_updating) {
            updatePreviews();
        }
    };
    connect(m_alignmentBox, &QComboBox::currentIndexChanged, this, changed);
    for (QDoubleSpinBox* field : {static_cast<QDoubleSpinBox*>(m_left),
                                  static_cast<QDoubleSpinBox*>(m_right),
                                  static_cast<QDoubleSpinBox*>(m_special), m_before, m_after,
                                  m_lineSpacing}) {
        connect(field, &QDoubleSpinBox::valueChanged, this, changed);
    }
    connect(m_specialBox, &QComboBox::currentIndexChanged, this, [this](int row) {
        showSpecialRow(row);
        updatePreviews();
    });
    connect(m_lineSpacingBox, &QComboBox::currentIndexChanged, this, [this](int row) {
        showLineSpacingRow(row, false);
        updatePreviews();
    });
    return tab;
}

QWidget* ParagraphDialog::createBreaksTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* columns = new QHBoxLayout(tab);
    columns->setSpacing(COLUMN_SPACING);
    auto* fields = new QVBoxLayout();
    fields->setSpacing(FIELD_SPACING);

    auto* pagination = new QGroupBox(tr("Pagination"), tab);
    auto* boxes = new QVBoxLayout(pagination);
    boxes->setSpacing(FIELD_SPACING);
    m_widowControl = new QCheckBox(tr("&Widow/Orphan control"), pagination);
    m_keepWithNext = new QCheckBox(tr("Keep with ne&xt"), pagination);
    m_keepTogether = new QCheckBox(tr("&Keep lines together"), pagination);
    m_pageBreakBefore = new QCheckBox(tr("Page break bef&ore"), pagination);
    m_widowControl->setChecked(m_shown.widowControl.value_or(true));
    m_keepWithNext->setChecked(m_shown.keepWithNext.value_or(false));
    m_keepTogether->setChecked(m_shown.keepTogether.value_or(false));
    m_pageBreakBefore->setChecked(m_shown.pageBreakBefore.value_or(false));
    for (QCheckBox* box : {m_widowControl, m_keepWithNext, m_keepTogether, m_pageBreakBefore}) {
        boxes->addWidget(box);
    }
    fields->addWidget(pagination);

    auto* note = new QLabel(
        tr("Widow/Orphan control leaves no single line of the paragraph at the bottom or at "
           "the top of a page. The page view follows these settings; the headings keep with "
           "the next paragraph in their style."),
        tab);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    fields->addWidget(note);
    fields->addStretch(1);
    columns->addLayout(fields, 1);

    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(FIELD_SPACING);
    previewColumn->addWidget(new QLabel(tr("Preview"), tab));
    m_breaksPreview = new ParagraphPreview(tab);
    previewColumn->addWidget(m_breaksPreview, 1);
    columns->addLayout(previewColumn);
    return tab;
}

void ParagraphDialog::showLineSpacingRow(int row, bool keepValue)
{
    const bool updating = m_updating;
    m_updating = true;
    switch (row) {
        case AtLeast:
        case Exactly:
            m_lineSpacing->setEnabled(true);
            m_lineSpacing->setDecimals(1);
            m_lineSpacing->setRange(editor::MIN_LINE_SPACING_POINTS, editor::MAX_LINE_SPACING_POINTS);
            m_lineSpacing->setSingleStep(1.0);
            m_lineSpacing->setSuffix(tr(" pt"));
            if (!keepValue) {
                m_lineSpacing->setValue(FIRST_LINE_POINTS);
            }
            break;
        case Multiple:
            m_lineSpacing->setEnabled(true);
            m_lineSpacing->setDecimals(2);
            m_lineSpacing->setRange(editor::MIN_LINE_SPACING_MULTIPLE,
                                    editor::MAX_LINE_SPACING_MULTIPLE);
            m_lineSpacing->setSingleStep(LINE_MULTIPLE_STEP);
            m_lineSpacing->setSuffix(QString());
            if (!keepValue) {
                m_lineSpacing->setValue(FIRST_LINE_MULTIPLE);
            }
            break;
        default: {
            // Single, 1.5 lines and double: their multiple, greyed out
            static constexpr std::array<double, 3> MULTIPLES = {1.0, 1.5, 2.0};
            m_lineSpacing->setDecimals(2);
            m_lineSpacing->setRange(editor::MIN_LINE_SPACING_MULTIPLE,
                                    editor::MAX_LINE_SPACING_MULTIPLE);
            m_lineSpacing->setSuffix(QString());
            m_lineSpacing->setValue(MULTIPLES.at(static_cast<size_t>(std::clamp(row, 0, 2))));
            m_lineSpacing->setEnabled(false);
            break;
        }
    }
    m_updating = updating;
}

void ParagraphDialog::showSpecialRow(int row)
{
    m_special->setEnabled(row != NoSpecial && !m_listItem);
    if (row == NoSpecial) {
        m_special->setStoredValue(0.0);
    } else if (m_special->storedValue() < SAME_LENGTH) {
        // As in Word: 1.27 cm
        m_special->setStoredValue(editor::INDENT_STEP_POINTS * 1.27 / 1.25);
    }
}

ParagraphFormat ParagraphDialog::shownFormat() const
{
    ParagraphFormat format;
    format.leftIndent = m_left->storedValue();
    format.rightIndent = m_right->storedValue();
    const qreal special = m_special->storedValue();
    switch (m_specialBox->currentIndex()) {
        case FirstLine:
            format.firstLineIndent = special;
            break;
        case Hanging:
            format.firstLineIndent = -special;
            break;
        default:
            format.firstLineIndent = 0.0;
            break;
    }
    format.spaceBefore = m_before->value();
    format.spaceAfter = m_after->value();
    switch (m_lineSpacingBox->currentIndex()) {
        case OneAndHalf:
            format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, 1.5};
            break;
        case Double:
            format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, 2.0};
            break;
        case AtLeast:
            format.lineSpacing = LineSpacing{LineSpacingRule::AtLeast, m_lineSpacing->value()};
            break;
        case Exactly:
            format.lineSpacing = LineSpacing{LineSpacingRule::Exactly, m_lineSpacing->value()};
            break;
        case Multiple:
            format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, m_lineSpacing->value()};
            break;
        default:
            format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, 1.0};
            break;
    }
    format.widowControl = m_widowControl->isChecked();
    format.keepWithNext = m_keepWithNext->isChecked();
    format.keepTogether = m_keepTogether->isChecked();
    format.pageBreakBefore = m_pageBreakBefore->isChecked();
    return format;
}

Qt::Alignment ParagraphDialog::shownAlignment() const
{
    return Qt::Alignment(m_alignmentBox->currentData().toInt());
}

ParagraphFormat ParagraphDialog::changes() const
{
    const ParagraphFormat shown = shownFormat();
    ParagraphFormat changes;
    const auto changedLength = [](const std::optional<qreal>& now,
                                  const std::optional<qreal>& before,
                                  std::optional<qreal>& change) {
        if (now && (!before || !sameLength(*now, *before))) {
            change = now;
        }
    };
    if (!m_listItem) {
        changedLength(shown.leftIndent, m_shown.leftIndent, changes.leftIndent);
        changedLength(shown.firstLineIndent, m_shown.firstLineIndent, changes.firstLineIndent);
    }
    changedLength(shown.rightIndent, m_shown.rightIndent, changes.rightIndent);
    changedLength(shown.spaceBefore, m_shown.spaceBefore, changes.spaceBefore);
    changedLength(shown.spaceAfter, m_shown.spaceAfter, changes.spaceAfter);
    const LineSpacing before = m_shown.lineSpacing.value_or(LineSpacing{});
    if (shown.lineSpacing->rule != before.rule ||
        !sameLength(shown.lineSpacing->value, before.value)) {
        changes.lineSpacing = shown.lineSpacing;
    }
    const auto changedFlag = [](const std::optional<bool>& now, std::optional<bool> before,
                                bool usual, std::optional<bool>& change) {
        if (now != before.value_or(usual)) {
            change = now;
        }
    };
    changedFlag(shown.widowControl, m_shown.widowControl, true, changes.widowControl);
    changedFlag(shown.keepWithNext, m_shown.keepWithNext, false, changes.keepWithNext);
    changedFlag(shown.keepTogether, m_shown.keepTogether, false, changes.keepTogether);
    changedFlag(shown.pageBreakBefore, m_shown.pageBreakBefore, false, changes.pageBreakBefore);
    return changes;
}

std::optional<Qt::Alignment> ParagraphDialog::alignmentChange() const
{
    const Qt::Alignment shown = shownAlignment();
    return shown != m_alignment ? std::optional<Qt::Alignment>(shown) : std::nullopt;
}

void ParagraphDialog::updatePreviews()
{
    const ParagraphFormat format = shownFormat();
    const Qt::Alignment alignment = shownAlignment();
    m_preview->showParagraph(format, alignment);
    if (m_breaksPreview != nullptr) {
        m_breaksPreview->showParagraph(format, alignment);
    }
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
