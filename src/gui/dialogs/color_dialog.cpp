/// @file color_dialog.cpp
/// @brief The program's own window for choosing a color

#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/core/settings_manager.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOptionSlider>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int SWATCH_SIZE = 20;            ///< Side of a swatch
constexpr int SWATCH_CELL = 26;            ///< Side of a swatch's cell, with the room around it
constexpr int PREVIEW_HEIGHT = 56;
constexpr int PREVIEW_WIDTH = 84;
constexpr int SPACING = 12;
constexpr int LABEL_SPACING = 4;
constexpr int SLIDER_MIN_WIDTH = 150;
constexpr int SLIDER_PAGE_STEP = 10;
constexpr int TRACK_HEIGHT = 10;           ///< Of a slider's track that shows its colors
constexpr int MAX_HUE = 359;
constexpr int MAX_PERCENT = 100;
constexpr int HALF_PERCENT = 50;
constexpr const char* RECENT_COLORS_KEY = "ui.recentColors";
constexpr const char* MY_COLORS_KEY = "ui.myColors";
constexpr const char* PALETTE_KEY = "ui.colorPalette";
constexpr const char* VALUES_SHOWN_KEY = "ui.colorValuesShown";

/// @brief The lightness of the shades of the color, from light to dark, in percent
constexpr std::array<int, ColorDialog::SHADES> SHADE_LIGHTNESS = {92, 83, 74, 65, 56, 47, 38, 29, 20};

/// @brief A shade this close to the color's lightness, in percent, is the color's own
constexpr int SAME_SHADE = 4;

/// @brief The hue slider's track: every hue, full and at its middle lightness
constexpr int HUE_TRACK_SATURATION = 75;
constexpr int HUE_TRACK_STEP = 60;
constexpr int FULL_TURN = 360;

/// @brief Text color readable on a background color: black or white on any swatch, which
///        no theme color can promise
QColor readableTextOn(const QColor& background)
{
    constexpr int LIGHT_BACKGROUND = 140;
    const int luminance = qGray(background.rgb());
    return luminance > LIGHT_BACKGROUND ? QColor(Qt::black) : QColor(Qt::white);
}

/// @brief Style sheet of a preview area in a color
QString previewStyle(const QColor& color, const QColor& border)
{
    return QStringLiteral("background-color: %1; color: %2; border: 1px solid %3; padding: 4px;")
        .arg(color.name(), readableTextOn(color).name(), border.name());
}

/// @brief The colors kept in the settings under @p key, at most @p most
QStringList keptColors(const char* key, int most)
{
    const auto names =
        core::SettingsManager::getInstance().get<std::vector<std::string>>(key, {});
    QStringList colors;
    for (const std::string& name : names) {
        const QColor color(QString::fromStdString(name));
        if (color.isValid() && colors.size() < most) {
            colors.append(color.name());
        }
    }
    return colors;
}

/// @brief Keep @p colors in the settings under @p key
void keepColors(const char* key, const QStringList& colors)
{
    std::vector<std::string> names;
    names.reserve(static_cast<size_t>(colors.size()));
    for (const QString& color : colors) {
        names.push_back(color.toStdString());
    }
    core::SettingsManager::getInstance().set<std::vector<std::string>>(key, names);
}

/// @brief Draws a swatch in its own color. An icon would be tinted when selected, which
///        would show the writer a color other than the one they pick.
class SwatchDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const QRect swatch(option.rect.center().x() - SWATCH_SIZE / 2 + 1,
                           option.rect.center().y() - SWATCH_SIZE / 2 + 1,
                           SWATCH_SIZE, SWATCH_SIZE);
        painter->save();
        painter->fillRect(swatch, index.data(Qt::UserRole).value<QColor>());
        painter->setPen(option.palette.color(QPalette::Mid));
        painter->drawRect(swatch.adjusted(0, 0, -1, -1));

        // The selected swatch gets a frame around it, the current one a thinner frame
        // while the list has the focus
        const QColor highlight = option.palette.color(QPalette::Highlight);
        if (option.state.testFlag(QStyle::State_Selected)) {
            painter->setPen(QPen(highlight, 2));
            painter->drawRect(swatch.adjusted(-2, -2, 1, 1));
        } else if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setPen(QPen(highlight, 1, Qt::DotLine));
            painter->drawRect(swatch.adjusted(-2, -2, 1, 1));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& /*option*/,
                   const QModelIndex& /*index*/) const override
    {
        return {SWATCH_CELL, SWATCH_CELL};
    }
};

