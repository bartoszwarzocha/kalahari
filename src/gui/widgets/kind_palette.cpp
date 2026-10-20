/// @file kind_palette.cpp
/// @brief The palette: the kinds of elements of all book types, which the writer chooses for a
/// user project

#include "kalahari/gui/widgets/kind_palette.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFontInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace kalahari {
namespace gui {

namespace {

constexpr qreal TILE_WIDTH_EMS = 8.3;     ///< Room for a name of two short words in a line
constexpr qreal TILE_HEIGHT_EMS = 7.0;    ///< At least the icon, two lines and "Create now"
constexpr qreal SMALL_FONT_SCALE = 0.88;  ///< The type and "Create now" under the name
constexpr int TILE_SPACING = 6;           ///< Between the tiles and their rows
constexpr int TILE_MARGIN = 3;            ///< Around the content of a tile
constexpr int TILE_LINE_SPACING = 2;      ///< Between the lines of a tile
constexpr int TOP_SPACING = 4;            ///< Between the box and the icon of a tile
constexpr int CORNER_RADIUS = 3;          ///< Of the background of a checked tile
constexpr qreal CHECKED_FILL = 0.1;       ///< The highlight in the background of a checked tile
constexpr qreal CHECKED_BORDER = 0.33;    ///< The highlight in the border of a checked tile
constexpr int GROUP_SPACING = 6;          ///< Between a heading, its line and its tiles
constexpr int PALETTE_MARGIN = 10;        ///< At the sides of the palette

/// The places of the groups, in the order of the palette
constexpr std::array<core::BookPlace, 4> GROUP_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                      core::BookPlace::Back,
                                                      core::BookPlace::Workshop};

} // anonymous namespace

/// @brief One kind in one group of the palette: its box, icon, name, type and "Create now"
class KindTile : public QWidget {
public:
    KindTile(const core::KindRef& kind, core::BookPlace place, QWidget* parent)
        : QWidget(parent)
        , m_kind(kind)
        , m_place(place)
    {
        const QString language = programLanguage();
        const QString name = kind.kind->name.text(language);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(TILE_MARGIN, TILE_MARGIN, TILE_MARGIN, TILE_MARGIN);
        layout->setSpacing(TILE_LINE_SPACING);

        // The box and the icon, side by side above the name
        auto* top = new QHBoxLayout();
        top->setSpacing(TOP_SPACING);
        m_check = new QCheckBox(this);
        m_check->setAccessibleName(name);
        m_check->setAccessibleDescription(kind.package->name.text(language));
        top->addWidget(m_check, 0, Qt::AlignTop);
        m_icon = new QLabel(this);
        top->addWidget(m_icon);
        layout->addLayout(top);
        layout->setAlignment(top, Qt::AlignHCenter);

        m_name = new QLabel(name, this);
        m_name->setTextFormat(Qt::PlainText);
        m_name->setWordWrap(true);
        m_name->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        layout->addWidget(m_name);

        // The type the kind comes from, in grey
        m_type = new QLabel(kind.package->name.text(language), this);
        m_type->setTextFormat(Qt::PlainText);
        m_type->setWordWrap(true);
        m_type->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_type->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(m_type);

        // Only on a checked kind; its room stays, so the tiles do not move
        //: A box on a tile of the palette: an element of the kind is made with the book. In
        //: Polish: Utwórz od razu
        m_now = new QCheckBox(QCoreApplication::translate("KindPalette", "Create now"), this);
        QSizePolicy nowPolicy = m_now->sizePolicy();
        nowPolicy.setRetainSizeWhenHidden(true);
        m_now->setSizePolicy(nowPolicy);
        if (!KindPalette::canCreateNow(kind)) {
            m_now->setEnabled(false);
            if (kind.kind->isMaterial()) {
                //: The tooltip of "Create now" on a material of the Workshop, e.g. a picture.
                //: In Polish: Materiał to kopia pliku wybranego przy dodawaniu.
                m_now->setToolTip(QCoreApplication::translate(
                    "KindPalette", "A material is a copy of a file chosen when it is added."));
            } else {
                //: The tooltip of "Create now" on a kind that opens in a window of its own,
                //: e.g. a character card. In Polish: Program nie ma jeszcze edytora tego
                //: rodzaju.
                m_now->setToolTip(QCoreApplication::translate(
                    "KindPalette", "The program does not have an editor of this kind yet."));
            }
        }
        m_now->hide();
        layout->addWidget(m_now, 0, Qt::AlignHCenter);
        layout->addStretch(1);

        updateIcon();
    }

    const core::KindRef& kind() const { return m_kind; }
    core::BookPlace place() const { return m_place; }
    QCheckBox* checkBox() const { return m_check; }
    QCheckBox* nowBox() const { return m_now; }

