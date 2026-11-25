// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file portrait_dialog.cpp
/// @brief The window Portrait: a portrait of a card put together from ready parts

#include "kalahari/gui/dialogs/portrait_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// The side of the preview
constexpr int PREVIEW_SIDE = 188;
/// The side of the portrait on a tile, and the width of a tile
constexpr int TILE_SIDE = 52;
constexpr int TILE_WIDTH = 76;
/// The lines of the name of a value under its tile
constexpr int TILE_LINES = 2;
/// The side of a swatch of a palette
constexpr int SWATCH_SIDE = 18;
/// The side of a portrait of the card in the strip, and the width of its place
constexpr int STRIP_SIDE = 40;
constexpr int STRIP_WIDTH = 64;
/// The tiles shown in a line of the right side, and its height in halves of a tile, at the
/// start
constexpr int TILES_ACROSS = 5;
constexpr int OPTIONS_HALF_TILES = 3;
/// The squares of the checkerboard behind a portrait without a background
constexpr int CHECKER = 8;
/// What a tile takes in a line beside its width: its frame and the room to the next one
constexpr int TILE_SPACING = 6;
/// The size of the names of the values under the tiles, a part of the size of the dialog's font
constexpr double SMALL_FONT = 0.9;
/// The size of a picture saved in the materials: the height, and the width of a bust
constexpr int PICTURE_SIDE = 800;

/// The properties of the tiles and the swatches: the part or the color, and the value
constexpr const char* PART_KEY = "portraitPart";
constexpr const char* VALUE_KEY = "portraitValue";
constexpr const char* COLOR_KEY = "portraitColor";
/// Undo, written out: the same on every system (Qt's standard key differs between them)
constexpr QKeyCombination UNDO_KEY(Qt::ControlModifier, Qt::Key_Z);

/// The frame a tile of @p part shows
AvatarFrame frameOf(const Avatar::Part& part) {
    if (part.closeUp) {
        return AvatarFrame::Face;
    }
    return part.full ? AvatarFrame::Full : AvatarFrame::Bust;
}

/// The words of @p text; a word with a hyphen too long for @p width breaks after it
QStringList wordsOf(const QString& text, const QFontMetrics& metrics, int width) {
    QStringList words;
    for (const QString& word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const qsizetype hyphen = word.indexOf(QLatin1Char('-'));
        if (metrics.horizontalAdvance(word) > width && hyphen > 0 && hyphen < word.size() - 1) {
            words << word.left(hyphen + 1) << QStringLiteral("\n") + word.mid(hyphen + 1);
        } else {
            words << word;
        }
    }
    return words;
}

/// @p text in at most @p lines lines of @p width pixels in @p metrics, the last one elided
QString wrapped(const QString& text, const QFontMetrics& metrics, int width, int lines) {
    QStringList result;
    QString line;
    for (QString word : wordsOf(text, metrics, width)) {
        // The rest of a word broken after its hyphen goes on in the next line
        const bool broken = word.startsWith(QLatin1Char('\n'));
        if (broken) {
            word.remove(0, 1);
            result << line;
            line = word;
            continue;
        }
        const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
        if (line.isEmpty() || metrics.horizontalAdvance(longer) <= width) {
            line = longer;
            continue;
        }
        result << line;
        line = word;
    }
    result << line;
    if (result.size() > lines) {
        const QString rest = result.mid(lines - 1).join(QLatin1Char(' '));
        result = result.mid(0, lines - 1);
        result << rest;
    }
    result.last() = metrics.elidedText(result.last(), Qt::ElideRight, width);
    return result.join(QLatin1Char('\n'));
}

/// A heading of a part or a color on the right side
QLabel* headingOf(const QString& text, QWidget* parent) {
    auto* heading = new QLabel(text, parent);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    return heading;
}

/// The scroll area of the tiles: as wide as a line of tiles and as high as two of them at the
/// start; the window gives it more room when it grows
class OptionsArea : public QScrollArea {
public:
    explicit OptionsArea(QWidget* parent) : QScrollArea(parent) {
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
    }

    [[nodiscard]] QSize sizeHint() const override {
        const int lineHeight = fontMetrics().lineSpacing();
        const int tile = TILE_SIDE + TILE_LINES * lineHeight + lineHeight;
        return {TILES_ACROSS * (TILE_WIDTH + TILE_SPACING) +
                    verticalScrollBar()->sizeHint().width(),
                OPTIONS_HALF_TILES * tile / 2 + 2 * lineHeight};
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        return {TILE_WIDTH * 2, TILE_SIDE + 2 * fontMetrics().lineSpacing()};
    }
};

/// The right side: as wide as the tiles want, whatever the description and the choices under
/// them would take in one line; they wrap
class OptionSide : public QWidget {
public:
    OptionSide(QWidget* parent, const QScrollArea* options) : QWidget(parent), m_area(options) {}