/// @brief A slider whose track shows the colors it gives, from its minimum to its maximum
class GradientSlider : public QSlider {
public:
    explicit GradientSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent)
    {
        setMinimumWidth(SLIDER_MIN_WIDTH);
        setPageStep(SLIDER_PAGE_STEP);
    }

    /// @brief The colors of the track, from the minimum (0) to the maximum (1)
    void setColors(const QGradientStops& stops)
    {
        m_stops = stops;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        QStyleOptionSlider option;
        initStyleOption(&option);

        // The track runs from the middle of the handle at the minimum to its middle at the
        // maximum, so the handle stands on the color it gives; its frame shows the focus
        const int handle = style()->pixelMetric(QStyle::PM_SliderLength, &option, this);
        const QRect track(handle / 2, (height() - TRACK_HEIGHT) / 2, width() - handle,
                          TRACK_HEIGHT);
        QLinearGradient gradient(track.topLeft(), track.topRight());
        gradient.setStops(m_stops);
        painter.fillRect(track, gradient);
        painter.setPen(palette().color(hasFocus() ? QPalette::Highlight : QPalette::Mid));
        painter.drawRect(track.adjusted(0, 0, -1, -1));

        option.subControls = QStyle::SC_SliderHandle;
        style()->drawComplexControl(QStyle::CC_Slider, &option, &painter, this);
    }

private:
    QGradientStops m_stops;
};

/// @brief The gradient slider @p slider is, as made by createAdjustments()
GradientSlider* gradientSlider(QSlider* slider)
{
    return static_cast<GradientSlider*>(slider);
}

} // anonymous namespace

ColorDialog::ColorDialog(const QColor& initial, const QString& description, QWidget* parent)
    : KalahariDialog(parent)
    , m_palettes(colorPalettes())
    , m_initial(QColor::fromRgb(initial.isValid() ? initial.rgb() : QColor(Qt::black).rgb()))
    , m_color(m_initial)
{
    setHeading(tr("Select Color"), description);
    setHeadingIcon(QStringLiteral("common.palette"));
    setCompactHeading(true);
    setAcceptText(tr("Select"));

    auto* columns = new QHBoxLayout();
    columns->setSpacing(SPACING * 2);
    columns->addLayout(createSwatches());
    columns->addLayout(createAdjustments(), 1);
    contentLayout()->addLayout(columns);

    // The palette chosen last, and the numeric values shown or not as last time
    auto& settings = core::SettingsManager::getInstance();
    const QString paletteId =
        QString::fromStdString(settings.get<std::string>(PALETTE_KEY, "basic"));
    int shown = 0;
    for (int i = 0; i < m_palettes.size(); ++i) {
        if (m_palettes.at(i).id == paletteId) {
            shown = i;
        }
    }
    m_paletteBox->setCurrentIndex(shown);
    connect(m_paletteBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        showPalette(index);
        if (index >= 0 && index < m_palettes.size()) {
            core::SettingsManager::getInstance().set<std::string>(
                PALETTE_KEY, m_palettes.at(index).id.toStdString());
        }
    });
    showValues(settings.get<bool>(VALUES_SHOWN_KEY, false));
    connect(m_valuesButton, &QToolButton::toggled, this, [this](bool shown) {
        showValues(shown);
        core::SettingsManager::getInstance().set<bool>(VALUES_SHOWN_KEY, shown);
    });

    setColor(m_initial);
    showPalette(shown);
    m_paletteList->setFocus();
}