    /// @brief Show the kind checked or not: the box, the background and "Create now", which
    ///        an unchecked kind loses
    void showChecked(bool checked) {
        m_check->setChecked(checked);
        m_now->setVisible(checked);
        if (!checked) {
            m_now->setChecked(false);
        }
        update();
    }

    /// @brief Whether an element of the kind is made with the book in the place of the tile
    bool createsNow() const {
        return m_check->isChecked() && m_now->isEnabled() && m_now->isChecked();
    }

    /// @brief Set "Create now", where the kind can be created now
    void setCreatesNow(bool now) { m_now->setChecked(now && m_now->isEnabled()); }

    /// @brief The fonts of the texts under the name, smaller than @p font
    void setSmallFonts(const QFont& font) {
        QFont small = font;
        if (font.pointSizeF() > 0) {
            small.setPointSizeF(font.pointSizeF() * SMALL_FONT_SCALE);
        } else if (font.pixelSize() > 0) {
            small.setPixelSize(std::max(1, qRound(font.pixelSize() * SMALL_FONT_SCALE)));
        }
        m_type->setFont(small);
        m_now->setFont(small);
    }

    /// @brief Draw the icon at the size and in the colors of the current icons
    void updateIcon() {
        auto& artProvider = core::ArtProvider::getInstance();
        const int size = artProvider.getIconSize(core::IconContext::Dialog);
        const QIcon icon =
            artProvider.getIcon(NavigatorPanel::iconIdOf(m_kind), core::IconContext::Dialog);
        m_icon->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatioF()));
        m_icon->setFixedSize(size, size);
    }

protected:
    /// A checked kind has a background and a border of the highlight
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        if (!m_check->isChecked()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor base = palette().color(QPalette::Base);
        const QColor highlight = palette().color(QPalette::Highlight);
        painter.setPen(QPen(mixedColor(base, highlight, CHECKED_BORDER), 1));
        painter.setBrush(mixedColor(base, highlight, CHECKED_FILL));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), CORNER_RADIUS,
                                CORNER_RADIUS);
    }

    /// A click on the tile, outside its boxes, checks or unchecks the kind
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            m_check->setFocus(Qt::MouseFocusReason);
            m_check->click();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    core::KindRef m_kind;
    core::BookPlace m_place;
    QCheckBox* m_check = nullptr;
    QLabel* m_icon = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_type = nullptr;
    QCheckBox* m_now = nullptr;
};

KindPalette::KindPalette(const core::BookTypeRegistry& registry, QWidget* parent)
    : QWidget(parent)
{
    // On the background of text fields, as a list
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(PALETTE_MARGIN, TILE_SPACING, PALETTE_MARGIN, PALETTE_MARGIN);
    layout->setSpacing(GROUP_SPACING);

    const QList<core::KindRef> kinds = registry.allKinds();
    for (const core::BookPlace place : GROUP_PLACES) {
        Group group;
        group.place = place;

        // The heading: the name of the group and, in grey, the number of its kinds
        auto* heading = new QWidget(this);
        auto* headingLayout = new QHBoxLayout(heading);
        headingLayout->setContentsMargins(0, GROUP_SPACING, 0, 0);
        group.name = new QLabel(heading);
        group.name->setTextFormat(Qt::PlainText);
        QFont bold;  // only the weight: the rest follows the font of the palette
        bold.setWeight(QFont::DemiBold);
        group.name->setFont(bold);
        headingLayout->addWidget(group.name);
        group.count = new QLabel(heading);
        group.count->setForegroundRole(QPalette::PlaceholderText);
        headingLayout->addWidget(group.count);
        headingLayout->addStretch(1);
        layout->addWidget(heading);

        auto* line = new QFrame(this);
        line->setFixedHeight(1);
        line->setAutoFillBackground(true);
        line->setBackgroundRole(QPalette::Midlight);
        layout->addWidget(line);

        auto* tiles = new QWidget(this);
        auto* flow = new FlowLayout(nullptr, TILE_SPACING, TILE_SPACING);
        tiles->setLayout(flow);
        for (const core::KindRef& kind : kinds) {
            if (!kind.offeredIn(place)) {
                continue;
            }
            auto* tile = new KindTile(kind, place, tiles);
            flow->addWidget(tile);
            group.tiles.append(tile);
            m_checked.insert(kind.kind, false);
            connect(tile->checkBox(), &QCheckBox::clicked, this,
                    [this, tile](bool checked) { setChecked(tile->kind().kind, checked); });
            connect(tile->nowBox(), &QCheckBox::clicked, this, &KindPalette::choiceChanged);
        }
        group.count->setText(QString::number(group.tiles.size()));
        layout->addWidget(tiles);
        m_groups.append(group);
    }
    layout->addStretch(1);

    // The kinds that have tiles, in the order of the palette
    for (const core::KindRef& kind : kinds) {
        if (m_checked.contains(kind.kind)) {
            m_kinds.append(kind);
        }
    }

    sizeTiles();

    // New icon colors or sizes (a change of theme or icon settings) redraw the icons
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() {
                for (const Group& group : m_groups) {
                    for (KindTile* tile : group.tiles) {
                        tile->updateIcon();
                    }
                }
                sizeTiles();
            });
}