    [[nodiscard]] QSize sizeHint() const override {
        const int width = m_area->sizeHint().width();
        return {width, layout() != nullptr ? layout()->totalHeightForWidth(width)
                                           : QWidget::sizeHint().height()};
    }

private:
    const QScrollArea* m_area;
};

/// A text in lines as wide as its place: not wider than that in its hint
class NarrowLabel : public QLabel {
public:
    explicit NarrowLabel(QWidget* parent) : QLabel(parent) { setWordWrap(true); }

    [[nodiscard]] QSize sizeHint() const override {
        return {minimumSizeHint().width(), QLabel::sizeHint().height()};
    }
};

/// The tile of value @p value of part @p part: @p avatar with the value, and whether @p avatar
/// has it; a part of many shows the value with the others
std::pair<QJsonObject, bool> tileOf(const QJsonObject& avatar, const Avatar::Part& part,
                                    const QString& value) {
    QJsonObject choices = avatar;
    if (!part.many) {
        choices.insert(part.key, value);
        return {choices, avatar.value(part.key).toString() == value};
    }
    QStringList values = avatar.value(part.key).toVariant().toStringList();
    const bool chosen = values.contains(value);
    if (!chosen) {
        values << value;
    }
    choices.insert(part.key, QJsonArray::fromStringList(values));
    return {choices, chosen};
}

}  // namespace

PortraitDialog::PortraitDialog(PortraitSubject subject, QWidget* parent)
    : KalahariDialog(parent),
      m_subject(std::move(subject)),
      m_random(QRandomGenerator::global()->generate()) {
    setObjectName(QStringLiteral("portraitDialog"));
    setHeadingIcon(QStringLiteral("template.character"));
    //: The window that puts a portrait of a character together from parts; %1: the name of
    //: the card. In Polish: Portret: %1
    setHeading(tr("Portrait: %1").arg(m_subject.name),
               //: In Polish: Złóż portret z gotowych elementów – bez rysowania. Każdą zmianę od
               //: razu widać w podglądzie.
               tr("Put the portrait together from ready parts, without drawing. Each change "
                  "shows at once in the preview."));
    setCompactHeading(true);
    if (!m_subject.color.isValid()) {
        m_subject.color = palette().color(QPalette::Mid);
    }
    //: The gear of the window Portrait. In Polish: Ustawienia portretów
    addSettingsButton(QString::fromLatin1(SettingsDialog::CARDS_PAGE), tr("Portrait Settings"));

    const qsizetype count = m_subject.portraits.size();
    if (m_subject.current < 0 || m_subject.current >= count ||
        !m_subject.portraits.at(m_subject.current).isComposed()) {
        m_subject.portraits << proposal();
        m_subject.current = m_subject.portraits.size() - 1;
    }
    const auto frame = core::SettingsManager::getInstance().get<std::string>(
        Avatar::FRAME_SETTING, Avatar::FRAME_BUST);
    m_frame = frame == Avatar::FRAME_FULL ? AvatarFrame::Full : AvatarFrame::Bust;

    auto* columns = new QHBoxLayout();
    columns->addWidget(previewSide(), 0, Qt::AlignTop);
    columns->addWidget(groupSide(), 0);
    columns->addWidget(optionSide(), 1);
    contentLayout()->addLayout(columns, 1);

    //: The keys of the window Portrait, beside its buttons. In Polish: ↑ ↓ – grupy · Tab –
    //: elementy · strzałki i Spacja – wybór · Ctrl+Z – cofnij
    auto* keys = new QLabel(tr("↑ ↓ – groups · Tab – parts · "
                               "arrows and Space – choice · Ctrl+Z – undo"));
    keys->setWordWrap(true);
    keys->setForegroundRole(QPalette::PlaceholderText);
    addSideWidget(keys);
    auto* undoKey = new QShortcut(QKeySequence(UNDO_KEY), this);
    undoKey->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoKey, &QShortcut::activated, this, &PortraitDialog::undo);

    showStrip();
    m_groups->setCurrentRow(0);
    changed();
}

