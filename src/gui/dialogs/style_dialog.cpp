/// @file style_dialog.cpp
/// @brief New Style... and Modify Style...

#include "kalahari/gui/dialogs/style_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/character_format.h"
#include "kalahari/editor/paragraph_format.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/dialogs/font_dialog.h"
#include "kalahari/gui/dialogs/paragraph_dialog.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/gui/widgets/wrapping_label.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace kalahari::gui::dialogs {

namespace {

/// Room between the columns of a page and between its fields
constexpr int COLUMN_SPACING = 16;
constexpr int FIELD_SPACING = 6;

/// The lists of the tab Font: the rows they show and their widths
constexpr int LIST_ROWS = 6;
constexpr int FAMILY_LIST_WIDTH = 150;
constexpr int TYPEFACE_LIST_WIDTH = 110;
constexpr int SIZE_LIST_WIDTH = 56;

/// The font and its size in the short window
constexpr int SHORT_FONT_WIDTH = 172;
constexpr int SHORT_SIZE_WIDTH = 64;

/// The previews: beside the tabs, and under the formatting of the short window
constexpr int TABS_PREVIEW_WIDTH = 200;
constexpr int TABS_PREVIEW_HEIGHT = 240;
constexpr int SHORT_PREVIEW_HEIGHT = 96;

/// The sizes of the letters a style can have, in points (as in Format > Font)
constexpr double MIN_SIZE = 1.0;
constexpr double MAX_SIZE = 1638.0;

/// The steps of the space before and after, in points, and of a multiple line spacing
constexpr double SPACING_STEP = 6.0;
constexpr double LINE_MULTIPLE_STEP = 0.25;

/// What a choice starts with: Multiple, a first line or a hanging indent (1.27 cm, as in Word)
constexpr double FIRST_LINE_MULTIPLE = 1.15;
constexpr double FIRST_INDENT_POINTS = 1.27 / 2.54 * 72.0;

/// The multiples of single, 1.5 lines and double line spacing
constexpr std::array<double, 3> LINE_MULTIPLES = {1.0, 1.5, 2.0};

/// Numbers the same to a hundredth
constexpr double SAME_NUMBER = 0.005;

/// The setting of the way the window shows a style, and its values
constexpr const char* VARIANT_SETTING = "ui.styleWindow";
constexpr const char* TABS_VALUE = "tabs";
constexpr const char* SHORT_VALUE = "short";

/// The preview: its padding; the pixels of a point of the letters and of an indent or a
/// space, from a narrow preview to a wide one; the share of the space before and after
constexpr qreal PREVIEW_PADDING = 10.0;
constexpr qreal NARROW_WIDTH = 200.0;
constexpr qreal WIDE_WIDTH = 560.0;
constexpr qreal NARROW_LETTERS = 10.0 / 12.0;
constexpr qreal WIDE_LETTERS = 12.5 / 12.0;
constexpr qreal NARROW_SPACES = 0.42;
constexpr qreal WIDE_SPACES = 0.6;
constexpr qreal SPACE_SHARE = 0.8;
constexpr qreal MIN_LETTER_PIXELS = 6.0;
constexpr qreal MAX_LETTER_PIXELS = 40.0;
constexpr qreal MIN_LINE_WIDTH = 24.0;
constexpr qreal MIN_LINE_HEIGHT = 0.5;
constexpr float GREY_ALPHA = 0.45F;
constexpr int POINTS_PER_INCH = 72;

/// The alignments, in the order of their buttons and of their list
struct Alignment {
    const char* name;  ///< As a style has it
    Qt::AlignmentFlag flag;
    const char* iconId;
};
constexpr std::array<Alignment, 4> ALIGNMENTS = {
    {{"left", Qt::AlignLeft, "format.alignLeft"},
     {"center", Qt::AlignHCenter, "format.alignCenter"},
     {"right", Qt::AlignRight, "format.alignRight"},
     {"justify", Qt::AlignJustify, "format.justify"}}};

/// The name of an alignment, with the key that gives it
QString alignmentText(size_t index) {
    switch (index) {
        case 0:
            return StyleDialog::tr("Align Left");
        case 1:
            return StyleDialog::tr("Align Center");
        case 2:
            return StyleDialog::tr("Align Right");
        default:
            return StyleDialog::tr("Justify");
    }
}

/// How wide a preview is, from a narrow one (0) to a wide one (1)
qreal widthShare(int width) {
    return std::clamp((width - NARROW_WIDTH) / (WIDE_WIDTH - NARROW_WIDTH), 0.0, 1.0);
}

/// Whether two numbers are the same to a hundredth
bool sameNumber(double a, double b) {
    return std::abs(a - b) < SAME_NUMBER;
}

/// A number of points as the fields show it: "12", "10,5"
QString pointsText(double points) {
    return QLocale().toString(points, 'g', 4);
}

/// A number of points read from a field; nothing for a size a style cannot have
std::optional<double> pointsOf(const QString& text) {
    bool ok = false;
    const double points = QLocale().toDouble(text.trimmed(), &ok);
    return ok && points >= MIN_SIZE && points <= MAX_SIZE ? std::optional<double>(points)
                                                          : std::nullopt;
}

/// The row of the list of line spacings for a multiple
int lineSpacingRow(double multiple) {
    for (size_t row = 0; row < LINE_MULTIPLES.size(); ++row) {
        if (sameNumber(multiple, LINE_MULTIPLES.at(row))) {
            return static_cast<int>(row);
        }
    }
    return StyleDialog::Multiple;
}

/// The fonts of the system, without the private ones
QStringList systemFamilies() {
    QStringList families = QFontDatabase::families();
    families.erase(std::remove_if(families.begin(), families.end(),
                                  [](const QString& family) {
                                      return QFontDatabase::isPrivateFamily(family);
                                  }),
                   families.end());
    return families;
}

/// A label in the color of hints, wrapped where it does not fit
WrappingLabel* hintLabel(QWidget* parent) {
    auto* label = new WrappingLabel(QString(), parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// A field of points: the space before and after
QDoubleSpinBox* pointsField(QWidget* parent) {
    auto* field = new QDoubleSpinBox(parent);
    field->setDecimals(1);
    field->setRange(0.0, editor::MAX_PARAGRAPH_SPACING);
    field->setSingleStep(SPACING_STEP);
    //: The unit of the spacing of paragraphs, points. In Polish: pt
    field->setSuffix(StyleDialog::tr(" pt"));
    return field;
}

QString normalId() {
    return QString::fromLatin1(editor::NORMAL_STYLE_ID);
}

}  // namespace

// =============================================================================
// StylePreview
// =============================================================================

StylePreview::StylePreview(QWidget* parent)
    : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void StylePreview::showStyle(const QVariantMap& before, const QVariantMap& style,
                             const QVariantMap& after) {
    m_before = before;
    m_style = style;
    m_after = after;
    update();
}

QFont StylePreview::styleFont() const {
    return fontOf(m_style);
}

QSize StylePreview::sizeHint() const {
    return minimumSize().expandedTo(minimumSizeHint());
}

QSize StylePreview::minimumSizeHint() const {
    return {static_cast<int>(NARROW_WIDTH) / 2, SHORT_PREVIEW_HEIGHT / 2};
}

qreal StylePreview::letterScale() const {
    return NARROW_LETTERS + (WIDE_LETTERS - NARROW_LETTERS) * widthShare(width());
}

qreal StylePreview::spaceScale() const {
    return NARROW_SPACES + (WIDE_SPACES - NARROW_SPACES) * widthShare(width());
}

QFont StylePreview::fontOf(const QVariantMap& look) const {
    QFont font = this->font();
    const QString family = look.value(QStringLiteral("fontFamily")).toString();
    if (!family.isEmpty()) {
        font.setFamilies({family});
    }
    const qreal pixels =
        std::clamp(look.value(QStringLiteral("fontSize")).toDouble() * letterScale(),
                   MIN_LETTER_PIXELS, MAX_LETTER_PIXELS);
    font.setPointSizeF(pixels * POINTS_PER_INCH / std::max(logicalDpiY(), 1));
    font.setBold(look.value(QStringLiteral("bold")).toBool());
    font.setItalic(look.value(QStringLiteral("italic")).toBool());
    font.setUnderline(look.value(QStringLiteral("underline")).toBool());
    return font;
}

qreal StylePreview::drawParagraph(QPainter& painter, const QString& text, const QVariantMap& look,
                                  qreal top, const QRectF& area) const {
    const qreal space = spaceScale();
    const auto length = [&look, space](const char* name) {
        return look.value(QLatin1String(name)).toDouble() * space;
    };
    // A hanging first line stays in the preview: the paragraph moves right by it
    const qreal firstLine = length("firstLineIndent");
    const qreal left = std::max(length("leftMargin"), -firstLine);
    const qreal right = std::max(length("rightMargin"), 0.0);
    const qreal width = std::max(area.width() - left - right, MIN_LINE_WIDTH);
    const qreal lineHeight =
        std::max(look.value(QStringLiteral("lineHeight")).toDouble(), MIN_LINE_HEIGHT);
    top += std::max(length("spaceBefore"), 0.0) * SPACE_SHARE;

    QTextLayout layout(text, fontOf(look), this);
    QTextOption option(
        editor::styleAlignmentOf(look.value(QStringLiteral("alignment")).toString()));
    option.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal y = top;
    bool first = true;
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        const qreal indent = first ? firstLine : 0.0;
        line.setLineWidth(std::max(width - indent, MIN_LINE_WIDTH));
        line.setPosition(QPointF(area.left() + left + indent, y));
        y += line.height() * lineHeight;
        first = false;
    }
    layout.endLayout();
    layout.draw(&painter, QPointF());
    return y + std::max(length("spaceAfter"), 0.0) * SPACE_SHARE;
}

void StylePreview::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    QPainter painter(this);
    painter.setClipRect(contentsRect());
    const QRectF area = QRectF(contentsRect()).adjusted(PREVIEW_PADDING, PREVIEW_PADDING / 2,
                                                        -PREVIEW_PADDING, -PREVIEW_PADDING);
    QColor grey = palette().color(QPalette::Text);
    grey.setAlphaF(GREY_ALPHA);