QLayout* ColorDialog::createSwatches()
{
    auto* swatches = new QVBoxLayout();
    swatches->setSpacing(LABEL_SPACING);

    // The palettes: the list of them, what the one shown has, and its swatches
    auto* paletteRow = new QHBoxLayout();
    paletteRow->setSpacing(LABEL_SPACING * 2);
    auto* paletteLabel = new QLabel(tr("&Palette:"), this);
    m_paletteBox = new QComboBox(this);
    for (const ColorPalette& palette : m_palettes) {
        m_paletteBox->addItem(palette.name, palette.id);
    }
    paletteLabel->setBuddy(m_paletteBox);
    paletteRow->addWidget(paletteLabel);
    paletteRow->addWidget(m_paletteBox, 1);
    swatches->addLayout(paletteRow);
    m_paletteDescription = new QLabel(this);
    m_paletteDescription->setForegroundRole(QPalette::PlaceholderText);
    m_paletteDescription->setWordWrap(true);
    swatches->addWidget(m_paletteDescription);
    m_paletteList = createSwatchList(tr("Palette"), PALETTE_FAMILIES);
    m_paletteList->setFixedHeight(PALETTE_SHADES * SWATCH_CELL + 2 * m_paletteList->frameWidth());
    swatches->addWidget(m_paletteList);
    swatches->addSpacing(SPACING);

    // The colors selected last
    auto* recentLabel = new QLabel(tr("Rec&ent colors"), this);
    m_recentList = createSwatchList(tr("Recent colors"), MAX_RECENT_COLORS);
    m_recentList->setFixedHeight(SWATCH_CELL + 2 * m_recentList->frameWidth());
    fillSwatches(m_recentList, recentColors());
    recentLabel->setBuddy(m_recentList);
    m_noRecentLabel = new QLabel(tr("The colors you select appear here."), this);
    m_noRecentLabel->setForegroundRole(QPalette::PlaceholderText);
    m_recentList->setVisible(m_recentList->count() > 0);
    m_noRecentLabel->setVisible(m_recentList->count() == 0);
    swatches->addWidget(recentLabel);
    swatches->addWidget(m_recentList);
    swatches->addWidget(m_noRecentLabel);
    swatches->addSpacing(SPACING);

    // My colors, with the buttons that add and remove them
    auto* myRow = new QHBoxLayout();
    myRow->setSpacing(LABEL_SPACING * 2);
    auto* myLabel = new QLabel(tr("&My colors"), this);
    m_addButton = new QPushButton(tr("&Add this color"), this);
    m_addButton->setAutoDefault(false);
    m_removeButton = new QPushButton(tr("Rem&ove"), this);
    m_removeButton->setAutoDefault(false);
    myRow->addWidget(myLabel);
    myRow->addStretch(1);
    myRow->addWidget(m_addButton);
    myRow->addWidget(m_removeButton);
    swatches->addLayout(myRow);
    m_myList = createSwatchList(tr("My colors"), MAX_MY_COLORS);
    m_myList->setFixedHeight(SWATCH_CELL + 2 * m_myList->frameWidth());
    fillSwatches(m_myList, myColors());
    myLabel->setBuddy(m_myList);
    swatches->addWidget(m_myList);
    m_myColorsNote = new QLabel(this);
    m_myColorsNote->setForegroundRole(QPalette::PlaceholderText);
    m_myColorsNote->setWordWrap(true);
    swatches->addWidget(m_myColorsNote);
    connect(m_addButton, &QPushButton::clicked, this, &ColorDialog::addMyColor);
    connect(m_removeButton, &QPushButton::clicked, this, &ColorDialog::removeMyColor);
    // Delete removes the selected one, written out as every key of the program is
    auto* removeKey = new QShortcut(QKeySequence(Qt::Key_Delete), m_myList);
    removeKey->setContext(Qt::WidgetShortcut);
    connect(removeKey, &QShortcut::activated, this, &ColorDialog::removeMyColor);
    m_myColorsNote->setText(m_myList->count() == 0 ? tr("Add the colors you often use here.")
                                                   : QString());
    m_myColorsNote->setVisible(!m_myColorsNote->text().isEmpty());

    swatches->addStretch(1);
    return swatches;
}