QWidget* PortraitDialog::previewSide() {
    auto* side = new QWidget(this);
    auto* layout = new QVBoxLayout(side);
    layout->setContentsMargins(0, 0, 0, 0);
    m_preview = new QLabel(side);
    m_preview->setObjectName(QStringLiteral("portraitPreview"));
    m_preview->setFixedSize(PREVIEW_SIDE, PREVIEW_SIDE);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    //: In Polish: Podgląd portretu
    m_preview->setAccessibleName(tr("Preview of the Portrait"));
    layout->addWidget(m_preview, 0, Qt::AlignHCenter);

    m_frameButtons = new QButtonGroup(side);
    //: The frame of the preview of a portrait. In Polish: Kadr:
    layout->addWidget(choiceRow(tr("Frame:"), m_frameButtons,
                                {//: In Polish: Popiersie
                                 tr("Bust"),
                                 //: In Polish: Cała postać
                                 tr("Whole Figure")}));
    m_frameButtons->button(m_frame == AvatarFrame::Full ? 1 : 0)->setChecked(true);
    connect(m_frameButtons, &QButtonGroup::idClicked, this,
            [this](int id) { setFrame(id == 1 ? AvatarFrame::Full : AvatarFrame::Bust); });

    m_styleButtons = new QButtonGroup(side);
    //: The style a portrait is drawn in. In Polish: Styl:
    layout->addWidget(choiceRow(tr("Style:"), m_styleButtons,
                                {//: In Polish: Płaski
                                 tr("Flat"),
                                 //: In Polish: Kreskowy
                                 tr("Line")}));
    connect(m_styleButtons, &QButtonGroup::idClicked, this, [this](int id) {
        keepForUndo();
        shown().style = QString::fromLatin1(id == 1 ? AvatarLook::LINE : AvatarLook::FLAT);
        changed();
    });

    auto* buttons = new QHBoxLayout();
    //: Draws the parts and the colors of a portrait by chance. In Polish: &Losuj
    m_randomButton = new QPushButton(tr("&Randomize"), side);
    //: In Polish: Dobiera elementy i naturalne kolory zgodnie z polem „Płeć”
    m_randomButton->setToolTip(tr("Chooses the parts and the natural colors by chance, as the "
                                  "field Gender suits"));
    //: In Polish: Cofnij
    m_undoButton = new QPushButton(tr("Undo"), side);
    m_undoButton->setToolTip(QKeySequence(UNDO_KEY).toString(QKeySequence::NativeText));
    //: In Polish: &Od nowa
    m_resetButton = new QPushButton(tr("Start &Over"), side);
    //: In Polish: Wraca do propozycji z pól „Płeć” i „Wiek” karty
    m_resetButton->setToolTip(tr("Back to the portrait the fields Gender and Age of the card "
                                 "suggest"));
    for (QPushButton* button : {m_randomButton, m_undoButton, m_resetButton}) {
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    connect(m_randomButton, &QPushButton::clicked, this, &PortraitDialog::randomize);
    connect(m_undoButton, &QPushButton::clicked, this, &PortraitDialog::undo);
    connect(m_resetButton, &QPushButton::clicked, this, &PortraitDialog::startOver);
    layout->addLayout(buttons);
    layout->addStretch(1);
    return side;
}

QWidget* PortraitDialog::choiceRow(const QString& label, QButtonGroup* group,
                                   const QStringList& texts) {
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* name = new QLabel(label, row);
    layout->addWidget(name);
    layout->addSpacing(name->fontMetrics().averageCharWidth());
    for (int i = 0; i < texts.size(); ++i) {
        auto* button = new QToolButton(row);
        button->setText(texts.at(i));
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        group->addButton(button, i);
        layout->addWidget(button);
    }
    group->setExclusive(true);
    name->setBuddy(group->button(0));
    return row;
}

QWidget* PortraitDialog::groupSide() {
    auto* side = new QWidget(this);
    auto* layout = new QVBoxLayout(side);
    layout->setContentsMargins(0, 0, 0, 0);
    //: The groups of the parts of a portrait. In Polish: &Elementy
    auto* partsLabel = new QLabel(tr("&Parts"), side);
    layout->addWidget(partsLabel);
    m_groups = new QListWidget(side);
    m_groups->setObjectName(QStringLiteral("portraitGroups"));
    partsLabel->setBuddy(m_groups);
    for (const Avatar::Group& group : Avatar::groups()) {
        auto* item = new QListWidgetItem(Avatar::groupName(group.id), m_groups);
        item->setData(Qt::UserRole, group.id);
        if (group.full) {
            //: The group of the parts only the whole figure shows. In Polish: Widać na całej
            //: postaci
            item->setToolTip(tr("Shown in the whole figure"));
        }
    }
    const int rows = m_groups->count();
    m_groups->setFixedHeight(m_groups->sizeHintForRow(0) * rows + 2 * m_groups->frameWidth());
    m_groups->setMaximumWidth(m_groups->sizeHintForColumn(0) + 2 * m_groups->frameWidth() +
                              m_groups->fontMetrics().averageCharWidth() * 2);
    connect(m_groups, &QListWidget::currentRowChanged, this, &PortraitDialog::showGroup);
    layout->addWidget(m_groups);

    auto* strip = new QHBoxLayout();
    //: The portraits of a card: its versions with their labels. In Polish: Por&trety karty
    auto* stripLabel = new QLabel(tr("Card Por&traits"), side);
    stripLabel->setWordWrap(true);
    strip->addWidget(stripLabel, 1);
    m_newButton = new QToolButton(side);
    m_newButton->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("common.add")));
    //: A new portrait of the card, a copy of the one shown. In Polish: Nowy portret
    m_newButton->setToolTip(tr("New Portrait"));
    m_newButton->setAccessibleName(m_newButton->toolTip());
    m_newButton->setAutoRaise(true);
    connect(m_newButton, &QToolButton::clicked, this, &PortraitDialog::addPortrait);
    strip->addWidget(m_newButton);
    layout->addLayout(strip);

    m_strip = new QListWidget(side);
    m_strip->setObjectName(QStringLiteral("portraitStrip"));
    stripLabel->setBuddy(m_strip);
    m_strip->setViewMode(QListView::IconMode);
    m_strip->setFlow(QListView::LeftToRight);
    m_strip->setWrapping(false);
    m_strip->setMovement(QListView::Static);
    m_strip->setIconSize(QSize(STRIP_SIDE, STRIP_SIDE));
    const int lineHeight = m_strip->fontMetrics().lineSpacing();
    m_strip->setGridSize(QSize(STRIP_WIDTH, STRIP_SIDE + lineHeight + lineHeight / 2));
    m_strip->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_strip->setFixedHeight(m_strip->gridSize().height() +
                            m_strip->horizontalScrollBar()->sizeHint().height() +
                            2 * m_strip->frameWidth());
    // Two portraits in view side by side, the groups as wide
    const int stripWidth = 2 * (STRIP_WIDTH + m_strip->frameWidth());
    m_groups->setMaximumWidth(std::max(m_groups->maximumWidth(), stripWidth));
    m_strip->setMinimumWidth(stripWidth);
    m_strip->setMaximumWidth(m_groups->maximumWidth());
    connect(m_strip, &QListWidget::currentRowChanged, this, [this](int row) {
        if (const QListWidgetItem* item = m_strip->item(row)) {
            showPortrait(item->data(Qt::UserRole).toLongLong());
        }
    });
    layout->addWidget(m_strip);

    auto* labelRow = new QHBoxLayout();
    //: The label of a portrait of a card ("youth", "after the battle"). In Polish: &Podpis:
    auto* labelName = new QLabel(tr("&Label:"), side);
    m_label = new QLineEdit(side);
    m_label->setObjectName(QStringLiteral("portraitLabel"));
    //: In Polish: np. młodość
    m_label->setPlaceholderText(tr("e.g. youth"));
    labelName->setBuddy(m_label);
    labelRow->addWidget(labelName);
    labelRow->addWidget(m_label, 1);
    connect(m_label, &QLineEdit::textEdited, this, [this](const QString& text) {
        shown().label = text.trimmed();
        showStrip();
    });
    layout->addLayout(labelRow);
    layout->addStretch(1);
    side->setMaximumWidth(m_groups->maximumWidth());
    return side;
}