    // The paragraph before in Normal, the style's, and the one after in its next style
    painter.setPen(grey);
    //: The paragraph before the one of the style in the preview of the window of a style.
    //: In Polish: Wieczorem wiatr ucichł i nad obozem zrobiło się cicho.
    qreal top = drawParagraph(
        painter, tr("In the evening the wind dropped and the camp went quiet."), m_before,
        area.top(), area);
    painter.setPen(palette().color(QPalette::Text));
    //: The paragraph of the style in its preview. In Polish: Pamiętał tamten ranek nad rzeką:
    //: mgła wisiała nisko, a łodzie stały jeszcze przy brzegu.
    top = drawParagraph(painter,
                        tr("He remembered that morning by the river: the mist hung low, and the "
                           "boats still lay by the bank."),
                        m_style, top, area);
    painter.setPen(grey);
    //: The paragraph after the one of the style in its preview. In Polish: Rano ruszyli dalej
    //: na północ, wzdłuż wyschniętego koryta.
    drawParagraph(painter, tr("In the morning they went on north, along the dry riverbed."),
                  m_after, top, area);
}

// =============================================================================
// StyleDialog
// =============================================================================

StyleDialog::StyleDialog(const StyleDraft& draft, const QString& typeName, bool offerApply,
                         QWidget* parent)
    : KalahariDialog(parent)
    , m_draft(draft)
    , m_typeName(typeName)
    , m_offerApply(offerApply && draft.kind() == StyleDraft::Kind::New)
    , m_variant(savedVariant()) {
    const bool isNew = m_draft.kind() == StyleDraft::Kind::New;
    //: In Polish: Styl akapitów tej książki. Czego nie ustawia sam, bierze ze stylu bazowego.
    QString description = tr("A paragraph style of this book. What it does not set itself, it "
                             "takes from its base style.");
    if (m_draft.kind() == StyleDraft::Kind::Type) {
        //: In Polish: Styl typu „%1”. Zmiany obowiązują tylko w tej książce; „Przywróć styl
        //: typu” je cofa.
        description = tr("A style of the type \"%1\". The changes are only for this book; Reset "
                         "to the Type's Style undoes them.")
                          .arg(m_typeName);
    } else if (m_draft.kind() == StyleDraft::Kind::Normal) {
        //: In Polish: Podstawowy styl książki: pozostałe style biorą z niego to, czego nie
        //: ustawiają same.
        description = tr("The base style of the book: the other styles take from it what they "
                         "do not set themselves.");
    }
    //: The window of a new paragraph style. In Polish: Nowy styl
    setHeading(isNew ? tr("New Style")
                     //: In Polish: Zmień styl „%1”
                     : tr("Modify the Style \"%1\"").arg(m_draft.name()),
               description);
    setHeadingIcon(isNew ? QStringLiteral("format.style.new")
                         : QStringLiteral("format.style.modify"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createTabsPage());
    m_stack->addWidget(createShortPage());
    contentLayout()->addWidget(m_stack, 1);

    // Under the window: the other way of showing it, and whether the new style goes to the
    // paragraph at the cursor
    m_variantButton = addSideButton(QString());
    connect(m_variantButton, &QPushButton::clicked, this, [this]() {
        setVariant(m_variant == Variant::Tabs ? Variant::Short : Variant::Tabs);
    });
    //: In Polish: Nadaj styl akapitowi z kursorem
    m_applyBox = new QCheckBox(tr("Apply to the paragraph at the c&ursor"));
    m_applyBox->setChecked(true);
    m_applyBox->setVisible(m_offerApply);
    addSideWidget(m_applyBox);

    for (Common& parts : m_parts) {
        connectCommon(parts);
    }
    addShortcuts();
    refresh();
    showVariant();

    // The name first; a style of the type keeps it
    if (!m_draft.isFixed()) {
        nameField()->setFocus();
        nameField()->selectAll();
    } else if (m_variant == Variant::Tabs) {
        m_tabs->setFocus();
    } else {
        m_shortFont->setFocus();
    }
}

// -----------------------------------------------------------------------------
// The variants
// -----------------------------------------------------------------------------

StyleDialog::Variant StyleDialog::savedVariant() {
    const std::string saved = core::SettingsManager::getInstance().get<std::string>(
        VARIANT_SETTING, TABS_VALUE);
    return saved == SHORT_VALUE ? Variant::Short : Variant::Tabs;
}

void StyleDialog::setVariant(Variant variant) {
    m_variant = variant;
    core::SettingsManager::getInstance().set<std::string>(
        VARIANT_SETTING, variant == Variant::Short ? SHORT_VALUE : TABS_VALUE);
    showVariant();
}

void StyleDialog::showVariant() {
    const int index = m_variant == Variant::Short ? 1 : 0;
    // The page not shown takes no room: the window has the size of the one shown
    for (int page = 0; page < m_stack->count(); ++page) {
        const QSizePolicy::Policy policy = page == index ? QSizePolicy::Preferred
                                                         : QSizePolicy::Ignored;
        m_stack->widget(page)->setSizePolicy(policy, policy);
    }
    m_stack->setCurrentIndex(index);
    if (m_variant == Variant::Tabs) {
        //: Switches the window of a style to its short variant. In Polish: Krótsze okno
        m_variantButton->setText(tr("S&horter Window"));
        //: In Polish: Czcionka, rozmiar, wyrównanie i interlinia na wierzchu, szeroki podgląd;
        //: reszta w oknach Czcionka i Akapit. Okno zapamiętuje wybór.
        m_variantButton->setToolTip(
            tr("The font, size, alignment and line spacing on top with a wide preview; the rest "
               "in the windows Font and Paragraph. The window remembers the choice."));
    } else {
        //: Switches the window of a style to its variant with tabs. In Polish: Okno z kartami
        m_variantButton->setText(tr("Window with &Tabs"));
        //: In Polish: Wszystkie ustawienia stylu na czterech kartach. Okno zapamiętuje wybór.
        m_variantButton->setToolTip(
            tr("All the settings of the style on four tabs. The window remembers the choice."));
    }
    m_stack->updateGeometry();
    if (isVisible()) {
        fitToScreen(sizeHint());
    }
}

const StyleDialog::Common& StyleDialog::partsOf(std::optional<Variant> variant) const {
    return m_parts.at(variant.value_or(m_variant) == Variant::Short ? 1 : 0);
}

QLineEdit* StyleDialog::nameField(std::optional<Variant> variant) const {
    return partsOf(variant).name;
}

QComboBox* StyleDialog::baseBox(std::optional<Variant> variant) const {
    return partsOf(variant).base;
}

QComboBox* StyleDialog::nextBox(std::optional<Variant> variant) const {
    return partsOf(variant).next;
}

QLabel* StyleDialog::problemLabel(std::optional<Variant> variant) const {
    return partsOf(variant).problem;
}

WrappingLabel* StyleDialog::summaryLabel(std::optional<Variant> variant) const {
    return partsOf(variant).summary;
}

QPushButton* StyleDialog::resetButton(std::optional<Variant> variant) const {
    return partsOf(variant).reset;
}

QCheckBox* StyleDialog::spellCheckBox(std::optional<Variant> variant) const {
    return partsOf(variant).spellCheck;
}

StylePreview* StyleDialog::preview(std::optional<Variant> variant) const {
    return partsOf(variant).preview;
}

QToolButton* StyleDialog::alignmentButton(Qt::Alignment alignment) const {
    for (size_t i = 0; i < ALIGNMENTS.size(); ++i) {
        if (ALIGNMENTS.at(i).flag == (alignment & Qt::AlignHorizontal_Mask)) {
            return qobject_cast<QToolButton*>(m_alignments->button(static_cast<int>(i)));
        }
    }
    return nullptr;
}

bool StyleDialog::applyToText() const {
    return m_offerApply && m_applyBox->isChecked();
}

bool StyleDialog::isMarkedOwn(const QString& property) const {
    for (const auto& [label, properties] : m_ownMarks) {
        if (properties.contains(property)) {
            return label->font().bold();
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// The parts both variants have
// -----------------------------------------------------------------------------

void StyleDialog::createNameFields(Common& parts, QWidget* page, QGridLayout* grid,
                                   bool sideBySide) {
    parts.name = new QLineEdit(page);
    parts.problem = new QLabel(page);
    parts.problem->setWordWrap(true);
    parts.problem->hide();
    parts.base = new QComboBox(page);
    parts.next = new QComboBox(page);
    //: In Polish: &Nazwa
    auto* nameLabel = new QLabel(tr("&Name"), page);
    //: The style a style is based on. In Polish: Na po&dstawie
    auto* baseLabel = new QLabel(tr("Based &on"), page);
    //: The style Enter starts after a paragraph of the style. In Polish: Następny &akapit
    auto* nextLabel = new QLabel(tr("Ne&xt paragraph"), page);
    nameLabel->setBuddy(parts.name);
    baseLabel->setBuddy(parts.base);
    nextLabel->setBuddy(parts.next);
    parts.next->setToolTip(tr("The style Enter starts at the end of a paragraph of this style"));

    if (m_draft.isFixed()) {
        parts.name->setEnabled(false);
        parts.base->setEnabled(false);
        //: In Polish: Nazwa stylu typu książki się nie zmienia; własny styl można nazwać
        //: dowolnie.
        parts.name->setToolTip(tr("A style of the book's type keeps its name; a style of the "
                                  "writer's own can have any name."));
    }

    if (sideBySide) {
        grid->addWidget(nameLabel, 0, 0);
        grid->addWidget(parts.name, 0, 1, 1, 3);
        grid->addWidget(parts.problem, 1, 1, 1, 3);
        grid->addWidget(baseLabel, 2, 0);
        grid->addWidget(parts.base, 2, 1);
        grid->addWidget(nextLabel, 2, 2);
        grid->addWidget(parts.next, 2, 3);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);
    } else {
        grid->addWidget(nameLabel, 0, 0);
        grid->addWidget(parts.name, 0, 1);
        grid->addWidget(parts.problem, 1, 1);
        grid->addWidget(baseLabel, 2, 0);
        grid->addWidget(parts.base, 2, 1);
        grid->addWidget(nextLabel, 3, 0);
        grid->addWidget(parts.next, 3, 1);
        grid->setColumnStretch(1, 1);
    }

    // The styles it can be based on: not itself, nor one based on it
    QStringList bases;
    QString emptyBase;
    if (m_draft.kind() == StyleDraft::Kind::Normal) {
        //: The base of the style Normal. In Polish: (brak – styl podstawowy)
        emptyBase = tr("(none – the base style)");
    } else if (m_draft.baseStyle().isEmpty()) {
        //: A style without a base style. In Polish: (brak)
        emptyBase = tr("(none)");
    }
    if (m_draft.kind() != StyleDraft::Kind::Normal && !m_draft.baseStyle().isEmpty()) {
        bases = m_draft.isFixed() ? QStringList{m_draft.baseStyle()}
                                  : editor::possibleBaseStyles(m_draft.styles(), m_draft.id());
    } else if (!m_draft.isFixed()) {
        bases = editor::possibleBaseStyles(m_draft.styles(), m_draft.id());
    }
    fillStyleBox(parts.base, bases, emptyBase);

    QStringList nexts;
    for (const editor::ParagraphStyle& style : m_draft.styles().styles()) {
        if (style.id != m_draft.id() || m_draft.id().isEmpty()) {
            nexts.append(style.id);
        }
    }
    //: The next paragraph in the same style. In Polish: (ten sam styl)
    fillStyleBox(parts.next, nexts, tr("(the same style)"));
}

QWidget* StyleDialog::createSummary(Common& parts, QWidget* page) {
    parts.summary = new WrappingLabel(QString(), page);
    parts.summary->setWordWrap(true);
    parts.summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    parts.reset = new QPushButton(page);
    parts.reset->setAutoDefault(false);
    if (m_draft.isFixed()) {
        //: In Polish: Przywróć &styl typu
        parts.reset->setText(tr("&Reset to the Type's Style"));
        //: In Polish: Styl znów taki jak w typie „%1”
        parts.reset->setToolTip(tr("The style as the type \"%1\" has it").arg(m_typeName));
    } else {
        //: In Polish: &Usuń własne ustawienia
        parts.reset->setText(tr("&Clear Own Settings"));
        //: In Polish: Wszystko jak w stylu bazowym
        parts.reset->setToolTip(tr("Everything as in the base style"));
    }
    return parts.summary;
}

void StyleDialog::fillStyleBox(QComboBox* box, const QStringList& ids, const QString& emptyText) {
    if (!emptyText.isEmpty()) {
        box->addItem(emptyText, QString());
    }
    for (const QString& id : ids) {
        const editor::ParagraphStyle* style = m_draft.styles().find(id);
        box->addItem(style != nullptr && !style->name.isEmpty() ? style->name : id, id);
    }
}

void StyleDialog::connectCommon(Common& parts) {
    connect(parts.name, &QLineEdit::textEdited, this, [this](const QString& name) {
        m_draft.setName(name);
        refresh();
    });
    connect(parts.base, &QComboBox::activated, this, [this, box = parts.base](int row) {
        m_draft.setBaseStyle(box->itemData(row).toString());
        refresh();
    });
    connect(parts.next, &QComboBox::activated, this, [this, box = parts.next](int row) {
        m_draft.setNextStyle(box->itemData(row).toString());
        refresh();
    });
    connect(parts.reset, &QPushButton::clicked, this, [this]() {
        m_draft.reset();
        refresh();
    });
    connect(parts.spellCheck, &QCheckBox::toggled, this,
            [this](bool on) { setValue(QStringLiteral("spellCheck"), on); });
}

void StyleDialog::addShortcuts() {
    const auto add = [this](const QKeySequence& keys, std::function<void()> action) {
        auto* shortcut = new QShortcut(keys, this);
        connect(shortcut, &QShortcut::activated, this, std::move(action));
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_B), [this]() {
        setValue(QStringLiteral("bold"), !value(QStringLiteral("bold")).toBool());
    });
    add(QKeySequence(Qt::CTRL | Qt::Key_I), [this]() {
        setValue(QStringLiteral("italic"), !value(QStringLiteral("italic")).toBool());
    });
    const std::array<Qt::Key, 4> keys = {Qt::Key_L, Qt::Key_E, Qt::Key_R, Qt::Key_J};
    for (size_t i = 0; i < ALIGNMENTS.size(); ++i) {
        add(QKeySequence(Qt::CTRL | keys.at(i)), [this, i]() {
            setValue(QStringLiteral("alignment"), QString::fromLatin1(ALIGNMENTS.at(i).name));
        });
    }
}

// -----------------------------------------------------------------------------
// On tabs
// -----------------------------------------------------------------------------

QWidget* StyleDialog::createTabsPage() {
    auto* page = new QWidget(this);
    auto* columns = new QHBoxLayout(page);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(COLUMN_SPACING);

    m_tabs = new QTabWidget(page);
    //: A tab of the window of a style. In Polish: &Ogólne
    m_tabs->addTab(createGeneralTab(), tr("&General"));
    //: In Polish: &Czcionka
    m_tabs->addTab(createFontTab(), tr("&Font"));
    //: In Polish: &Wcięcia i odstępy
    m_tabs->addTab(createIndentsTab(), tr("&Indents and Spacing"));
    //: In Polish: Po&działy i pisownia
    m_tabs->addTab(createBreaksTab(), tr("&Breaks and Spelling"));
    columns->addWidget(m_tabs, 1);

    // The preview beside every tab
    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(FIELD_SPACING);
    //: In Polish: Podgląd
    previewColumn->addWidget(new QLabel(tr("Preview"), page));
    m_parts[0].preview = new StylePreview(page);
    m_parts[0].preview->setMinimumSize(TABS_PREVIEW_WIDTH, TABS_PREVIEW_HEIGHT);
    m_parts[0].preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    previewColumn->addWidget(m_parts[0].preview, 1);
    columns->addLayout(previewColumn);
    return page;
}

QWidget* StyleDialog::createGeneralTab() {
    Common& parts = m_parts[0];
    auto* tab = new QWidget();
    auto* box = new QVBoxLayout(tab);
    box->setSpacing(FIELD_SPACING);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(FIELD_SPACING * 2);
    grid->setVerticalSpacing(FIELD_SPACING);
    createNameFields(parts, tab, grid, false);
    box->addLayout(grid);

    // What the style is, in one sentence, with Reset
    //: In Polish: Styl zawiera
    auto* has = new QGroupBox(tr("The Style Has"), tab);
    auto* hasBox = new QVBoxLayout(has);
    hasBox->setSpacing(FIELD_SPACING);
    hasBox->addWidget(createSummary(parts, has));
    auto* resetRow = new QHBoxLayout();
    resetRow->addWidget(parts.reset);
    resetRow->addStretch(1);
    hasBox->addLayout(resetRow);
    box->addWidget(has);

    m_ownHint = hintLabel(tab);
    box->addWidget(m_ownHint);
    box->addStretch(1);
    return tab;
}

QWidget* StyleDialog::createFontTab() {
    auto* tab = new QWidget();
    auto* box = new QVBoxLayout(tab);
    box->setSpacing(FIELD_SPACING);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(FIELD_SPACING * 2);
    grid->setVerticalSpacing(FIELD_SPACING);

    // A field over its list, as in Format > Font
    const auto fieldWithList = [this, tab, grid](const QString& text, int column,
                                                QLineEdit*& field, QListWidget*& list) {
        auto* label = new QLabel(text, tab);
        field = new QLineEdit(tab);
        list = new QListWidget(tab);
        label->setBuddy(field);
        list->setFocusPolicy(Qt::ClickFocus);
        auto* stack = new QVBoxLayout();
        stack->setSpacing(0);
        stack->addWidget(field);
        stack->addWidget(list);
        grid->addWidget(label, 0, column);
        grid->addLayout(stack, 1, column);
        field->installEventFilter(this);
        connect(field, &QLineEdit::textEdited, this,
                [this, list](const QString& typed) { findInList(list, typed); });
        return label;
    };
    //: In Polish: Czcio&nka
    markOwn(fieldWithList(tr("Fo&nt"), 0, m_familyField, m_familyList),
            {QStringLiteral("fontFamily")});
    //: The style of the letters: regular, italic, bold. In Polish: St&yl
    markOwn(fieldWithList(tr("St&yle"), 1, m_typefaceField, m_typefaceList),
            {QStringLiteral("bold"), QStringLiteral("italic")});
    //: In Polish: &Rozmiar
    markOwn(fieldWithList(tr("&Size"), 2, m_sizeField, m_sizeList), {QStringLiteral("fontSize")});
    grid->setColumnStretch(0, 3);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);

    m_familyList->addItems(systemFamilies());
    //: The styles of the letters, as in Format > Font. In Polish: Normalny, Kursywa,
    //: Pogrubiony, Pogrubiona kursywa
    m_typefaceList->addItems({tr("Regular"), tr("Italic"), tr("Bold"), tr("Bold Italic")});
    for (const qreal size : FontDialog::offeredSizes()) {
        m_sizeList->addItem(pointsText(size));
    }
    auto* sizeValidator = new QDoubleValidator(MIN_SIZE, MAX_SIZE, 1, m_sizeField);
    sizeValidator->setNotation(QDoubleValidator::StandardNotation);
    m_sizeField->setValidator(sizeValidator);
    for (QListWidget* list : {m_familyList, m_typefaceList, m_sizeList}) {
        list->setFixedHeight(list->sizeHintForRow(0) * LIST_ROWS + list->frameWidth() * 2);
        list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    m_familyList->setMinimumWidth(FAMILY_LIST_WIDTH);
    m_typefaceList->setMinimumWidth(TYPEFACE_LIST_WIDTH);
    m_sizeList->setMinimumWidth(SIZE_LIST_WIDTH);
    box->addLayout(grid);

    //: In Polish: Po&dkreślenie
    m_underline = new QCheckBox(tr("Un&derline"), tab);
    markOwn(m_underline, {QStringLiteral("underline")});
    box->addWidget(m_underline);

    m_screenNote = hintLabel(tab);
    //: The note about Normal on the tab Font of its window. In Polish: Na ekranie tekst w
    //: stylu „%1” ma czcionkę i rozmiar ustawione w Ustawieniach (Edytor › Ogólne); pozostałe
    //: style zachowują wobec niego te same proporcje.
    m_screenNote->setText(tr("On the screen the text in the style \"%1\" has the font and size "
                             "set in Settings (Editor > General); the other styles keep the same "
                             "proportions to it.")
                              .arg(m_draft.name()));
    m_screenNote->setVisible(m_draft.kind() == StyleDraft::Kind::Normal);
    box->addWidget(m_screenNote);
    box->addStretch(1);

    // A row of a list, or what is typed in its field
    connect(m_familyList, &QListWidget::currentTextChanged, this, [this](const QString& family) {
        if (!m_updating && !family.isEmpty()) {
            setValue(QStringLiteral("fontFamily"), family);
        }
    });
    connect(m_familyField, &QLineEdit::textEdited, this, [this](const QString& typed) {
        const QString family = typed.trimmed();
        const QList<QListWidgetItem*> same = m_familyList->findItems(family, Qt::MatchFixedString);
        if (!family.isEmpty()) {
            setValue(QStringLiteral("fontFamily"), same.isEmpty() ? family : same.first()->text());
        }
    });
    const auto chooseTypeface = [this](int row) {
        if (!m_updating && row >= 0) {
            setValues({{QStringLiteral("bold"), row >= 2},
                       {QStringLiteral("italic"), row % 2 == 1}});
        }
    };
    connect(m_typefaceList, &QListWidget::currentRowChanged, this, chooseTypeface);
    connect(m_typefaceField, &QLineEdit::textEdited, this,
            [this, chooseTypeface](const QString& typed) {
                const QList<QListWidgetItem*> same =
                    m_typefaceList->findItems(typed.trimmed(), Qt::MatchFixedString);
                if (!same.isEmpty()) {
                    chooseTypeface(m_typefaceList->row(same.first()));
                }
            });
    connect(m_sizeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_updating && row >= 0) {
            setValue(QStringLiteral("fontSize"), FontDialog::offeredSizes().at(row));
        }
    });
    connect(m_sizeField, &QLineEdit::textEdited, this, [this](const QString& typed) {
        if (const std::optional<double> size = pointsOf(typed)) {
            setValue(QStringLiteral("fontSize"), *size);
        }
    });
    connect(m_underline, &QCheckBox::toggled, this,
            [this](bool on) { setValue(QStringLiteral("underline"), on); });
    return tab;
}