QLayout* ColorDialog::createAdjustments()
{
    auto* adjustments = new QVBoxLayout();
    adjustments->setSpacing(LABEL_SPACING);

    // The previous color, which pressing brings back, beside the new one
    adjustments->addWidget(new QLabel(tr("Preview"), this));
    auto* preview = new QHBoxLayout();
    preview->setSpacing(0);
    m_previousButton = new QPushButton(tr("Pre&vious"), this);
    m_previousButton->setFixedSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    m_previousButton->setToolTip(tr("Bring back the previous color"));
    m_previousButton->setAutoDefault(false);
    connect(m_previousButton, &QPushButton::clicked, this, [this]() { setColor(m_initial); });
    m_newLabel = new QLabel(tr("New"), this);
    m_newLabel->setFixedSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    m_newLabel->setAlignment(Qt::AlignCenter);
    preview->addWidget(m_previousButton);
    preview->addWidget(m_newLabel);
    preview->addStretch(1);
    adjustments->addLayout(preview);

    // The name of the color; for a color of no palette, the closest named one. A line each
    // also when empty, so nothing under them moves.
    m_nameLabel = new QLabel(this);
    QFont bold = m_nameLabel->font();
    bold.setBold(true);
    m_nameLabel->setFont(bold);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setMinimumHeight(m_nameLabel->fontMetrics().lineSpacing());
    m_closestLabel = new QLabel(this);
    m_closestLabel->setForegroundRole(QPalette::PlaceholderText);
    m_closestLabel->setWordWrap(true);
    m_closestLabel->setMinimumHeight(m_closestLabel->fontMetrics().lineSpacing());
    adjustments->addWidget(m_nameLabel);
    adjustments->addWidget(m_closestLabel);
    adjustments->addSpacing(SPACING);

    // Lighter and darker shades of the color
    auto* shadesLabel = new QLabel(tr("&Shades, from light to dark"), this);
    m_shadesList = createSwatchList(tr("Shades"), SHADES);
    m_shadesList->setFixedHeight(SWATCH_CELL + 2 * m_shadesList->frameWidth());
    for (int i = 0; i < SHADES; ++i) {
        new QListWidgetItem(m_shadesList);
    }
    shadesLabel->setBuddy(m_shadesList);
    adjustments->addWidget(shadesLabel);
    adjustments->addWidget(m_shadesList);
    adjustments->addSpacing(SPACING);

    // Hue, intensity and lightness on sliders whose tracks show the colors they give
    auto* sliders = new QGridLayout();
    sliders->setHorizontalSpacing(SPACING);
    sliders->setVerticalSpacing(LABEL_SPACING);
    const auto addSlider = [this, sliders](const QString& label, const QString& tip,
                                           int maximum, int row) -> QSlider* {
        auto* text = new QLabel(label, this);
        auto* slider = new GradientSlider(this);
        slider->setRange(0, maximum);
        slider->setToolTip(tip);
        text->setToolTip(tip);
        text->setBuddy(slider);
        sliders->addWidget(text, row, 0);
        sliders->addWidget(slider, row, 1);
        connect(slider, &QSlider::valueChanged, this, &ColorDialog::onSliderMoved);
        return slider;
    };
    m_hueSlider = addSlider(tr("H&ue"), tr("The color itself: red, yellow, green, blue..."),
                            MAX_HUE, 0);
    m_intensitySlider = addSlider(tr("&Intensity"), tr("From grey to the full color"),
                                  MAX_PERCENT, 1);
    m_lightnessSlider = addSlider(tr("&Lightness"), tr("From black to white"), MAX_PERCENT, 2);
    QGradientStops hues;
    for (int hue = 0; hue <= FULL_TURN; hue += HUE_TRACK_STEP) {
        hues.append(QGradientStop(static_cast<qreal>(hue) / FULL_TURN,
                                  colorFromHsl(hue % FULL_TURN, HUE_TRACK_SATURATION,
                                               HALF_PERCENT)));
    }
    gradientSlider(m_hueSlider)->setColors(hues);
    adjustments->addLayout(sliders);
    adjustments->addSpacing(SPACING);

    // HEX, red, green and blue, for those who know them; hidden unless shown last time
    m_valuesButton = new QToolButton(this);
    m_valuesButton->setText(tr("&Numeric values (HEX and RGB)"));
    m_valuesButton->setCheckable(true);
    m_valuesButton->setAutoRaise(true);
    m_valuesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_valuesButton->setArrowType(Qt::RightArrow);
    adjustments->addWidget(m_valuesButton);

    m_valuesArea = new QWidget(this);
    auto* values = new QGridLayout(m_valuesArea);
    values->setContentsMargins(0, 0, 0, 0);
    values->setHorizontalSpacing(LABEL_SPACING * 2);
    values->setVerticalSpacing(LABEL_SPACING);
    auto* hexLabel = new QLabel(tr("&HEX"), m_valuesArea);
    m_hexEdit = new QLineEdit(m_valuesArea);
    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{0,6}")), m_hexEdit));
    m_hexEdit->setPlaceholderText(QStringLiteral("#RRGGBB"));
    hexLabel->setBuddy(m_hexEdit);
    connect(m_hexEdit, &QLineEdit::textEdited, this, &ColorDialog::onHexEdited);
    // Leaving the field shows the color in full, "#12AB34" for "12ab34"
    connect(m_hexEdit, &QLineEdit::editingFinished, this,
            [this]() { m_hexEdit->setText(m_color.name().toUpper()); });
    values->addWidget(hexLabel, 0, 0);
    values->addWidget(m_hexEdit, 0, 1, 1, 2);
    const std::array<QString, 3> rgbLabels = {tr("&Red"), tr("&Green"), tr("&Blue")};
    std::array<QSpinBox**, 3> rgbFields = {&m_redSpin, &m_greenSpin, &m_blueSpin};
    for (int i = 0; i < 3; ++i) {
        auto* label = new QLabel(rgbLabels.at(static_cast<size_t>(i)), m_valuesArea);
        auto* spin = new QSpinBox(m_valuesArea);
        spin->setRange(0, 255);
        label->setBuddy(spin);
        connect(spin, &QSpinBox::valueChanged, this, &ColorDialog::onRgbChanged);
        values->addWidget(label, 1, i);
        values->addWidget(spin, 2, i);
        *rgbFields.at(static_cast<size_t>(i)) = spin;
    }
    adjustments->addWidget(m_valuesArea);
    adjustments->addStretch(1);
    return adjustments;
}