QWidget* PortraitDialog::optionSide() {
    m_options = new OptionsArea(this);
    m_options->setObjectName(QStringLiteral("portraitOptions"));
    auto* side = new OptionSide(this, m_options);
    auto* layout = new QVBoxLayout(side);
    layout->setContentsMargins(0, 0, 0, 0);
    m_options->setParent(side);
    layout->addWidget(m_options, 1);

    //: The look of a portrait in words. In Polish: Opis wyglądu
    layout->addWidget(headingOf(tr("Description of the Look"), side));
    m_description = new NarrowLabel(side);
    m_description->setObjectName(QStringLiteral("portraitDescription"));
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_description);
    auto* boxes = new QWidget(side);
    auto* flow = new FlowLayout(boxes);
    flow->setContentsMargins(0, 0, 0, 0);
    //: Adds the description of the look of the portrait at the end of the field Appearance of
    //: the card. In Polish: &Dopisz opis na końcu pola „Wygląd”
    m_appendBox = new QCheckBox(tr("&Add the description at the end of the field "
                                   "“Appearance”"),
                                boxes);
    m_appendBox->setVisible(m_subject.hasAppearance);
    m_appendBox->setToolTip(
        m_subject.appearance.trimmed().isEmpty()
            //: In Polish: Pole „Wygląd” jest teraz puste.
            ? tr("The field “Appearance” is empty now.")
            //: %1: the text of the field. In Polish: W polu „Wygląd” jest teraz: %1
            : tr("The field “Appearance” has now: %1").arg(m_subject.appearance));
    //: Saves the portrait also as a PNG picture among the materials of the book. In Polish:
    //: &Zapisz też jako obraz w materiałach
    m_pictureBox = new QCheckBox(tr("Also &save as a picture in the materials"), boxes);
    flow->addWidget(m_appendBox);
    flow->addWidget(m_pictureBox);
    layout->addWidget(boxes);
    return side;
}