QWidget* StyleDialog::createIndentsTab() {
    auto* tab = new QWidget();
    auto* fields = new QVBoxLayout(tab);
    fields->setSpacing(FIELD_SPACING);

    auto* alignmentRow = new QHBoxLayout();
    //: In Polish: Wyrówna&nie
    auto* alignmentLabel = new QLabel(tr("Align&ment"), tab);
    m_alignmentBox = new QComboBox(tab);
    for (size_t i = 0; i < ALIGNMENTS.size(); ++i) {
        m_alignmentBox->addItem(alignmentText(i), QString::fromLatin1(ALIGNMENTS.at(i).name));
    }
    alignmentLabel->setBuddy(m_alignmentBox);
    markOwn(alignmentLabel, {QStringLiteral("alignment")});
    alignmentRow->addWidget(alignmentLabel);
    alignmentRow->addWidget(m_alignmentBox);
    alignmentRow->addStretch(1);
    fields->addLayout(alignmentRow);

    // The indents, as in Format > Paragraph
    //: In Polish: Wcięcia
    auto* indents = new QGroupBox(tr("Indentation"), tab);
    auto* indentGrid = new QGridLayout(indents);
    indentGrid->setHorizontalSpacing(FIELD_SPACING * 2);
    indentGrid->setVerticalSpacing(FIELD_SPACING);
    m_left = new LengthSpinBox(LengthUnit::Points, -editor::MAX_PARAGRAPH_INDENT,
                               editor::MAX_PARAGRAPH_INDENT, indents);
    m_right = new LengthSpinBox(LengthUnit::Points, -editor::MAX_PARAGRAPH_INDENT,
                                editor::MAX_PARAGRAPH_INDENT, indents);
    m_specialBox = new QComboBox(indents);
    //: The special indent: none, first line, hanging. In Polish: (brak), Pierwszy wiersz,
    //: Wysunięcie
    m_specialBox->addItems({tr("(none)"), tr("First line"), tr("Hanging")});
    m_special = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_PARAGRAPH_INDENT, indents);
    //: In Polish: &Lewe
    auto* leftLabel = new QLabel(tr("&Left"), indents);
    //: In Polish: P&rawe
    auto* rightLabel = new QLabel(tr("&Right"), indents);
    //: In Polish: &Specjalne
    auto* specialLabel = new QLabel(tr("S&pecial"), indents);
    //: A special indent "by" 1.27 cm. In Polish: &o
    auto* byLabel = new QLabel(tr("B&y"), indents);
    leftLabel->setBuddy(m_left);
    rightLabel->setBuddy(m_right);
    specialLabel->setBuddy(m_specialBox);
    byLabel->setBuddy(m_special);
    markOwn(leftLabel, {QStringLiteral("leftMargin")});
    markOwn(rightLabel, {QStringLiteral("rightMargin")});
    markOwn(specialLabel, {QStringLiteral("firstLineIndent")});
    markOwn(byLabel, {QStringLiteral("firstLineIndent")});
    indentGrid->addWidget(leftLabel, 0, 0);
    indentGrid->addWidget(m_left, 0, 1);
    indentGrid->addWidget(specialLabel, 0, 2);
    indentGrid->addWidget(m_specialBox, 0, 3);
    indentGrid->addWidget(rightLabel, 1, 0);
    indentGrid->addWidget(m_right, 1, 1);
    indentGrid->addWidget(byLabel, 1, 2);
    indentGrid->addWidget(m_special, 1, 3);
    fields->addWidget(indents);

    // The space before and after, and the line spacing in lines
    //: In Polish: Odstępy
    auto* spacing = new QGroupBox(tr("Spacing"), tab);
    auto* spacingGrid = new QGridLayout(spacing);
    spacingGrid->setHorizontalSpacing(FIELD_SPACING * 2);
    spacingGrid->setVerticalSpacing(FIELD_SPACING);
    m_before = pointsField(spacing);
    m_after = pointsField(spacing);
    m_lineSpacingBox = new QComboBox(spacing);
    //: The line spacings of a style. In Polish: Pojedyncza, 1,5 wiersza, Podwójna,
    //: Wielokrotna
    m_lineSpacingBox->addItems({tr("Single"), tr("1.5 lines"), tr("Double"), tr("Multiple")});
    m_lineSpacing = new QDoubleSpinBox(spacing);
    m_lineSpacing->setDecimals(2);
    m_lineSpacing->setRange(editor::MIN_LINE_SPACING_MULTIPLE, editor::MAX_LINE_SPACING_MULTIPLE);
    m_lineSpacing->setSingleStep(LINE_MULTIPLE_STEP);
    //: In Polish: Prz&ed
    auto* beforeLabel = new QLabel(tr("Befor&e"), spacing);
    //: In Polish: P&o
    auto* afterLabel = new QLabel(tr("&After"), spacing);
    //: In Polish: &Interlinia
    auto* lineLabel = new QLabel(tr("Li&ne spacing"), spacing);
    //: Line spacing Multiple "at" 1.15. In Polish: &co
    auto* atLabel = new QLabel(tr("A&t"), spacing);
    beforeLabel->setBuddy(m_before);
    afterLabel->setBuddy(m_after);
    lineLabel->setBuddy(m_lineSpacingBox);
    atLabel->setBuddy(m_lineSpacing);
    markOwn(beforeLabel, {QStringLiteral("spaceBefore")});
    markOwn(afterLabel, {QStringLiteral("spaceAfter")});
    markOwn(lineLabel, {QStringLiteral("lineHeight")});
    markOwn(atLabel, {QStringLiteral("lineHeight")});
    spacingGrid->addWidget(beforeLabel, 0, 0);
    spacingGrid->addWidget(m_before, 0, 1);
    spacingGrid->addWidget(lineLabel, 0, 2);
    spacingGrid->addWidget(m_lineSpacingBox, 0, 3);
    spacingGrid->addWidget(afterLabel, 1, 0);
    spacingGrid->addWidget(m_after, 1, 1);
    spacingGrid->addWidget(atLabel, 1, 2);
    spacingGrid->addWidget(m_lineSpacing, 1, 3);
    fields->addWidget(spacing);
    fields->addStretch(1);

    connect(m_alignmentBox, &QComboBox::activated, this, [this](int row) {
        setValue(QStringLiteral("alignment"), m_alignmentBox->itemData(row));
    });
    connect(m_left, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating) {
            setValue(QStringLiteral("leftMargin"), m_left->storedValue());
        }
    });
    connect(m_right, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating) {
            setValue(QStringLiteral("rightMargin"), m_right->storedValue());
        }
    });
    connect(m_specialBox, &QComboBox::activated, this, [this](int row) {
        const double by = m_special->storedValue() > SAME_NUMBER ? m_special->storedValue()
                                                                 : FIRST_INDENT_POINTS;
        setValue(QStringLiteral("firstLineIndent"),
                 row == FirstLine ? by : row == Hanging ? -by : 0.0);
    });
    connect(m_special, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating && m_specialBox->currentIndex() != NoSpecial) {
            const double by = m_special->storedValue();
            setValue(QStringLiteral("firstLineIndent"),
                     m_specialBox->currentIndex() == Hanging ? -by : by);
        }
    });
    connect(m_before, &QDoubleSpinBox::valueChanged, this, [this](double points) {
        if (!m_updating) {
            setValue(QStringLiteral("spaceBefore"), points);
        }
    });
    connect(m_after, &QDoubleSpinBox::valueChanged, this, [this](double points) {
        if (!m_updating) {
            setValue(QStringLiteral("spaceAfter"), points);
        }
    });
    connect(m_lineSpacingBox, &QComboBox::activated, this, [this](int row) {
        const double now = value(QStringLiteral("lineHeight")).toDouble();
        double multiple = FIRST_LINE_MULTIPLE;
        if (row != Multiple) {
            multiple = LINE_MULTIPLES.at(static_cast<size_t>(std::clamp(row, 0, 2)));
        } else if (lineSpacingRow(now) == Multiple) {
            multiple = now;
        }
        setValue(QStringLiteral("lineHeight"), multiple);
    });
    connect(m_lineSpacing, &QDoubleSpinBox::valueChanged, this, [this](double multiple) {
        if (!m_updating && m_lineSpacingBox->currentIndex() == Multiple) {
            setValue(QStringLiteral("lineHeight"), multiple);
        }
    });
    return tab;
}