QStringList ColorDialog::recentColors()
{
    return keptColors(RECENT_COLORS_KEY, MAX_RECENT_COLORS);
}

void ColorDialog::addRecentColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    QStringList colors = recentColors();
    colors.removeAll(color.name());
    colors.prepend(color.name());
    keepColors(RECENT_COLORS_KEY, colors.mid(0, MAX_RECENT_COLORS));
}

QStringList ColorDialog::myColors()
{
    return keptColors(MY_COLORS_KEY, MAX_MY_COLORS);
}

std::optional<QColor> ColorDialog::getColor(const QColor& initial, QWidget* parent,
                                            const QString& description)
{
    ColorDialog dialog(initial, description, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.color();
}

void ColorDialog::setColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_color = QColor::fromRgb(color.rgb());
    m_color.setAlpha(255);
    // A grey has no hue: the hue slider keeps the one it shows
    const int hue = m_color.hslHue();
    if (hue >= 0) {
        m_hue = hue;
    }
    m_saturation = qRound(m_color.hslSaturationF() * MAX_PERCENT);
    m_lightness = qRound(m_color.lightnessF() * MAX_PERCENT);
    updateViews();
}

void ColorDialog::accept()
{
    addRecentColor(m_color);
    KalahariDialog::accept();
}

QListWidget* ColorDialog::createSwatchList(const QString& accessibleName, int columns)
{
    auto* list = new QListWidget(this);
    list->setAccessibleName(accessibleName);
    list->setViewMode(QListView::IconMode);
    list->setMovement(QListView::Static);
    list->setFlow(QListView::LeftToRight);
    list->setWrapping(true);
    list->setUniformItemSizes(true);
    list->setItemDelegate(new SwatchDelegate(list));
    list->setGridSize(QSize(SWATCH_CELL, SWATCH_CELL));
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // The cells of a row: the view needs a little room beyond them, or it wraps one sooner
    list->setFixedWidth(columns * SWATCH_CELL + 2 * list->frameWidth() + SWATCH_CELL / 2);

    // The selected swatch is the color: arrows and clicks select, Space too
    connect(list, &QListWidget::itemSelectionChanged, this, [this, list]() {
        if (!m_updating) {
            onSwatchChosen(list);
        }
    });
    return list;
}

void ColorDialog::fillSwatches(QListWidget* list, const QStringList& colors)
{
    const bool updating = m_updating;
    m_updating = true;
    list->clear();
    for (const QString& name : colors) {
        const QColor color(name);
        auto* item = new QListWidgetItem(list);
        const QString colorName = nameOf(color);
        item->setData(Qt::UserRole, color);
        item->setToolTip(colorName + QLatin1Char('\n') + color.name().toUpper());
        item->setData(Qt::AccessibleTextRole, colorName);
    }
    m_updating = updating;
}