core::CardPortrait PortraitDialog::proposal() const {
    core::CardPortrait portrait;
    portrait.style = Avatar::newStyle();
    portrait.avatar = Avatar::proposal(m_subject.gender, m_subject.age);
    return portrait;
}

QString PortraitDialog::description() const {
    return Avatar::described(Avatar::resolved(shown().avatar), true);
}

bool PortraitDialog::appendsDescription() const {
    return m_subject.hasAppearance && m_appendBox->isChecked();
}

bool PortraitDialog::savesPicture() const {
    return m_pictureBox->isChecked();
}

QImage PortraitDialog::picture() const {
    return pictureOf(shown(), m_frame, m_subject.color, m_subject.gender);
}

QImage PortraitDialog::pictureOf(const core::CardPortrait& portrait, AvatarFrame frame,
                                 const QColor& card, const QString& gender) {
    const QRectF view = AvatarArt::drawn().view(frame);
    const int width = frame == AvatarFrame::Full && view.height() > 0
                          ? qRound(PICTURE_SIDE * view.width() / view.height())
                          : PICTURE_SIDE;
    return Avatar::imageOf(portrait, frame, card, gender, QSize(width, PICTURE_SIDE));
}

QLabel* PortraitDialog::descriptionLabel() const {
    return m_description;
}

QToolButton* PortraitDialog::tile(const QString& key, const QString& value) const {
    const QList<QToolButton*> tiles = optionButtons<QToolButton>();
    const auto found = std::find_if(tiles.cbegin(), tiles.cend(), [&](const QToolButton* each) {
        return each->property(PART_KEY).toString() == key &&
               each->property(VALUE_KEY).toString() == value;
    });
    return found == tiles.cend() ? nullptr : *found;
}

QToolButton* PortraitDialog::swatch(const QString& key, const QString& color) const {
    const QList<QToolButton*> swatches = optionButtons<QToolButton>();
    const auto found =
        std::find_if(swatches.cbegin(), swatches.cend(), [&](const QToolButton* each) {
            return each->property(COLOR_KEY).toString() == key &&
                   each->property(VALUE_KEY).toString() == color;
        });
    return found == swatches.cend() ? nullptr : *found;
}

QPushButton* PortraitDialog::otherColorButton(const QString& key) const {
    const QList<QPushButton*> buttons = optionButtons<QPushButton>();
    const auto found = std::find_if(buttons.cbegin(), buttons.cend(), [&](const QPushButton* each) {
        return each->property(COLOR_KEY).toString() == key;
    });
    return found == buttons.cend() ? nullptr : *found;
}

void PortraitDialog::keepForUndo() {
    m_undo << Step{m_subject.portraits, m_subject.current};
}

void PortraitDialog::changed() {
    m_undoButton->setEnabled(!m_undo.isEmpty());
    {
        const QSignalBlocker blocker(m_styleButtons);
        m_styleButtons->button(shown().style == QLatin1String(AvatarLook::LINE) ? 1 : 0)
            ->setChecked(true);
    }
    if (m_label->text() != shown().label) {
        m_label->setText(shown().label);
    }
    showPreview();
    showTiles();
    showStrip();
    m_description->setText(description());
}

void PortraitDialog::choose(const QString& key, const QString& value) {
    const Avatar::Part* part = Avatar::part(key);
    if (part == nullptr) {
        return;
    }
    keepForUndo();
    QJsonObject& avatar = shown().avatar;
    if (part->many) {
        QStringList values = Avatar::resolved(avatar).value(key).toVariant().toStringList();
        if (values.contains(value)) {
            values.removeAll(value);
        } else {
            values << value;
        }
        avatar.insert(key, QJsonArray::fromStringList(values));
    } else {
        avatar.insert(key, value);
    }
    if (part->full && m_frame != AvatarFrame::Full) {
        setFrame(AvatarFrame::Full);
    }
    changed();
}

void PortraitDialog::setColor(const QString& key, const QString& color) {
    const Avatar::Color* chosen = Avatar::color(key);
    if (chosen == nullptr) {
        return;
    }
    keepForUndo();
    shown().avatar.insert(key, color);
    if (chosen->full && m_frame != AvatarFrame::Full) {
        setFrame(AvatarFrame::Full);
    }
    if (swatch(key, color) == nullptr) {
        buildOptions();
    }
    changed();
}

void PortraitDialog::chooseOtherColor(const QString& key) {
    const QString now = Avatar::resolved(shown().avatar).value(key).toString();
    //: The title of the color window for a color of a portrait; %1: the color ("Skin Color").
    //: In Polish: Portret – %1
    const std::optional<QColor> color = ColorDialog::getColor(
        QColor(now), this, tr("Portrait – %1").arg(Avatar::colorName(key)));
    if (!color || color->name() == now) {
        return;
    }
    setColor(key, color->name());
    if (QToolButton* chosen = swatch(key, color->name())) {
        chosen->setFocus();
    }
}