QWidget* StyleDialog::createBreaksTab() {
    auto* tab = new QWidget();
    auto* fields = new QVBoxLayout(tab);
    fields->setSpacing(FIELD_SPACING);

    //: In Polish: Podział na strony
    auto* pagination = new QGroupBox(tr("Pagination"), tab);
    auto* boxes = new QVBoxLayout(pagination);
    boxes->setSpacing(FIELD_SPACING);
    //: In Polish: Kontrola &bękartów i wdów
    m_widowControl = new QCheckBox(tr("&Widow/Orphan control"), pagination);
    //: In Polish: Ra&zem z następnym
    m_keepWithNext = new QCheckBox(tr("Keep with ne&xt"), pagination);
    //: In Polish: Zachowaj wiersze &razem
    m_keepTogether = new QCheckBox(tr("&Keep lines together"), pagination);
    //: In Polish: Podział strony prz&ed
    m_pageBreakBefore = new QCheckBox(tr("Page break bef&ore"), pagination);
    const std::array<std::pair<QCheckBox*, const char*>, 4> flags = {
        {{m_widowControl, "widowControl"},
         {m_keepWithNext, "keepWithNext"},
         {m_keepTogether, "keepTogether"},
         {m_pageBreakBefore, "pageBreakBefore"}}};
    for (const auto& [box, name] : flags) {
        boxes->addWidget(box);
        markOwn(box, {QString::fromLatin1(name)});
        connect(box, &QCheckBox::toggled, this,
                [this, property = QString::fromLatin1(name)](bool on) { setValue(property, on); });
    }
    fields->addWidget(pagination);

    //: In Polish: Pisownia
    auto* spelling = new QGroupBox(tr("Spelling"), tab);
    auto* spellingBox = new QVBoxLayout(spelling);
    spellingBox->setSpacing(FIELD_SPACING);
    //: In Polish: S&prawdzaj pisownię
    m_parts[0].spellCheck = new QCheckBox(tr("Check s&pelling"), spelling);
    markOwn(m_parts[0].spellCheck, {QStringLiteral("spellCheck")});
    spellingBox->addWidget(m_parts[0].spellCheck);
    WrappingLabel* note = hintLabel(spelling);
    //: In Polish: Bez sprawdzania są np. akapity kodu.
    note->setText(tr("The paragraphs of code, for example, are not checked."));
    spellingBox->addWidget(note);
    fields->addWidget(spelling);
    fields->addStretch(1);
    return tab;
}