void KindPalette::setGroupName(core::BookPlace place, const QString& name)
{
    for (Group& group : m_groups) {
        if (group.place == place) {
            group.name->setText(name);
        }
    }
}

QString KindPalette::groupName(core::BookPlace place) const
{
    for (const Group& group : m_groups) {
        if (group.place == place) {
            return group.name->text();
        }
    }
    return QString();
}

void KindPalette::setChoice(const core::PaletteChoice& choice)
{
    for (auto it = m_checked.begin(); it != m_checked.end(); ++it) {
        const core::ElementKind* kind = it.key();
        it.value() =
            std::any_of(choice.kinds.cbegin(), choice.kinds.cend(),
                        [kind](const core::KindRef& chosen) { return chosen.kind == kind; });
    }
    for (const Group& group : m_groups) {
        for (KindTile* tile : group.tiles) {
            tile->showChecked(m_checked.value(tile->kind().kind));
            tile->setCreatesNow(std::any_of(
                choice.elements.cbegin(), choice.elements.cend(),
                [tile](const core::StartElement& element) {
                    return element.kind.kind == tile->kind().kind && element.place == tile->place();
                }));
        }
    }
    emit choiceChanged();
}

core::PaletteChoice KindPalette::choice() const
{
    core::PaletteChoice choice;
    for (const core::KindRef& kind : m_kinds) {
        if (m_checked.value(kind.kind)) {
            choice.kinds.append(kind);
        }
    }
    for (const Group& group : m_groups) {
        for (const KindTile* tile : group.tiles) {
            if (tile->createsNow()) {
                choice.elements.append(core::StartElement{tile->kind(), tile->place()});
            }
        }
    }
    return choice;
}

void KindPalette::checkAll()
{
    core::PaletteChoice all = choice();
    all.kinds = m_kinds;
    setChoice(all);
}

void KindPalette::checkOnly(const QString& packageId)
{
    core::PaletteChoice only;
    for (const core::KindRef& kind : m_kinds) {
        if (kind.package->id == packageId) {
            only.kinds.append(kind);
        }
    }
    for (const core::StartElement& element : choice().elements) {
        if (element.kind.package->id == packageId) {
            only.elements.append(element);
        }
    }
    setChoice(only);
}

void KindPalette::uncheckAll()
{
    setChoice(core::PaletteChoice{});
}

int KindPalette::checkedCount() const
{
    return static_cast<int>(
        std::count_if(m_checked.cbegin(), m_checked.cend(), [](bool checked) { return checked; }));
}

QList<core::KindRef> KindPalette::kindsIn(core::BookPlace place) const
{
    QList<core::KindRef> kinds;
    for (const Group& group : m_groups) {
        if (group.place != place) {
            continue;
        }
        for (const KindTile* tile : group.tiles) {
            kinds.append(tile->kind());
        }
    }
    return kinds;
}

bool KindPalette::canCreateNow(const core::KindRef& kind)
{
    return kind && kind.kind->form != core::ElementForm::Window;
}

void KindPalette::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        sizeTiles();
    }
}

void KindPalette::setChecked(const core::ElementKind* kind, bool checked)
{
    m_checked[kind] = checked;
    for (const Group& group : m_groups) {
        for (KindTile* tile : group.tiles) {
            if (tile->kind().kind == kind) {
                tile->showChecked(checked);
            }
        }
    }
    emit choiceChanged();
}

void KindPalette::sizeTiles()
{
    // As wide as the mockup's tiles at the font of the palette, or as "Create now" in the
    // language of the program, and as high as the highest tile, so that the tiles stand in
    // even rows
    int width = qRound(TILE_WIDTH_EMS * QFontInfo(font()).pixelSize());
    for (const Group& group : m_groups) {
        for (KindTile* tile : group.tiles) {
            tile->setSmallFonts(font());
            width = std::max(width, tile->nowBox()->sizeHint().width() + 2 * TILE_MARGIN);
        }
    }
    int height = qRound(TILE_HEIGHT_EMS * QFontInfo(font()).pixelSize());
    for (const Group& group : m_groups) {
        for (KindTile* tile : group.tiles) {
            tile->setMinimumSize(0, 0);
            tile->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            height = std::max(height, tile->layout()->totalHeightForWidth(width));
        }
    }
    for (const Group& group : m_groups) {
        for (KindTile* tile : group.tiles) {
            tile->setFixedSize(width, height);
        }
    }
}

} // namespace gui
} // namespace kalahari