void PortraitDialog::randomize() {
    keepForUndo();
    shown().avatar = Avatar::randomized(Avatar::resolved(shown().avatar), m_subject.gender,
                                        AvatarPalettes::current(), m_random);
    refreshOptions();
    changed();
}

void PortraitDialog::undo() {
    if (m_undo.isEmpty()) {
        return;
    }
    const Step step = m_undo.takeLast();
    m_subject.portraits = step.portraits;
    m_subject.current = step.current;
    refreshOptions();
    changed();
}

void PortraitDialog::startOver() {
    keepForUndo();
    shown().avatar = proposal().avatar;
    refreshOptions();
    changed();
}

void PortraitDialog::addPortrait() {
    keepForUndo();
    core::CardPortrait copy = shown();
    //: The label of a new portrait of a card, a copy of the one shown. In Polish: nowy
    copy.label = tr("new", "label of a portrait");
    m_subject.portraits << copy;
    m_subject.current = m_subject.portraits.size() - 1;
    changed();
    m_label->setFocus();
    m_label->selectAll();
}

void PortraitDialog::showPortrait(qsizetype index) {
    if (index == m_subject.current || index < 0 || index >= m_subject.portraits.size() ||
        !m_subject.portraits.at(index).isComposed()) {
        return;
    }
    m_subject.current = index;
    refreshOptions();
    changed();
}

void PortraitDialog::setFrame(AvatarFrame frame) {
    m_frame = frame;
    {
        const QSignalBlocker blocker(m_frameButtons);
        m_frameButtons->button(frame == AvatarFrame::Full ? 1 : 0)->setChecked(true);
    }
    showPreview();
}

void PortraitDialog::showGroup(int row) {
    if (row < 0 || row >= Avatar::groups().size()) {
        return;
    }
    if (Avatar::groups().at(row).full && m_frame != AvatarFrame::Full) {
        setFrame(AvatarFrame::Full);
    }
    buildOptions();
    showTiles();
}

void PortraitDialog::buildOptions() {
    const int row = m_groups->currentRow();
    if (row < 0 || row >= Avatar::groups().size()) {
        return;
    }
    const Avatar::Group& group = Avatar::groups().at(row);
    // The page goes at once, and is deleted later: one of its buttons may be what calls
    if (QWidget* old = m_options->takeWidget()) {
        old->hide();
        old->deleteLater();
    }
    m_signBox = nullptr;
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    m_options->setWidget(page);
    for (const QString& key : group.parts) {
        if (const Avatar::Part* part = Avatar::part(key)) {
            addTiles(*part);
        }
    }
    for (const QString& key : group.colors) {
        if (const Avatar::Color* color = Avatar::color(key)) {
            addPalette(*color);
        }
    }
    if (group.sign) {
        //: In Polish: Znak płci
        layout->addWidget(headingOf(tr("Sign of the Gender"), page));
        //: %1: the sign ("♀"). In Polish: Z&nak z pola „Płeć” w rogu portretu (%1)
        m_signBox = new QCheckBox(tr("The sig&n of the field “Gender” in the corner "
                                     "of the portrait (%1)")
                                      .arg(Avatar::signOf(m_subject.gender)),
                                  page);
        connect(m_signBox, &QCheckBox::toggled, this, [this](bool on) {
            keepForUndo();
            shown().avatar.insert(QLatin1String(Avatar::SIGN), on);
            changed();
        });
        layout->addWidget(m_signBox);
    }
    layout->addStretch(1);
}

void PortraitDialog::refreshOptions() {
    const int row = m_groups->currentRow();
    if (row < 0 || row >= Avatar::groups().size()) {
        return;
    }
    const QJsonObject avatar = Avatar::resolved(shown().avatar);
    const QStringList& colors = Avatar::groups().at(row).colors;
    if (std::any_of(colors.cbegin(), colors.cend(), [this, &avatar](const QString& key) {
            return swatch(key, avatar.value(key).toString()) == nullptr;
        })) {
        buildOptions();
    }
}