// -----------------------------------------------------------------------------
// Short
// -----------------------------------------------------------------------------

QWidget* StyleDialog::createShortPage() {
    Common& parts = m_parts[1];
    auto* page = new QWidget(this);
    auto* box = new QVBoxLayout(page);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(FIELD_SPACING);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(FIELD_SPACING * 2);
    grid->setVerticalSpacing(FIELD_SPACING);
    createNameFields(parts, page, grid, true);
    box->addLayout(grid);

    // What changes most often; the rest in Format > Font and Format > Paragraph
    //: In Polish: Formatowanie
    auto* formatting = new QGroupBox(tr("Formatting"), page);
    auto* rows = new QVBoxLayout(formatting);
    rows->setSpacing(FIELD_SPACING);
    const int iconSide = core::ArtProvider::getInstance().getIconSize(core::IconContext::Menu);
    const auto toolButton = [formatting, iconSide](const QString& iconId, const QString& text) {
        auto* button = new QToolButton(formatting);
        button->setIcon(core::ArtProvider::getInstance().getIcon(iconId, core::IconContext::Menu));
        button->setIconSize(QSize(iconSide, iconSide));
        button->setCheckable(true);
        button->setAutoRaise(false);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setToolTip(text);
        button->setAccessibleName(text);
        return button;
    };

    auto* fontRow = new QHBoxLayout();
    m_shortFont = new QComboBox(formatting);
    m_shortFont->setEditable(true);
    m_shortFont->setInsertPolicy(QComboBox::NoInsert);
    m_shortFont->addItems(systemFamilies());
    m_shortFont->setFixedWidth(SHORT_FONT_WIDTH);
    //: In Polish: Czcionka
    m_shortFont->setAccessibleName(tr("Font"));
    m_shortFont->setToolTip(tr("Font"));
    m_shortSize = new QComboBox(formatting);
    m_shortSize->setEditable(true);
    m_shortSize->setInsertPolicy(QComboBox::NoInsert);
    for (const qreal size : FontDialog::offeredSizes()) {
        m_shortSize->addItem(pointsText(size), size);
    }
    auto* sizeValidator = new QDoubleValidator(MIN_SIZE, MAX_SIZE, 1, m_shortSize);
    sizeValidator->setNotation(QDoubleValidator::StandardNotation);
    m_shortSize->setValidator(sizeValidator);
    m_shortSize->setFixedWidth(SHORT_SIZE_WIDTH);
    //: In Polish: Rozmiar
    m_shortSize->setAccessibleName(tr("Size"));
    m_shortSize->setToolTip(tr("Size"));
    if (m_draft.kind() == StyleDraft::Kind::Normal) {
        //: The note about Normal in the short window of a style. In Polish: Na ekranie tekst
        //: w stylu „%1” ma czcionkę i rozmiar z Ustawień (Edytor › Ogólne).
        const QString note = tr("On the screen the text in the style \"%1\" has the font and "
                                "size set in Settings (Editor > General).")
                                 .arg(m_draft.name());
        m_shortFont->setToolTip(note);
        m_shortSize->setToolTip(note);
    }
    //: In Polish: Pogrubienie (Ctrl+B)
    m_bold = toolButton(QStringLiteral("format.bold"), tr("Bold (Ctrl+B)"));
    //: In Polish: Kursywa (Ctrl+I)
    m_italic = toolButton(QStringLiteral("format.italic"), tr("Italic (Ctrl+I)"));
    //: Opens Format > Font for the style. In Polish: &Czcionka...
    m_fontButton = new QPushButton(tr("&Font..."), formatting);
    m_fontButton->setAutoDefault(false);
    fontRow->addWidget(m_shortFont);
    fontRow->addWidget(m_shortSize);
    fontRow->addWidget(m_bold);
    fontRow->addWidget(m_italic);
    fontRow->addStretch(1);
    fontRow->addWidget(m_fontButton);
    rows->addLayout(fontRow);

    auto* paragraphRow = new QHBoxLayout();
    m_alignments = new QButtonGroup(formatting);
    m_alignments->setExclusive(true);
    const std::array<const char*, 4> keys = {"Ctrl+L", "Ctrl+E", "Ctrl+R", "Ctrl+J"};
    for (size_t i = 0; i < ALIGNMENTS.size(); ++i) {
        const QString key =
            QKeySequence(QString::fromLatin1(keys.at(i))).toString(QKeySequence::NativeText);
        QToolButton* button = toolButton(QString::fromLatin1(ALIGNMENTS.at(i).iconId),
                                         //: An alignment with its key: Align Left (Ctrl+L). In
                                         //: Polish: %1 (%2)
                                         tr("%1 (%2)").arg(alignmentText(i), key));
        m_alignments->addButton(button, static_cast<int>(i));
        paragraphRow->addWidget(button);
    }
    auto* separator = new QFrame(formatting);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    paragraphRow->addWidget(separator);
    //: In Polish: &Interlinia
    auto* lineLabel = new QLabel(tr("Line spac&ing"), formatting);
    m_shortLineSpacing = new QComboBox(formatting);
    lineLabel->setBuddy(m_shortLineSpacing);
    paragraphRow->addWidget(lineLabel);
    paragraphRow->addWidget(m_shortLineSpacing);
    paragraphRow->addStretch(1);
    //: In Polish: Sprawdzaj pisownię
    parts.spellCheck = new QCheckBox(tr("Chec&k spelling"), formatting);
    markOwn(parts.spellCheck, {QStringLiteral("spellCheck")});
    paragraphRow->addWidget(parts.spellCheck);
    //: Opens Format > Paragraph for the style. In Polish: &Akapit...
    m_paragraphButton = new QPushButton(tr("&Paragraph..."), formatting);
    m_paragraphButton->setAutoDefault(false);
    paragraphRow->addWidget(m_paragraphButton);
    rows->addLayout(paragraphRow);
    box->addWidget(formatting);

    parts.preview = new StylePreview(page);
    parts.preview->setMinimumHeight(SHORT_PREVIEW_HEIGHT);
    box->addWidget(parts.preview, 1);

    auto* summaryRow = new QHBoxLayout();
    summaryRow->setSpacing(COLUMN_SPACING);
    summaryRow->addWidget(createSummary(parts, page), 1);
    summaryRow->addWidget(parts.reset, 0, Qt::AlignTop);
    box->addLayout(summaryRow);

    connect(m_shortFont, &QComboBox::activated, this, [this](int row) {
        setValue(QStringLiteral("fontFamily"), m_shortFont->itemText(row));
    });
    connect(m_shortFont->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        const QString family = m_shortFont->currentText().trimmed();
        if (!m_updating && !family.isEmpty()) {
            setValue(QStringLiteral("fontFamily"), family);
        }
    });
    connect(m_shortSize, &QComboBox::activated, this, [this](int row) {
        setValue(QStringLiteral("fontSize"), m_shortSize->itemData(row));
    });
    connect(m_shortSize->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        if (const std::optional<double> size = pointsOf(m_shortSize->currentText());
            size && !m_updating) {
            setValue(QStringLiteral("fontSize"), *size);
        }
    });
    connect(m_bold, &QToolButton::toggled, this,
            [this](bool on) { setValue(QStringLiteral("bold"), on); });
    connect(m_italic, &QToolButton::toggled, this,
            [this](bool on) { setValue(QStringLiteral("italic"), on); });
    connect(m_alignments, &QButtonGroup::idClicked, this, [this](int id) {
        setValue(QStringLiteral("alignment"),
                 QString::fromLatin1(ALIGNMENTS.at(static_cast<size_t>(id)).name));
    });
    connect(m_shortLineSpacing, &QComboBox::activated, this, [this](int row) {
        setValue(QStringLiteral("lineHeight"), m_shortLineSpacing->itemData(row));
    });
    connect(m_fontButton, &QPushButton::clicked, this, &StyleDialog::editFont);
    connect(m_paragraphButton, &QPushButton::clicked, this, &StyleDialog::editParagraph);
    return page;
}