void ColorDialog::showPalette(int index)
{
    if (index < 0 || index >= m_palettes.size()) {
        return;
    }
    const ColorPalette& palette = m_palettes.at(index);
    m_paletteDescription->setText(palette.description);
    const bool updating = m_updating;
    m_updating = true;
    m_paletteList->clear();
    for (const NamedColor& named : palette.colors) {
        auto* item = new QListWidgetItem(m_paletteList);
        item->setData(Qt::UserRole, named.color);
        item->setToolTip(named.name + QLatin1Char('\n') + named.color.name().toUpper());
        item->setData(Qt::AccessibleTextRole, named.name);
    }
    // The recent colors and My colors are named after the palette shown first
    for (QListWidget* list : {m_recentList, m_myList}) {
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            const QColor color = item->data(Qt::UserRole).value<QColor>();
            const QString colorName = nameOf(color);
            item->setToolTip(colorName + QLatin1Char('\n') + color.name().toUpper());
            item->setData(Qt::AccessibleTextRole, colorName);
        }
    }
    m_updating = updating;
    updateViews();
}

void ColorDialog::onSwatchChosen(QListWidget* list)
{
    const QList<QListWidgetItem*> selected = list->selectedItems();
    if (selected.isEmpty()) {
        m_removeButton->setEnabled(!m_myList->selectedItems().isEmpty());
        return;
    }
    if (list == m_shadesList) {
        // A shade keeps the hue and the intensity: only the lightness changes
        const int row = m_shadesList->row(selected.first());
        m_lightness = SHADE_LIGHTNESS.at(static_cast<size_t>(row));
        m_color = colorFromHsl(m_hue, m_saturation, m_lightness);
        updateViews();
        return;
    }
    setColor(selected.first()->data(Qt::UserRole).value<QColor>());
}

void ColorDialog::onSliderMoved()
{
    if (m_updating) {
        return;
    }
    m_hue = m_hueSlider->value();
    m_saturation = m_intensitySlider->value();
    m_lightness = m_lightnessSlider->value();
    m_color = colorFromHsl(m_hue, m_saturation, m_lightness);
    updateViews();
}

void ColorDialog::onHexEdited(const QString& text)
{
    QString hex = text.trimmed();
    if (!hex.startsWith(QLatin1Char('#'))) {
        hex.prepend(QLatin1Char('#'));
    }
    // A complete value chooses the color; an incomplete one cannot be selected yet
    const bool complete = hex.size() == 7 && QColor::isValidColorName(hex);
    acceptButton()->setEnabled(complete);
    if (complete) {
        m_typingHex = true;
        setColor(QColor(hex));
        m_typingHex = false;
    }
}

void ColorDialog::onRgbChanged()
{
    if (m_updating) {
        return;
    }
    setColor(QColor(m_redSpin->value(), m_greenSpin->value(), m_blueSpin->value()));
}

void ColorDialog::addMyColor()
{
    QStringList colors = myColors();
    QString note;
    if (colors.contains(m_color.name())) {
        note = tr("This color is already among My colors.");
    } else if (colors.size() >= MAX_MY_COLORS) {
        note = tr("My colors are full: remove one to add another.");
    } else {
        colors.append(m_color.name());
        keepColors(MY_COLORS_KEY, colors);
        fillSwatches(m_myList, colors);
    }
    m_myColorsNote->setText(note);
    m_myColorsNote->setVisible(!note.isEmpty());
    updateViews();
}

void ColorDialog::removeMyColor()
{
    const QList<QListWidgetItem*> selected = m_myList->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    QStringList colors = myColors();
    colors.removeAll(selected.first()->data(Qt::UserRole).value<QColor>().name());
    keepColors(MY_COLORS_KEY, colors);
    fillSwatches(m_myList, colors);
    m_myColorsNote->setText(colors.isEmpty() ? tr("Add the colors you often use here.")
                                             : QString());
    m_myColorsNote->setVisible(!m_myColorsNote->text().isEmpty());
    updateViews();
}

void ColorDialog::showValues(bool shown)
{
    m_valuesArea->setVisible(shown);
    m_valuesButton->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
    {
        const QSignalBlocker blocker(m_valuesButton);
        m_valuesButton->setChecked(shown);
    }
    // Once the layouts, the content's first and then the window's, have taken in the shown
    // or hidden values
    QTimer::singleShot(0, this, [this]() {
        if (isVisible()) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            fitToScreen(sizeHint());
        }
    });
}

QString ColorDialog::nameOf(const QColor& color) const
{
    // The palette shown first, as the same color may have a name in another one too
    const int shown = m_paletteBox != nullptr ? m_paletteBox->currentIndex() : -1;
    if (shown >= 0 && shown < m_palettes.size()) {
        const QString name = paletteColorName({m_palettes.at(shown)}, color);
        if (!name.isEmpty()) {
            return name;
        }
    }
    const QString name = paletteColorName(m_palettes, color);
    return name.isEmpty() ? tr("Own color") : name;
}