void PortraitDialog::addTiles(const Avatar::Part& part) {
    QWidget* page = m_options->widget();
    auto* layout = static_cast<QVBoxLayout*>(page->layout());
    QString title = Avatar::partName(part.key);
    if (part.many) {
        //: After the name of a part of which several values go together ("Extras"). In
        //: Polish: %1 – można łączyć
        title = tr("%1 – they go together").arg(title);
    }
    layout->addWidget(headingOf(title, page));
    auto* row = new QWidget(page);
    auto* flow = new FlowLayout(row, 4, 4);
    flow->setContentsMargins(0, 0, 0, 0);
    // The tiles of a part of one value are exclusive among themselves (as radio buttons, the
    // arrows choose), those of a part of many are not
    QFont small = row->font();
    small.setPointSizeF(small.pointSizeF() * SMALL_FONT);
    const QFontMetrics metrics(small);
    for (const QString& value : part.values) {
        auto* tile = new QToolButton(row);
        tile->setProperty(PART_KEY, part.key);
        tile->setProperty(VALUE_KEY, value);
        tile->setCheckable(true);
        tile->setAutoRaise(true);
        tile->setFont(small);
        tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tile->setIconSize(QSize(TILE_SIDE, TILE_SIDE));
        const QString name = Avatar::valueName(part.key, value);
        tile->setText(wrapped(name, metrics, TILE_WIDTH - 4, TILE_LINES));
        tile->setToolTip(Avatar::partName(part.key) + QStringLiteral(": ") + name);
        tile->setAccessibleName(tile->toolTip());
        tile->setFixedWidth(TILE_WIDTH);
        tile->setAutoExclusive(!part.many);
        flow->addWidget(tile);
        connect(tile, &QToolButton::clicked, this,
                [this, key = part.key, value]() { choose(key, value); });
    }
    layout->addWidget(row);
}

void PortraitDialog::addPalette(const Avatar::Color& color) {
    QWidget* page = m_options->widget();
    auto* layout = static_cast<QVBoxLayout*>(page->layout());
    QLabel* heading = headingOf(Avatar::colorName(color.key), page);
    heading->setProperty(COLOR_KEY, color.key);
    layout->addWidget(heading);
    auto* row = new QWidget(page);
    auto* flow = new FlowLayout(row, 3, 3);
    flow->setContentsMargins(0, 0, 0, 0);
    const AvatarPalettes palettes = AvatarPalettes::current();
    QStringList natural = palettes.colors(color.palette);
    const QStringList other = palettes.colors(color.palette + QStringLiteral("Other"));
    const QString now = Avatar::resolved(shown().avatar).value(color.key).toString();
    if (!natural.contains(now) && !other.contains(now)) {
        natural << now;
    }
    addSwatches(row, color.key, natural);
    if (!other.isEmpty()) {
        //: Before the colors of a palette that are not natural ones (elves, ghosts). In
        //: Polish: Spoza natury:
        flow->addWidget(new QLabel(tr("Not natural:"), row));
        addSwatches(row, color.key, other);
    }
    //: Opens the color window of the program for a color of a portrait. In Polish: Inny...
    auto* otherButton = new QPushButton(tr("Other..."), row);
    otherButton->setProperty(COLOR_KEY, color.key);
    otherButton->setAutoDefault(false);
    connect(otherButton, &QPushButton::clicked, this,
            [this, key = color.key]() { chooseOtherColor(key); });
    flow->addWidget(otherButton);
    layout->addWidget(row);
}

void PortraitDialog::addSwatches(QWidget* row, const QString& key, const QStringList& colors) {
    const Avatar::Color* color = Avatar::color(key);
    const QColor rim = palette().color(QPalette::Mid);
    for (const QString& each : colors) {
        auto* swatch = new QToolButton(row);
        swatch->setProperty(COLOR_KEY, key);
        swatch->setProperty(VALUE_KEY, each);
        swatch->setCheckable(true);
        swatch->setAutoExclusive(true);
        swatch->setAutoRaise(true);
        swatch->setIcon(QIcon(PaletteEdit::swatchOf(QColor(each), SWATCH_SIDE,
                                                    devicePixelRatioF(), rim)));
        swatch->setIconSize(QSize(SWATCH_SIDE, SWATCH_SIDE));
        const QString name =
            color != nullptr ? AvatarPalettes::colorName(color->palette, each) : QString();
        swatch->setToolTip(name.isEmpty() ? each : name);
        swatch->setAccessibleName(Avatar::colorName(key) + QStringLiteral(": ") +
                                  swatch->toolTip());
        row->layout()->addWidget(swatch);
        connect(swatch, &QToolButton::clicked, this,
                [this, key, each]() { setColor(key, each); });
    }
}