void StyleDialog::editFont() {
    // The parts a style does not set as the text has them without a format of its own
    editor::CharacterFormat shown;
    shown.family = value(QStringLiteral("fontFamily")).toString();
    shown.size = value(QStringLiteral("fontSize")).toDouble();
    shown.bold = value(QStringLiteral("bold")).toBool();
    shown.italic = value(QStringLiteral("italic")).toBool();
    shown.underline = value(QStringLiteral("underline")).toBool();
    shown.strikethrough = false;
    shown.position = QTextCharFormat::AlignNormal;
    shown.capitals = QFont::MixedCase;
    shown.letterSpacing = 0.0;
    shown.color = QColor();
    shown.highlight = QColor();
    FontDialog dialog(shown, QString(), this);
    dialog.showForStyle(m_draft.name());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const editor::CharacterFormat changes = dialog.changes();
    QVariantMap values;
    if (changes.family) {
        values.insert(QStringLiteral("fontFamily"), *changes.family);
    }
    if (changes.size) {
        values.insert(QStringLiteral("fontSize"), *changes.size);
    }
    if (changes.bold) {
        values.insert(QStringLiteral("bold"), *changes.bold);
    }
    if (changes.italic) {
        values.insert(QStringLiteral("italic"), *changes.italic);
    }
    if (changes.underline) {
        values.insert(QStringLiteral("underline"), *changes.underline);
    }
    setValues(values);
}