void ColorDialog::updateViews()
{
    m_updating = true;

    const QColor border = palette().color(QPalette::Mid);
    m_previousButton->setStyleSheet(previewStyle(m_initial, border));
    m_newLabel->setStyleSheet(previewStyle(m_color, border));
    m_newLabel->setAccessibleName(tr("New color %1").arg(m_color.name().toUpper()));

    // Its name; for a color of no palette, the closest named one
    const QString name = nameOf(m_color);
    const bool own = paletteColorName(m_palettes, m_color).isEmpty();
    m_nameLabel->setText(name);
    m_closestLabel->setText(
        own ? tr("Closest in the palettes: %1").arg(closestPaletteColorName(m_palettes, m_color))
            : QString());

    // The shades of the color, from light to dark; the one of its lightness is marked
    int nearest = 0;
    for (int i = 0; i < SHADES; ++i) {
        const int lightness = SHADE_LIGHTNESS.at(static_cast<size_t>(i));
        if (std::abs(lightness - m_lightness) <
            std::abs(SHADE_LIGHTNESS.at(static_cast<size_t>(nearest)) - m_lightness)) {
            nearest = i;
        }
    }
    for (int i = 0; i < SHADES; ++i) {
        const QColor shade =
            colorFromHsl(m_hue, m_saturation, SHADE_LIGHTNESS.at(static_cast<size_t>(i)));
        QListWidgetItem* item = m_shadesList->item(i);
        const QString where = i < nearest ? tr("Lighter") : i > nearest ? tr("Darker")
                                                                        : tr("This lightness");
        item->setData(Qt::UserRole, shade);
        item->setToolTip(where + QLatin1Char('\n') + shade.name().toUpper());
        item->setData(Qt::AccessibleTextRole, where);
    }
    const bool ownShade =
        std::abs(SHADE_LIGHTNESS.at(static_cast<size_t>(nearest)) - m_lightness) <= SAME_SHADE;
    if (ownShade) {
        m_shadesList->setCurrentRow(nearest);
    } else {
        m_shadesList->clearSelection();
        m_shadesList->selectionModel()->setCurrentIndex(m_shadesList->model()->index(nearest, 0),
                                                        QItemSelectionModel::NoUpdate);
    }

    // The sliders, and the colors their tracks give from here
    m_hueSlider->setValue(m_hue);
    m_intensitySlider->setValue(m_saturation);
    m_lightnessSlider->setValue(m_lightness);
    gradientSlider(m_intensitySlider)
        ->setColors({{0.0, colorFromHsl(m_hue, 0, m_lightness)},
                     {1.0, colorFromHsl(m_hue, MAX_PERCENT, m_lightness)}});
    gradientSlider(m_lightnessSlider)
        ->setColors({{0.0, colorFromHsl(m_hue, m_saturation, 0)},
                     {0.5, colorFromHsl(m_hue, m_saturation, HALF_PERCENT)},
                     {1.0, colorFromHsl(m_hue, m_saturation, MAX_PERCENT)}});

    if (!m_typingHex) {
        m_hexEdit->setText(m_color.name().toUpper());
    }
    m_redSpin->setValue(m_color.red());
    m_greenSpin->setValue(m_color.green());
    m_blueSpin->setValue(m_color.blue());
    acceptButton()->setEnabled(true);

    // Select the swatch of the color, if a list has it. Otherwise nothing is selected,
    // but the list keeps a current item, so getting the focus does not pick a color.
    for (QListWidget* list : {m_paletteList, m_recentList, m_myList}) {
        QListWidgetItem* match = nullptr;
        for (int i = 0; i < list->count() && !match; ++i) {
            if (list->item(i)->data(Qt::UserRole).value<QColor>() == m_color) {
                match = list->item(i);
            }
        }
        if (match) {
            list->setCurrentItem(match);
        } else {
            list->clearSelection();
            if (!list->currentItem() && list->count() > 0) {
                list->selectionModel()->setCurrentIndex(list->model()->index(0, 0),
                                                        QItemSelectionModel::NoUpdate);
            }
        }
    }
    m_removeButton->setEnabled(!m_myList->selectedItems().isEmpty());

    m_updating = false;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