void PortraitDialog::showTiles() {
    QWidget* page = m_options->widget();
    if (page == nullptr) {
        return;
    }
    const QJsonObject avatar = Avatar::resolved(shown().avatar);
    for (QToolButton* button : page->findChildren<QToolButton*>()) {
        const QString value = button->property(VALUE_KEY).toString();
        if (const QString key = button->property(COLOR_KEY).toString(); !key.isEmpty()) {
            button->setChecked(avatar.value(key).toString() == value);
            continue;
        }
        const Avatar::Part* part = Avatar::part(button->property(PART_KEY).toString());
        if (part == nullptr) {
            continue;
        }
        const auto [choices, chosen] = tileOf(avatar, *part, value);
        button->setChecked(chosen);
        button->setIcon(QIcon(drawn(choices, frameOf(*part), TILE_SIDE, false)));
    }
    for (QLabel* heading : page->findChildren<QLabel*>()) {
        if (heading->property(COLOR_KEY).toString() == QLatin1String("clothesColor")) {
            const bool armor = avatar.value(QStringLiteral("clothes")).toString() ==
                               QLatin1String("armor");
            //: After "Clothes Color" while the clothes are an armor. In Polish: %1 – zbroja
            //: ma kolor stali
            heading->setText(armor ? tr("%1 – the armor has the color of steel")
                                         .arg(Avatar::colorName(QStringLiteral("clothesColor")))
                                   : Avatar::colorName(QStringLiteral("clothesColor")));
        }
    }
    if (m_signBox != nullptr) {
        const QSignalBlocker blocker(m_signBox);
        m_signBox->setChecked(avatar.value(QLatin1String(Avatar::SIGN)).toBool());
    }
}

void PortraitDialog::showPreview() {
    const qreal ratio = devicePixelRatioF();
    const int pixels = qRound(PREVIEW_SIDE * ratio);
    QPixmap preview(pixels, pixels);
    preview.fill(palette().color(QPalette::Base));
    const QJsonObject avatar = Avatar::resolved(shown().avatar);
    QPainter painter(&preview);
    if (avatar.value(QStringLiteral("background")).toString() == QLatin1String("none")) {
        // A checkerboard where the portrait has no background
        const int square = qRound(CHECKER * ratio);
        const QColor dark = palette().color(QPalette::AlternateBase);
        for (int y = 0; y < pixels; y += square) {
            for (int x = (y / square) % 2 * square; x < pixels; x += 2 * square) {
                painter.fillRect(x, y, square, square, dark);
            }
        }
    }
    painter.drawImage(0, 0, Avatar::imageOf(shown(), m_frame, m_subject.color, m_subject.gender,
                                            QSize(pixels, pixels)));
    painter.end();
    preview.setDevicePixelRatio(ratio);
    m_preview->setPixmap(preview);
}

void PortraitDialog::showStrip() {
    QList<qsizetype> composed;
    for (qsizetype i = 0; i < m_subject.portraits.size(); ++i) {
        if (m_subject.portraits.at(i).isComposed()) {
            composed << i;
        }
    }
    // The items stay while there are as many: the strip may be what calls
    const QSignalBlocker blocker(m_strip);
    if (m_strip->count() != composed.size()) {
        m_strip->clear();
        for (qsizetype i = 0; i < composed.size(); ++i) {
            new QListWidgetItem(m_strip);
        }
    }
    for (int row = 0; row < m_strip->count(); ++row) {
        const qsizetype index = composed.at(row);
        const core::CardPortrait& portrait = m_subject.portraits.at(index);
        //: The name of a portrait of a card without a label; %1: its number. In Polish:
        //: Portret %1
        const QString name =
            portrait.label.isEmpty() ? tr("Portrait %1").arg(index + 1) : portrait.label;
        const QString text = index == 0 ? QStringLiteral("\u2605 ") + name : name;
        QListWidgetItem* item = m_strip->item(row);
        item->setIcon(QIcon(drawn(portrait.avatar, AvatarFrame::Bust, STRIP_SIDE, false)));
        item->setText(m_strip->fontMetrics().elidedText(text, Qt::ElideRight, STRIP_WIDTH - 4));
        //: The main portrait of a card, shown on the card. In Polish: %1 (główny, na karcie)
        item->setToolTip(index == 0 ? tr("%1 (the main one, on the card)").arg(name) : name);
        item->setData(Qt::UserRole, static_cast<qlonglong>(index));
        if (index == m_subject.current) {
            m_strip->setCurrentItem(item);
        }
    }
}

QPixmap PortraitDialog::drawn(const QJsonObject& avatar, AvatarFrame frame, int side,
                              bool withSign) const {
    core::CardPortrait portrait = shown();
    portrait.avatar = avatar;
    if (!withSign) {
        portrait.avatar.insert(QLatin1String(Avatar::SIGN), false);
    }
    const qreal ratio = devicePixelRatioF();
    const int pixels = qRound(side * ratio);
    const QRectF view = AvatarArt::drawn().view(frame);
    const int width = frame == AvatarFrame::Full && view.height() > 0
                          ? qRound(pixels * view.width() / view.height())
                          : pixels;
    QPixmap pixmap = QPixmap::fromImage(Avatar::imageOf(
        portrait, frame, m_subject.color, m_subject.gender, QSize(std::max(1, width), pixels)));
    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

void PortraitDialog::changeEvent(QEvent* event) {
    KalahariDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_options != nullptr &&
        m_options->widget() != nullptr) {
        buildOptions();
        changed();
    }
}

}  // namespace kalahari::gui::dialogs