void StyleDialog::editParagraph() {
    const auto number = [this](const char* name) {
        return value(QLatin1String(name)).toDouble();
    };
    const auto flag = [this](const char* name) { return value(QLatin1String(name)).toBool(); };
    editor::ParagraphFormat shown;
    shown.leftIndent = number("leftMargin");
    shown.rightIndent = number("rightMargin");
    shown.firstLineIndent = number("firstLineIndent");
    shown.spaceBefore = number("spaceBefore");
    shown.spaceAfter = number("spaceAfter");
    shown.lineSpacing =
        editor::LineSpacing{editor::LineSpacingRule::Multiple, number("lineHeight")};
    shown.widowControl = flag("widowControl");
    shown.keepWithNext = flag("keepWithNext");
    shown.keepTogether = flag("keepTogether");
    shown.pageBreakBefore = flag("pageBreakBefore");
    ParagraphDialog dialog(shown,
                           editor::styleAlignmentOf(value(QStringLiteral("alignment")).toString()),
                           false, this);
    dialog.showForStyle(m_draft.name());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const editor::ParagraphFormat changes = dialog.changes();
    QVariantMap values;
    const auto take = [&values](const char* name, const auto& part) {
        if (part) {
            values.insert(QLatin1String(name), *part);
        }
    };
    take("leftMargin", changes.leftIndent);
    take("rightMargin", changes.rightIndent);
    take("firstLineIndent", changes.firstLineIndent);
    take("spaceBefore", changes.spaceBefore);
    take("spaceAfter", changes.spaceAfter);
    take("widowControl", changes.widowControl);
    take("keepWithNext", changes.keepWithNext);
    take("keepTogether", changes.keepTogether);
    take("pageBreakBefore", changes.pageBreakBefore);
    if (changes.lineSpacing && changes.lineSpacing->rule == editor::LineSpacingRule::Multiple) {
        values.insert(QStringLiteral("lineHeight"), changes.lineSpacing->value);
    }
    if (const std::optional<Qt::Alignment> alignment = dialog.alignmentChange()) {
        values.insert(QStringLiteral("alignment"), editor::styleAlignmentName(*alignment));
    }
    setValues(values);
}

// -----------------------------------------------------------------------------
// The draft and its parts
// -----------------------------------------------------------------------------

void StyleDialog::setValue(const QString& name, const QVariant& value) {
    if (m_updating) {
        return;
    }
    m_draft.setValue(name, value);
    refresh();
}

void StyleDialog::setValues(const QVariantMap& values) {
    if (m_updating) {
        return;
    }
    m_draft.setValues(values);
    refresh();
}

QVariant StyleDialog::value(const QString& name) const {
    return m_draft.value(name);
}

QVariantMap StyleDialog::lookOf(const QString& id) const {
    if (id.isEmpty() || id == m_draft.id()) {
        return m_draft.look();
    }
    const editor::ParagraphStyle* style = m_draft.styles().find(id);
    QVariantMap look;
    for (const QString& name : StyleDraft::propertyNames()) {
        const QVariant own = style != nullptr ? style->properties.value(name) : QVariant();
        look.insert(name, own.isValid() ? own : StyleDraft::defaultValue(name));
    }
    return look;
}

void StyleDialog::markOwn(QWidget* label, const QStringList& properties) {
    m_ownMarks.append({label, properties});
}

void StyleDialog::findInList(QListWidget* list, const QString& text) {
    const QString typed = text.trimmed();
    if (typed.isEmpty()) {
        return;
    }
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->text().startsWith(typed, Qt::CaseInsensitive)) {
            const bool updating = m_updating;
            m_updating = true;
            list->setCurrentRow(row);
            list->scrollToItem(list->item(row), QAbstractItemView::PositionAtTop);
            m_updating = updating;
            return;
        }
    }
}

bool StyleDialog::eventFilter(QObject* watched, QEvent* event) {
    // Up and Down in a field move in its list, Page Up and Page Down by its page
    if (event->type() == QEvent::KeyPress) {
        QListWidget* list = watched == m_familyField     ? m_familyList
                            : watched == m_typefaceField ? m_typefaceList
                            : watched == m_sizeField     ? m_sizeList
                                                         : nullptr;
        auto* key = static_cast<QKeyEvent*>(event);
        if (list != nullptr && list->count() > 0 && key->modifiers() == Qt::NoModifier) {
            int step = 0;
            switch (key->key()) {
                case Qt::Key_Up:
                    step = -1;
                    break;
                case Qt::Key_Down:
                    step = 1;
                    break;
                case Qt::Key_PageUp:
                    step = -LIST_ROWS;
                    break;
                case Qt::Key_PageDown:
                    step = LIST_ROWS;
                    break;
                default:
                    break;
            }
            if (step != 0) {
                const int row = list->currentRow() < 0
                                    ? (step > 0 ? 0 : list->count() - 1)
                                    : std::clamp(list->currentRow() + step, 0, list->count() - 1);
                list->setCurrentRow(row);
                if (auto* field = qobject_cast<QLineEdit*>(watched)) {
                    field->selectAll();
                }
                return true;
            }
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

// -----------------------------------------------------------------------------
// Showing the draft
// -----------------------------------------------------------------------------

void StyleDialog::refresh() {
    const bool updating = m_updating;
    m_updating = true;
    for (const Common& parts : m_parts) {
        refreshCommon(parts);
    }
    refreshTabs();
    refreshShort();
    for (const auto& [label, properties] : m_ownMarks) {
        const bool own = std::any_of(properties.cbegin(), properties.cend(),
                                     [this](const QString& name) { return m_draft.isOwn(name); });
        if (label->font().bold() != own) {
            QFont font = label->font();
            font.setBold(own);
            label->setFont(font);
        }
    }
    acceptButton()->setEnabled(m_draft.problem().isEmpty());
    m_updating = updating;
}

void StyleDialog::refreshCommon(const Common& parts) {
    if (parts.name->text() != m_draft.name()) {
        parts.name->setText(m_draft.name());
    }
    const auto choose = [](QComboBox* box, const QString& id) {
        const int row = box->findData(id);
        if (row >= 0 && row != box->currentIndex()) {
            box->setCurrentIndex(row);
        }
    };
    choose(parts.base, m_draft.baseStyle());
    choose(parts.next, m_draft.nextStyle());

    const QString problem = m_draft.problem();
    if (parts.problem->text() != problem) {
        parts.problem->setText(problem);
        const bool shown = !problem.isEmpty();
        if (parts.problem->isVisibleTo(this) != shown) {
            parts.problem->setVisible(shown);
            if (shown && isVisible()) {
                growForContent();
            }
        }
    }
    parts.summary->setText(m_draft.summary());
    parts.reset->setEnabled(m_draft.canReset());
    parts.spellCheck->setChecked(value(QStringLiteral("spellCheck")).toBool());
    parts.preview->showStyle(lookOf(normalId()), m_draft.look(), lookOf(m_draft.nextStyle()));
}

void StyleDialog::refreshTabs() {
    // The font: its field and list
    const QString family = value(QStringLiteral("fontFamily")).toString();
    if (m_familyField->text().trimmed() != family) {
        m_familyField->setText(family);
    }
    const QList<QListWidgetItem*> familyItems =
        m_familyList->findItems(family, Qt::MatchFixedString);
    if (!familyItems.isEmpty() && m_familyList->currentItem() != familyItems.first()) {
        m_familyList->setCurrentItem(familyItems.first());
        m_familyList->scrollToItem(familyItems.first(), QAbstractItemView::PositionAtTop);
    }
    const int typeface = (value(QStringLiteral("bold")).toBool() ? 2 : 0) +
                         (value(QStringLiteral("italic")).toBool() ? 1 : 0);
    if (m_typefaceList->currentRow() != typeface) {
        m_typefaceList->setCurrentRow(typeface);
    }
    if (m_typefaceField->text() != m_typefaceList->item(typeface)->text() &&
        !m_typefaceField->hasFocus()) {
        m_typefaceField->setText(m_typefaceList->item(typeface)->text());
    }
    const double size = value(QStringLiteral("fontSize")).toDouble();
    const std::optional<double> typed = pointsOf(m_sizeField->text());
    if (!typed || !sameNumber(*typed, size)) {
        m_sizeField->setText(pointsText(size));
    }
    const QList<qreal>& sizes = FontDialog::offeredSizes();
    const auto offered = std::find_if(sizes.cbegin(), sizes.cend(),
                                      [size](qreal each) { return sameNumber(each, size); });
    const int sizeRow = offered != sizes.cend() ? static_cast<int>(offered - sizes.cbegin()) : -1;
    if (m_sizeList->currentRow() != sizeRow) {
        m_sizeList->setCurrentRow(sizeRow);
    }
    m_underline->setChecked(value(QStringLiteral("underline")).toBool());

    // The indents and the spacing
    const int alignment = m_alignmentBox->findData(value(QStringLiteral("alignment")).toString());
    m_alignmentBox->setCurrentIndex(std::max(alignment, 0));
    const auto showLength = [](LengthSpinBox* field, double points) {
        if (!sameNumber(field->storedValue(), points)) {
            field->setStoredValue(points);
        }
    };
    showLength(m_left, value(QStringLiteral("leftMargin")).toDouble());
    showLength(m_right, value(QStringLiteral("rightMargin")).toDouble());
    const double firstLine = value(QStringLiteral("firstLineIndent")).toDouble();
    const int special = firstLine > SAME_NUMBER    ? FirstLine
                        : firstLine < -SAME_NUMBER ? Hanging
                                                   : NoSpecial;
    m_specialBox->setCurrentIndex(special);
    showLength(m_special, std::abs(firstLine));
    m_special->setEnabled(special != NoSpecial);
    const auto showPoints = [](QDoubleSpinBox* field, double points) {
        if (!sameNumber(field->value(), points)) {
            field->setValue(points);
        }
    };
    showPoints(m_before, value(QStringLiteral("spaceBefore")).toDouble());
    showPoints(m_after, value(QStringLiteral("spaceAfter")).toDouble());
    const double multiple = value(QStringLiteral("lineHeight")).toDouble();
    m_lineSpacingBox->setCurrentIndex(lineSpacingRow(multiple));
    showPoints(m_lineSpacing, multiple);
    m_lineSpacing->setEnabled(lineSpacingRow(multiple) == Multiple);

    // The breaks
    m_widowControl->setChecked(value(QStringLiteral("widowControl")).toBool());
    m_keepWithNext->setChecked(value(QStringLiteral("keepWithNext")).toBool());
    m_keepTogether->setChecked(value(QStringLiteral("keepTogether")).toBool());
    m_pageBreakBefore->setChecked(value(QStringLiteral("pageBreakBefore")).toBool());

    // What a bold label means
    if (m_draft.kind() == StyleDraft::Kind::Normal) {
        //: In Polish: Styl podstawowy ustawia wszystko sam: na kartach nic nie pochodzi z
        //: innego stylu.
        m_ownHint->setText(tr("The base style sets everything itself: nothing on the tabs comes "
                              "from another style."));
    } else if (m_draft.baseStyle().isEmpty()) {
        //: In Polish: Na kartach pogrubiona etykieta oznacza własne ustawienie stylu; pozostałe
        //: pola pokazują ustawienia programu.
        m_ownHint->setText(tr("On the tabs a bold label marks a setting of the style's own; the "
                              "other fields show the program's settings."));
    } else {
        const editor::ParagraphStyle* base = m_draft.styles().find(m_draft.baseStyle());
        //: In Polish: Na kartach pogrubiona etykieta oznacza własne ustawienie stylu; pozostałe
        //: pola pokazują wartości stylu „%1”. Wpisanie wartości stylu bazowego usuwa własne
        //: ustawienie.
        m_ownHint->setText(tr("On the tabs a bold label marks a setting of the style's own; the "
                              "other fields show the values of the style \"%1\". A value of the "
                              "base style typed in a field is no longer the style's own.")
                               .arg(base != nullptr ? base->name : m_draft.baseStyle()));
    }
}

void StyleDialog::refreshShort() {
    const QString family = value(QStringLiteral("fontFamily")).toString();
    if (m_shortFont->currentText() != family) {
        const int row = m_shortFont->findText(family, Qt::MatchFixedString);
        if (row >= 0) {
            m_shortFont->setCurrentIndex(row);
        } else {
            m_shortFont->setEditText(family);
        }
    }
    const double size = value(QStringLiteral("fontSize")).toDouble();
    const std::optional<double> shownSize = pointsOf(m_shortSize->currentText());
    if (!shownSize || !sameNumber(*shownSize, size)) {
        const int row = m_shortSize->findText(pointsText(size));
        if (row >= 0) {
            m_shortSize->setCurrentIndex(row);
        } else {
            m_shortSize->setEditText(pointsText(size));
        }
    }
    m_bold->setChecked(value(QStringLiteral("bold")).toBool());
    m_italic->setChecked(value(QStringLiteral("italic")).toBool());
    if (QToolButton* button = alignmentButton(
            editor::styleAlignmentOf(value(QStringLiteral("alignment")).toString()))) {
        button->setChecked(true);
    }

    // Single, 1.5 lines, double, and the multiple of the style when it is another
    const double multiple = value(QStringLiteral("lineHeight")).toDouble();
    const bool other = lineSpacingRow(multiple) == Multiple;
    const int wanted = static_cast<int>(LINE_MULTIPLES.size()) + (other ? 1 : 0);
    if (m_shortLineSpacing->count() != wanted ||
        (other && !sameNumber(m_shortLineSpacing->itemData(Multiple).toDouble(), multiple))) {
        m_shortLineSpacing->clear();
        m_shortLineSpacing->addItem(tr("Single"), LINE_MULTIPLES.at(0));
        m_shortLineSpacing->addItem(tr("1.5 lines"), LINE_MULTIPLES.at(1));
        m_shortLineSpacing->addItem(tr("Double"), LINE_MULTIPLES.at(2));
        if (other) {
            //: Line spacing of so many lines. In Polish: Wielokrotna %1
            m_shortLineSpacing->addItem(tr("Multiple %1").arg(QLocale().toString(multiple, 'g', 3)),
                                        multiple);
        }
    }
    m_shortLineSpacing->setCurrentIndex(lineSpacingRow(multiple));
}

}  // namespace kalahari::gui::dialogs
