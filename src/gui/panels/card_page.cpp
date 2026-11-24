// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_page.cpp
/// @brief The card of a character, a location or an item of the Workshop

#include "kalahari/gui/panels/card_page.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/card_fields_dialog.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/card_chapters_box.h"
#include "kalahari/gui/widgets/card_name_box.h"
#include "kalahari/gui/widgets/own_fields_box.h"
#include "kalahari/gui/widgets/palette_edit.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringListModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>

namespace kalahari::gui {

namespace {

/// Room between the parts of a card, and around them
constexpr int SPACING = 11;
/// Room between the lines of a group
constexpr int LINE_SPACING = 6;
/// Room between the portrait and the name
constexpr int PORTRAIT_SPACING = 16;
/// Room around the data of the full card, under its bar
constexpr int FULL_MARGIN = 16;
/// Room around the bar of the full card
constexpr int BAR_MARGIN = 4;
/// Side of the gear at the title of the group of the short card
constexpr int TITLE_BUTTON = 16;
/// Width of the column of the data of the full card, beside the long description
constexpr int DATA_MINIMUM_WIDTH = 280;
constexpr int DATA_MAXIMUM_WIDTH = 380;
/// Height the long description has at least: on a small screen the card scrolls
constexpr int DESCRIPTION_MINIMUM_HEIGHT = 320;
/// Lines of In short and of a field with a longer text
constexpr int TEXT_LINES = 3;
/// How much larger than the text the name of the full card is
constexpr qreal NAME_SCALE = 1.5;
/// Part of the side of a portrait that the height of the initials takes
constexpr qreal INITIALS_SCALE = 0.36;
/// Part of the side of a portrait that the icon of the kind takes
constexpr qreal ICON_SCALE = 0.56;
/// The picture of a portrait is read at most this many times as large as the portrait
constexpr int PICTURE_SCALE = 4;
/// Of the height a picture has over its width, the part above the portrait: a face is near
/// the top of a picture of a person
constexpr int ABOVE_FACE = 4;
/// Side of the swatch of the color of the card
constexpr int SWATCH_SIDE = 16;

/// Height of @p lines lines of @p edit, with its frame
int linesHeight(const QPlainTextEdit* edit, int lines) {
    const auto margin = static_cast<int>(edit->document()->documentMargin());
    return (edit->fontMetrics().lineSpacing() * lines) + (2 * margin) + (2 * edit->frameWidth());
}

/// Nothing of the children of @p widget reaches it any more. QWidget's destructor deletes them
/// after the class of @p widget is gone, and a field with the keys finishes its editing then.
void cutOffChildren(QWidget* widget) {
    for (QObject* child : widget->findChildren<QObject*>()) {
        child->disconnect(widget);
    }
}

/// The kind of the pictures of the materials, which a portrait from a file becomes
core::KindRef pictureKind(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return {};
    }
    for (const core::KindRef& kind :
         project->kindsIn(book.bookTypes(), core::BookPlace::Workshop)) {
        if (kind.kind->isMaterial() &&
            kind.kind->id == QLatin1String(core::MaterialKinds::PICTURE) &&
            core::ProjectManager::canAdd(kind)) {
            return kind;
        }
    }
    return {};
}

/// The initials of @p title on @p color, @p pixels on a side; in the text or the base color
/// of the theme, whichever stands out more
/// The color that stands out more on @p background: the text or the base color of the theme
QColor inkOn(const QColor& background) {
    const QPalette palette = QApplication::palette();
    const QColor light = palette.color(QPalette::Base);
    const QColor dark = palette.color(QPalette::Text);
    return core::contrastRatio(light, background) >= core::contrastRatio(dark, background)
               ? light
               : dark;
}

/// The color of a portrait without a picture: that of the card, else a grey of the theme
QColor backgroundOf(const QColor& color) {
    return color.isValid() ? color : QApplication::palette().color(QPalette::Mid);
}

QPixmap initialsPortrait(const QString& title, const QColor& color, int pixels) {
    const QColor background = backgroundOf(color);
    QPixmap pixmap(pixels, pixels);
    pixmap.fill(background);
    const QString initials = CardPage::initialsOf(title);
    if (initials.isEmpty()) {
        return pixmap;
    }
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPixelSize(std::max(1, qRound(pixels * INITIALS_SCALE)));
    painter.setFont(font);
    painter.setPen(inkOn(background));
    painter.drawText(QRect(0, 0, pixels, pixels), Qt::AlignCenter, initials);
    return pixmap;
}

/// The icon @p iconId of the kind of a card on @p color, @p pixels on a side, in one color: the
/// text or the base color of the theme, whichever stands out more
QPixmap iconPortrait(const QString& iconId, const QColor& color, int pixels) {
    const QColor background = backgroundOf(color);
    QPixmap pixmap(pixels, pixels);
    pixmap.fill(background);
    const int side = std::max(1, qRound(pixels * ICON_SCALE));
    const QPixmap icon = core::ArtProvider::getInstance()
                             .getIcon(iconId, core::IconContext::Toolbar)
                             .pixmap(QSize(side, side));
    if (icon.isNull()) {
        return pixmap;
    }
    // The shape of the icon, in the ink
    QImage glyph = icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    glyph.setDevicePixelRatio(1.0);
    QPainter tint(&glyph);
    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tint.fillRect(glyph.rect(), inkOn(background));
    tint.end();
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect target((pixels - side) / 2, (pixels - side) / 2, side, side);
    painter.drawImage(target, glyph);
    return pixmap;
}

/// @p square in a circle, the corners clear
QPixmap circleOf(const QPixmap& square) {
    QPixmap round(square.size());
    round.fill(Qt::transparent);
    QPainter painter(&round);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath circle;
    circle.addEllipse(QRectF(QPointF(0, 0), QSizeF(square.size())));
    painter.setClipPath(circle);
    painter.drawPixmap(0, 0, square);
    return round;
}

/// Give @p combo the values of @p field to choose from, after an empty one, with @p value
/// chosen; a value the field does not have is kept, shown as it is saved
void fillChoices(QComboBox* combo, const core::CardField* field, const QString& value) {
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(QString(), QString());
    if (field != nullptr) {
        for (const core::CardFieldChoice& choice : field->choices) {
            combo->addItem(choice.name.text(programLanguage()), choice.id);
        }
    }
    int index = combo->findData(value);
    if (index < 0) {
        combo->addItem(value, value);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}

/// A title for a menu: each '&' doubled, so that it is not taken for an access key
QString menuText(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

/// Show @p value in @p editor, the control of @p field, unless it has the keys
void showValue(QWidget* editor, const core::CardField* field, const QString& value,
               bool readOnly) {
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        fillChoices(combo, field, value);
        combo->setEnabled(!readOnly);
        return;
    }
    if (auto* lines = qobject_cast<QPlainTextEdit*>(editor)) {
        if (!lines->hasFocus() && lines->toPlainText() != value) {
            lines->setPlainText(value);
        }
        lines->setReadOnly(readOnly);
        return;
    }
    if (auto* line = qobject_cast<QLineEdit*>(editor)) {
        if (!line->hasFocus() && line->text() != value) {
            line->setText(value);
        }
        line->setReadOnly(readOnly);
    }
}

/// A button of the bar of the full card: @p text beside icon @p iconId, with the object name
/// @p name
QToolButton* barButton(QWidget* bar, const QString& text, const QString& iconId,
                       const QString& name) {
    auto* button = new QToolButton(bar);
    button->setObjectName(name);
    button->setText(text);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    card::keepIcon(button, iconId, core::IconContext::Toolbar);
    return button;
}

/// Whether @p event opens the menu of the portrait: Enter, or the menu key (a right click)
bool opensMenu(const QEvent* event) {
    return (event->type() == QEvent::KeyPress &&
            card::isOpenKey(static_cast<const QKeyEvent*>(event))) ||
           event->type() == QEvent::ContextMenu;
}

/// Portrait @p index of @p element, or its main one for an index it does not have; nullptr:
/// none
const core::CardPortrait* portraitAt(const core::ProjectElement& element, qsizetype index) {
    const QList<core::CardPortrait>& portraits = element.card.portraits;
    return index >= 0 && index < portraits.size() ? &portraits.at(index)
                                                  : element.card.mainPortrait();
}

/// Picture @p imageId of book @p book as a square of @p pixels across its middle, near the top,
/// where a face usually is; null without a picture
QPixmap pictureSquare(const core::ProjectManager& book, const QString& imageId, int pixels) {
    const core::ProjectElement* image = imageId.isEmpty() ? nullptr : book.findElement(imageId);
    if (image == nullptr || image->file.isEmpty()) {
        return {};
    }
    const QImage picture = readPicture(book.filePathOf(*image), image->rotation,
                                       QSize(pixels, pixels) * PICTURE_SCALE);
    if (picture.isNull()) {
        return {};
    }
    const int square = std::min(picture.width(), picture.height());
    const int left = (picture.width() - square) / 2;
    const int top = (picture.height() - square) / ABOVE_FACE;
    return QPixmap::fromImage(picture.copy(left, top, square, square)
                                  .scaled(pixels, pixels, Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation));
}

/// What portrait @p shown of @p element in book @p book looks like at @p size, to draw it again
/// only after a change: the name, the color and the gender of the card, the parts of a portrait
/// of parts, the picture and when its file changed
QString portraitKey(const core::ProjectManager& book, const core::ProjectElement& element,
                    const core::CardPortrait* shown, const QString& size) {
    QString key = element.title + QLatin1Char('|') + element.card.color + QLatin1Char('|') +
                  size + QLatin1Char('|') + element.card.value(QStringLiteral("gender"));
    if (shown == nullptr) {
        return key;
    }
    if (shown->isComposed()) {
        key += QLatin1Char('|') + shown->style + QLatin1Char('|') +
               QString::fromUtf8(QJsonDocument(shown->avatar).toJson(QJsonDocument::Compact));
    }
    if (const core::ProjectElement* image =
            shown->image.isEmpty() ? nullptr : book.findElement(shown->image)) {
        const QString path = book.filePathOf(*image);
        key += QLatin1Char('|') + path + QLatin1Char('|') + QString::number(image->rotation) +
               QLatin1Char('|') +
               QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
    }
    return key;
}

}  // namespace

CardPage::CardPage(core::ProjectManager& book, bool full, QWidget* parent)
    : QWidget(parent),
      m_full(full),
      m_portrait(new QToolButton(this)),
      m_portraitMenu(new QMenu(m_portrait)),
      m_nameEdit(new QLineEdit(this)),
      m_settingsButton(new QToolButton(this)),
      m_shortEdit(new QPlainTextEdit(this)),
      m_form(new QFormLayout()),
      m_book(book) {
    setObjectName(full ? QStringLiteral("cardFullCard") : QStringLiteral("cardPage"));

    // The gear first, at the top of the card; then the portrait, a picture whose menu opens
    // on a click, Space, Enter or the menu key
    m_settingsButton->setObjectName(QStringLiteral("cardSettingsButton"));
    m_settingsButton->setAutoRaise(true);
    //: The gear of a card of a character, a location or an item: Settings > Cards. In Polish:
    //: Ustawienia kart
    m_settingsButton->setToolTip(tr("Card Settings"));
    m_settingsButton->setAccessibleName(m_settingsButton->toolTip());
    connect(m_settingsButton, &QToolButton::clicked, this, &CardPage::requestSettings);

    m_portrait->setObjectName(QStringLiteral("cardPortrait"));
    m_portrait->setAutoRaise(true);
    m_portrait->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_portrait->setPopupMode(QToolButton::InstantPopup);
    m_portrait->setStyleSheet(
        QStringLiteral("QToolButton#cardPortrait { padding: 1px; }"
                       "QToolButton#cardPortrait::menu-indicator { image: none; width: 0px; }"));
    const int side = full ? FULL_PORTRAIT : SHORT_PORTRAIT;
    m_portrait->setIconSize(QSize(side, side));
    m_portrait->setCursor(Qt::PointingHandCursor);
    //: The portrait of a card of a character, a location or an item. In Polish: Portret
    m_portrait->setAccessibleName(tr("Portrait"));
    //: The tooltip of the portrait of a card. In Polish: Zmień portret
    m_portrait->setToolTip(tr("Change the Portrait"));
    m_portrait->installEventFilter(this);
    m_portraitMenu->setObjectName(QStringLiteral("cardPortraitMenu"));
    m_portrait->setMenu(m_portraitMenu);
    connect(m_portraitMenu, &QMenu::aboutToShow, this, &CardPage::fillPortraitMenu);

    m_nameEdit->setObjectName(QStringLiteral("cardNameEdit"));
    //: The name of a character, a location or an item on its card. In Polish: Nazwa
    m_nameEdit->setAccessibleName(tr("Name"));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &CardPage::saveName);

    // In short: who the character is, what the location is; saved when it loses the keys
    m_shortEdit->setObjectName(QStringLiteral("cardShortEdit"));
    //: The short description of a card, for screen readers. In Polish: W skrócie
    m_shortEdit->setAccessibleName(tr("In Short"));
    //: In Polish: Kim lub czym jest – w jednym lub dwóch zdaniach...
    m_shortEdit->setPlaceholderText(tr("Who or what it is, in a sentence or two..."));
    m_shortEdit->setTabChangesFocus(true);
    m_shortEdit->setFixedHeight(linesHeight(m_shortEdit, TEXT_LINES));
    m_shortEdit->installEventFilter(this);

    m_form->setContentsMargins(0, 0, 0, 0);
    m_form->setSpacing(LINE_SPACING);
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // The relations with the other cards; the other card shows on the short card in the
    // Navigator, on the full card as its full card
    m_relations = new RelationsBox(m_book, full, this);
    connect(m_relations, &RelationsBox::requestShowCard, this,
            full ? &CardPage::requestOpenCard : &CardPage::requestSelectCard);
    connect(m_relations, &RelationsBox::cardAdded, this, &CardPage::cardAdded);
    connect(m_relations, &RelationsBox::relationAdded, this, &CardPage::relationAdded);
    // The declension of the name, as the book's dictionary has it
    m_nameBox = new CardNameBox(m_book, full, this);
    connect(m_nameBox, &CardNameBox::requestNameWindow, this, &CardPage::requestNameWindow);
    connect(m_nameBox, &CardNameBox::requestOtherName, this, &CardPage::requestOtherName);

    if (full) {
        createFullLayout();
    } else {
        createShortLayout();
    }
    // Files, web addresses and pictures dropped on it are its materials
    setAcceptDrops(true);

    // A change made elsewhere: in the other card, the Navigator, when the picture of the
    // portrait goes
    connect(&m_book, &core::ProjectManager::cardChanged, this, [this](const QString& elementId) {
        if (elementId == m_elementId) {
            refresh();
        }
    });
    connect(&m_book, &core::ProjectManager::elementRenamed, this,
            [this](const QString& elementId) {
                if (elementId == m_elementId) {
                    refresh();
                }
            });
    // New fields of the cards of its kind (the window Card Fields): the form anew
    connect(&m_book, &core::ProjectManager::cardFieldsChanged, this, [this](const QString& kind) {
        if (kind == m_kindKey) {
            m_kindKey.clear();
            refresh();
        }
    });
    // The icons, and the colors of the theme the initials and the swatch are drawn in
    const auto redraw = [this]() {
        m_portraitKey.clear();
        refresh();
    };
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            redraw);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this, redraw);
    // The layout and the portraits follow Settings > Cards; the listener runs on the thread
    // that changes them, the card changes on its own
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == LAYOUT_SETTING || isPortraitSetting(key)) {
                QMetaObject::invokeMethod(
                    this, [this, key]() { takeSetting(key); }, Qt::QueuedConnection);
            }
        });
}

CardPage::~CardPage() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    cutOffChildren(this);
}

void CardPage::createFullLayout() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The bar of the card stays in its place while the data scroll
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("cardBar"));
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(BAR_MARGIN * 2, BAR_MARGIN, BAR_MARGIN * 2, BAR_MARGIN);
    //: The button of the bar of a full card that searches the texts of the book for the name of
    //: the card. In Polish: Znajdź w książce
    m_findButton = barButton(bar, tr("Find in Book"), QStringLiteral("edit.findInBook"),
                             QStringLiteral("cardFindButton"));
    //: In Polish: Szuka nazwy karty we wszystkich tekstach książki, w każdej formie i pod
    //: innymi imionami (panel „Wyszukiwanie”)
    m_findButton->setToolTip(tr("Searches all the texts of the book for the name of the card, "
                                "in all its forms and by its other names (the Search panel)"));
    connect(m_findButton, &QToolButton::clicked, this,
            [this]() { emit requestFindInBook(m_elementId); });
    barLayout->addWidget(m_findButton);
    auto* barLine = new QFrame(bar);
    barLine->setFrameShape(QFrame::VLine);
    barLine->setFrameShadow(QFrame::Sunken);
    barLayout->addWidget(barLine);
    //: The button of the bar of a full card that opens the window Relation. In Polish:
    //: Relacja...
    m_relationButton = barButton(bar, tr("Relation..."), QStringLiteral("common.link"),
                                 QStringLiteral("cardRelationButton"));
    //: In Polish: Nowa relacja z inną kartą
    m_relationButton->setToolTip(tr("A new relation with another card"));
    connect(m_relationButton, &QToolButton::clicked, m_relations, &RelationsBox::addRelation);
    barLayout->addWidget(m_relationButton);
    //: The button of the bar of a full card that opens the window Card Fields. In Polish:
    //: Pola...
    m_fieldsButton = barButton(bar, tr("Fields..."), QStringLiteral("card.fields"),
                               QStringLiteral("cardFieldsButton"));
    //: In Polish: Pola kart tego rodzaju: kolejność i to, gdzie widać każde pole
    m_fieldsButton->setToolTip(
        tr("The fields of the cards of this kind: their order and where each is shown"));
    connect(m_fieldsButton, &QToolButton::clicked, this, &CardPage::editFields);
    barLayout->addWidget(m_fieldsButton);
    barLayout->addStretch(1);
    barLayout->addWidget(m_settingsButton);
    card::keepIcon(m_settingsButton, QStringLiteral("dock.settings"),
                         core::IconContext::Toolbar);
    layout->addWidget(bar);
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("cardScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(FULL_MARGIN, LINE_SPACING, FULL_MARGIN, FULL_MARGIN);
    contentLayout->setSpacing(SPACING);

    // The header: the portrait beside the name, the kind with the field beside it and the
    // color, In short
    auto* header = new QHBoxLayout();
    header->setSpacing(PORTRAIT_SPACING);
    auto* portraitColumn = new QVBoxLayout();
    portraitColumn->setSpacing(0);
    portraitColumn->addWidget(m_portrait, 0, Qt::AlignHCenter);
    portraitColumn->addWidget(portraitArrows(content), 0, Qt::AlignHCenter);
    portraitColumn->addStretch(1);
    header->addLayout(portraitColumn);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(LINE_SPACING);
    fitNameFont();
    titles->addWidget(m_nameEdit);
    auto* kindRow = new QHBoxLayout();
    kindRow->setSpacing(LINE_SPACING);
    m_kindIcon = new QLabel(content);
    m_kindIcon->setObjectName(QStringLiteral("cardKindIcon"));
    m_subtitleLabel = new QLabel(content);
    m_subtitleLabel->setObjectName(QStringLiteral("cardSubtitleLabel"));
    m_subtitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    //: The color of a card of a character, a location or an item, which the Navigator shows
    //: beside its name. In Polish: Kolor karty:
    auto* colorCaption = new QLabel(tr("Card color:"), content);
    m_colorButton = new QToolButton(content);
    m_colorButton->setObjectName(QStringLiteral("cardColorButton"));
    m_colorButton->setAutoRaise(true);
    //: In Polish: Zmień kolor karty...
    m_colorButton->setToolTip(tr("Change the Card Color..."));
    //: In Polish: Kolor karty
    m_colorButton->setAccessibleName(tr("Card color"));
    connect(m_colorButton, &QToolButton::clicked, this, &CardPage::chooseColor);
    kindRow->addWidget(m_kindIcon);
    kindRow->addWidget(m_subtitleLabel);
    kindRow->addSpacing(SPACING);
    kindRow->addWidget(colorCaption);
    kindRow->addWidget(m_colorButton);
    kindRow->addStretch(1);
    titles->addLayout(kindRow);
    //: The caption of the short description of a card: who the character is, what the
    //: location is. In Polish: W skrócie:
    auto* shortCaption = new QLabel(tr("In short:"), content);
    shortCaption->setBuddy(m_shortEdit);
    titles->addWidget(shortCaption);
    titles->addWidget(m_shortEdit);
    header->addLayout(titles, 1);
    contentLayout->addLayout(header);

    // Under the header: the data beside the long description, or on tabs
    m_body = new QWidget(content);
    m_body->setObjectName(QStringLiteral("cardBody"));
    auto* dataColumn = new QWidget(m_body);
    m_dataColumn = dataColumn;
    dataColumn->setMinimumWidth(DATA_MINIMUM_WIDTH);
    auto* dataLayout = new QVBoxLayout(dataColumn);
    dataLayout->setContentsMargins(0, 0, 0, 0);
    dataLayout->setSpacing(SPACING);
    //: The group of the fields of a card of a character, a location or an item. In Polish:
    //: Dane
    auto* fieldsGroup = new QGroupBox(tr("Data"), dataColumn);
    fieldsGroup->setObjectName(QStringLiteral("cardDataGroup"));
    auto* fieldsLayout = new QVBoxLayout(fieldsGroup);
    fieldsLayout->setContentsMargins(SPACING, SPACING, SPACING, SPACING);
    fieldsLayout->addLayout(m_form);
    m_fieldsBox = fieldsGroup;
    dataLayout->addWidget(fieldsGroup);
    dataLayout->addWidget(m_nameBox);
    //: In Polish: Własne pola karty
    m_ownFields = new OwnFieldsBox(m_book, QStringLiteral("card"), tr("Own fields of the card"),
                                   false, dataColumn);
    m_ownFields->setAddToKind(true);
    dataLayout->addWidget(m_ownFields);
    dataLayout->addStretch(1);
    //: The long description of a card of a character, a location or an item: the text of the
    //: card. In Polish: Opis
    m_descriptionGroup = new QGroupBox(tr("Description"), m_body);
    m_descriptionGroup->setObjectName(QStringLiteral("cardDescriptionGroup"));
    m_descriptionGroup->setMinimumHeight(DESCRIPTION_MINIMUM_HEIGHT);
    auto* descriptionLayout = new QVBoxLayout(m_descriptionGroup);
    descriptionLayout->setContentsMargins(SPACING, SPACING, SPACING, SPACING);
    // Beside the data: the long description, then the relations, the chapters and the
    // materials of the card
    m_chapters = new CardChaptersBox(m_book, m_body);
    connect(m_chapters, &CardChaptersBox::requestOpenChapter, this,
            &CardPage::requestOpenChapter);
    m_materials = new LinkedMaterialsBox(m_book, m_body);
    connect(m_materials, &LinkedMaterialsBox::requestOpenMaterial, this,
            &CardPage::requestOpenMaterial);
    connect(m_materials, &LinkedMaterialsBox::menuOpening, this, &CardPage::addPortraitAction);
    m_relations->setParent(m_body);
    placeBody();
    contentLayout->addWidget(m_body, 1);

    scroll->setWidget(content);
    layout->addWidget(scroll, 1);
}

void CardPage::placeBody() {
    const bool tabs = hasTabs();
    if (m_body == nullptr || (m_body->layout() != nullptr && tabs == m_hasTabs)) {
        return;
    }
    m_hasTabs = tabs;
    // The parts out of the layout they had, as they were shown
    const QList<QWidget*> parts{m_dataColumn, m_descriptionGroup, m_relations, m_chapters,
                                m_materials};
    QList<bool> hidden;
    for (QWidget* part : parts) {
        hidden << part->isHidden();
        part->setParent(m_body);
    }
    delete m_body->layout();
    delete m_tabs;
    m_tabs = nullptr;
    if (tabs) {
        placeOnTabs();
    } else {
        placeInColumns();
    }
    for (qsizetype i = 0; i < parts.size(); ++i) {
        parts.at(i)->setHidden(hidden.at(i));
    }
}

void CardPage::placeOnTabs() {
    auto* layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(m_body);
    m_tabs->setObjectName(QStringLiteral("cardTabs"));
    m_tabs->setDocumentMode(true);
    const auto page = [this](const QList<QWidget*>& widgets, bool stretch) {
        auto* each = new QWidget(m_tabs);
        auto* pageLayout = new QVBoxLayout(each);
        pageLayout->setContentsMargins(0, SPACING, 0, 0);
        pageLayout->setSpacing(SPACING);
        for (QWidget* widget : widgets) {
            pageLayout->addWidget(widget, stretch ? 0 : 1);
        }
        if (stretch) {
            pageLayout->addStretch(1);
        }
        return each;
    };
    //: A tab of the full card: the data, the declension and the own fields. In Polish:
    //: &Ogólne
    const QString general = tr("&General");
    //: A tab of the full card: the long description. In Polish: O&pis
    const QString description = tr("&Description");
    //: A tab of the full card: the relations with the other cards. In Polish: &Relacje
    const QString relations = tr("&Relations");
    //: A tab of the full card: the chapters the card is in and its materials. In Polish:
    //: Rozdziały i &materiały
    const QString chapters = tr("Chapters and &Materials");
    m_tabs->addTab(page({m_dataColumn}, false), general);
    m_tabs->addTab(page({m_descriptionGroup}, false), description);
    m_tabs->addTab(page({m_relations}, true), relations);
    m_tabs->addTab(page({m_chapters, m_materials}, true), chapters);
    layout->addWidget(m_tabs);
    m_dataColumn->setMaximumWidth(QWIDGETSIZE_MAX);
}

void CardPage::placeInColumns() {
    // The data beside the long description, then the relations, the chapters and the
    // materials of the card
    auto* columns = new QHBoxLayout(m_body);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(SPACING);
    columns->addWidget(m_dataColumn);
    auto* side = new QVBoxLayout();
    side->setSpacing(SPACING);
    side->addWidget(m_descriptionGroup, 1);
    side->addWidget(m_relations);
    side->addWidget(m_chapters);
    side->addWidget(m_materials);
    columns->addLayout(side, 1);
    m_dataColumn->setMaximumWidth(DATA_MAXIMUM_WIDTH);
}

void CardPage::takeSetting(const std::string& key) {
    if (key == LAYOUT_SETTING) {
        placeBody();
    } else if (isPortraitSetting(key)) {
        m_portraitKey.clear();
        refresh();
    }
}

bool CardPage::isPortraitSetting(const std::string& key) {
    return key == SHAPE_SETTING || key == NO_PICTURE_SETTING;
}

bool CardPage::hasTabs() {
    return core::SettingsManager::getInstance().get<std::string>(LAYOUT_SETTING, LAYOUT_COLUMNS) ==
           LAYOUT_TABS;
}

bool CardPage::roundPortraits() {
    return core::SettingsManager::getInstance().get<std::string>(SHAPE_SETTING, SHAPE_SQUARE) ==
           SHAPE_CIRCLE;
}

bool CardPage::kindIconWithoutPicture() {
    return core::SettingsManager::getInstance().get<std::string>(
               NO_PICTURE_SETTING, NO_PICTURE_INITIALS) == NO_PICTURE_ICON;
}

void CardPage::createShortLayout() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(SPACING);

    m_infoGroup = new QGroupBox(this);
    m_infoGroup->setObjectName(QStringLiteral("cardInfoGroup"));
    auto* info = new QVBoxLayout(m_infoGroup);
    info->setContentsMargins(SPACING, SPACING, SPACING, SPACING);
    info->setSpacing(LINE_SPACING);
    // The gear at the title of the group
    m_settingsButton->setParent(m_infoGroup);
    m_settingsButton->setFixedSize(TITLE_BUTTON, TITLE_BUTTON);
    card::keepIcon(m_settingsButton, QStringLiteral("dock.settings"),
                         core::IconContext::Menu);
    m_infoGroup->installEventFilter(this);

    // The portrait beside the name and the field beside the kind (the role of a character)
    auto* header = new QHBoxLayout();
    header->setSpacing(SPACING);
    header->addWidget(m_portrait, 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(LINE_SPACING);
    titles->addWidget(m_nameEdit);
    m_subtitleLayout = new QVBoxLayout();
    m_subtitleLayout->setContentsMargins(0, 0, 0, 0);
    titles->addLayout(m_subtitleLayout);
    titles->addStretch(1);
    header->addLayout(titles, 1);
    info->addLayout(header);

    auto* shortCaption = new QLabel(tr("In short:"), m_infoGroup);
    shortCaption->setBuddy(m_shortEdit);
    info->addWidget(shortCaption);
    info->addWidget(m_shortEdit);

    // The fields the writer wants in the Properties panel
    m_fieldsBox = new QWidget(m_infoGroup);
    auto* fieldsLayout = new QVBoxLayout(m_fieldsBox);
    fieldsLayout->setContentsMargins(0, 0, 0, 0);
    fieldsLayout->addLayout(m_form);
    info->addWidget(m_fieldsBox);
    layout->addWidget(m_infoGroup);
    layout->addWidget(m_nameBox);
    layout->addWidget(m_relations);

    //: The button of the short card of a character, a location or an item in the Properties
    //: panel, which opens its full card. In Polish: Otwórz kartę
    m_openButton = new QPushButton(tr("Open Card"), this);
    m_openButton->setObjectName(QStringLiteral("cardOpenButton"));
    card::keepIcon(m_openButton, QStringLiteral("file.open"), core::IconContext::Button);
    //: In Polish: Otwórz pełną kartę obok rozdziałów
    m_openButton->setToolTip(tr("Open the full card beside the chapters"));
    connect(m_openButton, &QPushButton::clicked, this,
            [this]() { emit requestOpenCard(m_elementId); });
    auto* openRow = new QHBoxLayout();
    openRow->addWidget(m_openButton);
    openRow->addStretch(1);
    layout->addLayout(openRow);
    layout->addStretch(1);
}

void CardPage::showCard(const QString& elementId) {
    if (elementId != m_elementId) {
        m_shownPortrait = 0;
    }
    m_elementId = elementId;
    m_portraitKey.clear();
    m_relations->showCard(elementId);
    m_nameBox->showCard(elementId);
    if (m_full) {
        m_chapters->showCard(elementId);
        //: The materials of a card without any. In Polish: Z kartą nie są powiązane żadne
        //: materiały. „+” albo Insert je powiąże, a plik albo obraz upuszczony na kartę doda
        //: nowy materiał.
        m_materials->showLinked(elementId,
                                tr("No materials are linked with the card. \"+\" or Insert "
                                   "links them; a file or a picture dropped on the card adds "
                                   "one."));
    }
    refresh();
}

void CardPage::refresh() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const core::KindRef kind = element != nullptr ? m_book.kindOf(*element) : core::KindRef{};
    if (!kind || !kind.kind->isCard()) {
        return;
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    if (const QString kindKey = kind.reference(); kindKey != m_kindKey) {
        buildFields(kind);
        m_kindKey = kindKey;
    }
    showHeader(*element);
    fillFields(*element);
    showPortrait(*element);
    if (m_ownFields != nullptr) {
        m_ownFields->showFields(m_elementId, element->fields);
    }
    m_updating = wasUpdating;
}

void CardPage::showHeader(const core::ProjectElement& element) {
    const bool readOnly = m_book.isReadOnly();
    if (!m_nameEdit->hasFocus() && m_nameEdit->text() != element.title) {
        m_nameEdit->setText(element.title);
    }
    m_nameEdit->setReadOnly(readOnly);
    if (!m_shortEdit->hasFocus() && m_shortEdit->toPlainText() != element.description) {
        m_shortEdit->setPlainText(element.description);
    }
    m_shortEdit->setReadOnly(readOnly);
    if (m_full) {
        m_subtitleLabel->setText(subtitleOf(m_book, element));
        showKindIcon(element);
        showColor(element);
        m_colorButton->setEnabled(!readOnly);
        m_relationButton->setEnabled(!readOnly);
        m_fieldsButton->setEnabled(!readOnly);
        return;
    }
    const QHash<QString, core::KindWords> nouns{
        {QStringLiteral("kind"), wordsOf(m_book, element)}};
    //: The group of the short card of a character, a location or an item in the Properties
    //: panel. In Polish: Informacje o {kind:locative}
    m_infoGroup->setTitle(core::fillWords(tr("{Kind} Information"), nouns));
    card::placeAtTitle(m_infoGroup, m_settingsButton);
}

void CardPage::showKindIcon(const core::ProjectElement& element) {
    auto& art = core::ArtProvider::getInstance();
    const int size = art.getIconSize(core::IconContext::Menu);
    m_kindIcon->setPixmap(
        art.getIcon(NavigatorPanel::iconIdOf(m_book.bookTypes(), element),
                    core::IconContext::Menu)
            .pixmap(QSize(size, size), devicePixelRatioF()));
}

void CardPage::focusName() {
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->selectAll();
}

void CardPage::setDescriptionWidget(QWidget* description) {
    if (m_descriptionGroup == nullptr || description == nullptr) {
        return;
    }
    // In the frame of a text field, as the other fields of the card
    auto* frame = new QFrame(m_descriptionGroup);
    frame->setObjectName(QStringLiteral("descriptionFrame"));
    frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(description);
    m_descriptionGroup->layout()->addWidget(frame);
    m_description = description;
    // After the data, before the relations, as it stands
    setTabOrder(m_ownFields->table(), description);
    setTabOrder(description, m_relations->addButton());
}

void CardPage::buildFields(const core::KindRef& kind) {
    clearFields();
    QList<QWidget*> besides;
    QList<QWidget*> fields;
    for (const core::CardField& field : m_book.cardFields(kind)) {
        if (!(m_full ? field.onCard : field.inProperties)) {
            continue;
        }
        QWidget* editor = createEditor(field);
        m_editors.insert(field.id, editor);
        if (!m_full && field.subtitle) {
            // Right under the name, before In short
            m_subtitleLayout->addWidget(editor);
            besides << editor;
        } else {
            //: The caption of a field of a card: %1 its name ("Age"). In Polish: %1:
            m_form->addRow(tr("%1:").arg(field.name.text(programLanguage())), editor);
            fields << editor;
        }
    }
    m_fieldsBox->setVisible(m_form->rowCount() > 0);
    orderKeys(besides, fields);
}

void CardPage::clearFields() {
    m_editors.clear();
    m_fieldOf.clear();
    while (m_form->rowCount() > 0) {
        m_form->removeRow(0);
    }
    if (m_subtitleLayout == nullptr) {
        return;
    }
    for (QLayoutItem* item = m_subtitleLayout->takeAt(0); item != nullptr;
         item = m_subtitleLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void CardPage::editFields() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    // The form follows the fields saved (cardFieldsChanged())
    dialogs::editCardFields(m_book, m_kindKey, window());
}

void CardPage::orderKeys(const QList<QWidget*>& besides, const QList<QWidget*>& fields) {
    QList<QWidget*> chain;
    if (m_full) {
        chain << m_findButton << m_relationButton << m_fieldsButton;
    }
    chain << m_settingsButton << m_portrait << m_nameEdit;
    if (m_colorButton != nullptr) {
        chain << m_colorButton;
    }
    chain << besides << m_shortEdit << fields << m_nameBox->keyParts();
    if (m_ownFields != nullptr) {
        chain << m_ownFields->addButton() << m_ownFields->removeButton() << m_ownFields->table();
    }
    if (m_description != nullptr) {
        chain << m_description;
    }
    chain << m_relations->addButton() << m_relations->removeButton() << m_relations->list();
    if (m_full) {
        chain << m_chapters->addButton() << m_chapters->removeButton() << m_chapters->list()
              << m_materials->addButton() << m_materials->removeButton() << m_materials->list();
    }
    if (m_openButton != nullptr) {
        chain << m_openButton;
    }
    for (qsizetype i = 1; i < chain.size(); ++i) {
        setTabOrder(chain.at(i - 1), chain.at(i));
    }
}

QWidget* CardPage::createEditor(const core::CardField& field) {
    const QString fieldId = field.id;
    QWidget* editor = nullptr;
    switch (field.type) {
    case core::CardFieldType::Choice: {
        auto* combo = new QComboBox(this);
        connect(combo, &QComboBox::activated, this, [this, combo, fieldId]() {
            saveValue(fieldId, combo->currentData().toString());
        });
        editor = combo;
        break;
    }
    case core::CardFieldType::LongText: {
        auto* lines = new QPlainTextEdit(this);
        lines->setTabChangesFocus(true);
        lines->setFixedHeight(linesHeight(lines, TEXT_LINES));
        // Saved when it loses the keys
        lines->installEventFilter(this);
        m_fieldOf.insert(lines, fieldId);
        editor = lines;
        break;
    }
    case core::CardFieldType::Card: {
        auto* line = new QLineEdit(this);
        //: A field of a card that names another card (the owner of an item, the location a
        //: location lies in), when it is empty. In Polish: Wpisz albo wybierz kartę...
        line->setPlaceholderText(tr("Type or choose a card..."));
        auto* completer = new QCompleter(line);
        completer->setModel(new QStringListModel(completer));
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        line->setCompleter(completer);
        // A list of the cards at once: the arrow at its end, a click in it or Alt+Down show
        // them all, typing narrows them
        QAction* showAll = line->addAction(
            core::ArtProvider::getInstance().getIcon(QStringLiteral("common.expandMore")),
            QLineEdit::TrailingPosition);
        showAll->setObjectName(QStringLiteral("showAllCards"));
        //: The arrow at the end of a field of a card that names another card. In Polish:
        //: Pokaż wszystkie karty (Alt+↓)
        showAll->setToolTip(tr("Show all the cards (Alt+Down)"));
        connect(showAll, &QAction::triggered, this, [this, line]() { showAllCards(line); });
        // The names of the cards as they are when it gets the keys
        line->installEventFilter(this);
        m_fieldOf.insert(line, fieldId);
        // The name of a card: the relation with that card
        connect(line, &QLineEdit::editingFinished, this,
                [this, line, fieldId]() { saveCardText(fieldId, line->text().trimmed()); });
        editor = line;
        break;
    }
    case core::CardFieldType::Text:
        break;
    }
    if (editor == nullptr) {
        // A text: a line
        auto* line = new QLineEdit(this);
        connect(line, &QLineEdit::editingFinished, this,
                [this, line, fieldId]() { saveValue(fieldId, line->text().trimmed()); });
        editor = line;
    }
    editor->setObjectName(QStringLiteral("cardField_") + fieldId);
    editor->setAccessibleName(field.name.text(programLanguage()));
    return editor;
}

void CardPage::fillFields(const core::ProjectElement& element) {
    const core::KindRef kind = m_book.kindOf(element);
    const bool readOnly = m_book.isReadOnly();
    for (auto it = m_editors.cbegin(); it != m_editors.cend(); ++it) {
        showValue(it.value(), m_book.cardField(kind, it.key()),
                  m_book.cardFieldText(element.id, it.key()), readOnly);
    }
}

void CardPage::showPortrait(const core::ProjectElement& element) {
    const qreal ratio = devicePixelRatioF();
    const int side = m_full ? FULL_PORTRAIT : SHORT_PORTRAIT;
    const QList<core::CardPortrait>& portraits = element.card.portraits;
    if (m_shownPortrait < 0 || m_shownPortrait >= portraits.size()) {
        m_shownPortrait = 0;
    }
    showPortraitArrows(element);
    // Drawn again only after a change of what it shows, of its size or of the colors
    const QString key = portraitKey(
        m_book, element, portraits.isEmpty() ? nullptr : &portraits.at(m_shownPortrait),
        QString::number(side) + QLatin1Char('|') + QString::number(ratio) + QLatin1Char('|') +
            palette().color(QPalette::Base).name());
    if (key == m_portraitKey) {
        return;
    }
    m_portraitKey = key;
    m_portrait->setIcon(QIcon(portraitOf(m_book, element, side, ratio, m_shownPortrait)));
    m_portrait->setIconSize(QSize(side, side));
}

void CardPage::showColor(const core::ProjectElement& element) {
    if (m_colorButton == nullptr) {
        return;
    }
    m_colorButton->setIcon(QIcon(PaletteEdit::swatchOf(QColor(element.card.color), SWATCH_SIDE,
                                                       devicePixelRatioF(),
                                                       palette().color(QPalette::Mid))));
    m_colorButton->setIconSize(QSize(SWATCH_SIDE, SWATCH_SIDE));
    m_colorButton->setAccessibleDescription(element.card.color);
}

QPixmap CardPage::portraitOf(const core::ProjectManager& book, const core::ProjectElement& element,
                             int side, qreal pixelRatio, qsizetype index) {
    const int pixels = std::max(1, qRound(side * pixelRatio));
    QPixmap portrait;
    const core::CardPortrait* shown = portraitAt(element, index);
    if (shown != nullptr && shown->isComposed()) {
        portrait = QPixmap::fromImage(Avatar::imageOf(
            *shown, AvatarFrame::Bust, backgroundOf(QColor(element.card.color)),
            element.card.value(QStringLiteral("gender")), QSize(pixels, pixels)));
    } else if (shown != nullptr) {
        portrait = pictureSquare(book, shown->image, pixels);
    }
    if (portrait.isNull()) {
        const core::KindRef kind = book.kindOf(element);
        portrait = kindIconWithoutPicture() && kind
                       ? iconPortrait(kind.kind->icon, QColor(element.card.color), pixels)
                       : initialsPortrait(element.title, QColor(element.card.color), pixels);
    }
    if (roundPortraits()) {
        portrait = circleOf(portrait);
    }
    portrait.setDevicePixelRatio(pixelRatio);
    return portrait;
}

QString CardPage::initialsOf(const QString& title) {
    QString initials;
    const QStringList words = title.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& word : words) {
        const auto* const letter = std::find_if(word.cbegin(), word.cend(),
                                         [](QChar each) { return each.isLetterOrNumber(); });
        if (letter == word.cend()) {
            continue;
        }
        initials += letter->toUpper();
        if (initials.size() == 2) {
            break;
        }
    }
    return initials;
}

void CardPage::fillPortraitMenu() {
    // The submenu of the pictures goes with the menu's actions
    const QList<QMenu*> submenus =
        m_portraitMenu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
    m_portraitMenu->clear();
    qDeleteAll(submenus);
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    const bool readOnly = m_book.isReadOnly();
    const QList<core::CardPortrait>& portraits = element->card.portraits;
    const QString current =
        m_shownPortrait < portraits.size() ? portraits.at(m_shownPortrait).image : QString();
    auto& art = core::ArtProvider::getInstance();
    addComposeAction(*element);

    //: The submenu of the portrait of a card with the pictures of the materials of the book.
    //: In Polish: Obraz z &materiałów
    QMenu* pictures = m_portraitMenu->addMenu(
        art.getIcon(QStringLiteral("common.image"), core::IconContext::Menu),
        tr("Picture from the &Materials"));
    pictures->setObjectName(QStringLiteral("cardPortraitPictures"));
    fillPictures(pictures, current);
    pictures->setEnabled(!readOnly && !pictures->isEmpty());

    //: In Polish: Obraz z &pliku...
    QAction* file =
        m_portraitMenu->addAction(art.getIcon(QStringLiteral("file.open"), core::IconContext::Menu),
                                  tr("Picture from a &File..."));
    file->setObjectName(QStringLiteral("cardPortraitFile"));
    file->setEnabled(!readOnly && static_cast<bool>(pictureKind(m_book)));
    connect(file, &QAction::triggered, this, &CardPage::choosePortraitFile);
    addPortraitsMenu(*element);
    m_portraitMenu->addSeparator();
    //: Takes the portrait off the card; its picture stays in the materials. In Polish:
    //: &Usuń portret
    QAction* remove = m_portraitMenu->addAction(
        art.getIcon(QStringLiteral("common.remove"), core::IconContext::Menu),
        tr("&Remove Portrait"));
    remove->setObjectName(QStringLiteral("cardPortraitRemove"));
    remove->setEnabled(!readOnly && !portraits.isEmpty());
    connect(remove, &QAction::triggered, this, &CardPage::removePortrait);
}

void CardPage::fillPictures(QMenu* pictures, const QString& current) {
    QList<const core::ProjectElement*> linked;
    QList<const core::ProjectElement*> others;
    for (const core::ProjectElement* picture : m_book.pictureMaterials()) {
        (picture->linkTo(m_elementId) != nullptr ? linked : others) << picture;
    }
    const auto addPicture = [this, pictures, &current](const core::ProjectElement* picture) {
        QAction* action = pictures->addAction(
            menuText(picture->title.isEmpty() ? picture->file : picture->title));
        action->setCheckable(true);
        action->setChecked(picture->id == current);
        connect(action, &QAction::triggered, this,
                [this, imageId = picture->id]() { setPortrait(imageId); });
    };
    std::for_each(linked.cbegin(), linked.cend(), addPicture);
    if (!linked.isEmpty() && !others.isEmpty()) {
        pictures->addSeparator();
    }
    std::for_each(others.cbegin(), others.cend(), addPicture);
}

void CardPage::choosePortraitFile() {
    if (m_book.isReadOnly()) {
        return;
    }
    using Operation = ProgramFolders::Operation;
    const QString path = QFileDialog::getOpenFileName(
        this,
        //: The system window choosing the picture of the portrait of a card. In Polish:
        //: Wybierz obraz portretu
        tr("Choose the Picture of the Portrait"),
        ProgramFolders::windowFolder(Operation::AddMaterial),
        //: The pictures of the window choosing a portrait. In Polish: Obrazy
        editor::imageFileFilter(tr("Images")) + QStringLiteral(";;") +
            //: In Polish: Wszystkie pliki (*)
            tr("All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    ProgramFolders::remember(Operation::AddMaterial, QFileInfo(path).absolutePath());
    addPortraitFile(path);
}

void CardPage::setPortrait(const QString& imageId) {
    if (!linkWithCard(imageId)) {
        return;
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    // First of the portraits, with its label when the card had it already
    core::CardData card = element->card;
    core::CardPortrait portrait;
    portrait.image = imageId;
    const auto found = std::find_if(
        card.portraits.begin(), card.portraits.end(),
        [&imageId](const core::CardPortrait& each) { return each.image == imageId; });
    if (found != card.portraits.end()) {
        portrait = *found;
        card.portraits.erase(found);
    }
    card.portraits.prepend(portrait);
    saveCard(card);
}

bool CardPage::linkWithCard(const QString& imageId) {
    const core::ProjectElement* image = m_book.findElement(imageId);
    if (m_book.findElement(m_elementId) == nullptr || image == nullptr || m_book.isReadOnly() ||
        !m_book.isMaterial(*image)) {
        return false;
    }
    if (image->linkTo(m_elementId) != nullptr) {
        return true;
    }
    QStringList targets;
    for (const core::ElementLink& link : image->links) {
        targets << link.to;
    }
    targets << m_elementId;
    if (!m_book.setLinks(imageId, targets)) {
        //: In Polish: Nie zmieniono portretu
        dialogs::MessageDialog::warning(this, tr("Portrait Not Changed"),
                                        tr("Failed to save changes."));
        return false;
    }
    return true;
}

void CardPage::addPortraitFile(const QString& path) {
    if (m_elementId.isEmpty() || m_book.isReadOnly() || !pictureKind(m_book)) {
        return;
    }
    if (!MaterialFile::of(path).readable) {
        //: In Polish: Nie dodano portretu
        dialogs::MessageDialog::warning(
            this, tr("Portrait Not Added"),
            //: %1: the name of the file. In Polish: Plik „%1” nie jest obrazem, który Kalahari
            //: potrafi pokazać.
            tr("The file \"%1\" is not a picture Kalahari can show.")
                .arg(QFileInfo(path).fileName()));
        return;
    }
    core::NewMaterial material;
    material.title = QFileInfo(path).completeBaseName();
    material.filePath = path;
    addPortrait(material);
}

void CardPage::addPortrait(core::NewMaterial material) {
    const core::KindRef kind = pictureKind(m_book);
    if (m_elementId.isEmpty() || m_book.isReadOnly() || !kind) {
        return;
    }
    material.links = {m_elementId};
    const QString materialId = m_book.addMaterial(material, kind);
    if (materialId.isEmpty()) {
        dialogs::MessageDialog::warning(
            this, tr("Portrait Not Added"),
            //: In Polish: Nie udało się skopiować obrazu do materiałów książki.
            tr("The picture could not be copied into the materials of the book."));
        return;
    }
    emit materialAdded(materialId);
    setPortrait(materialId);
}

QString CardPage::addPicture(const QImage& image, const QString& title) {
    const core::KindRef kind = pictureKind(m_book);
    if (m_elementId.isEmpty() || m_book.isReadOnly() || !kind || image.isNull()) {
        return {};
    }
    core::NewMaterial material;
    material.title = title;
    material.fileName = IncomingMaterials::pictureFileName(QDateTime::currentDateTime());
    material.data = IncomingMaterials::pngOf(image);
    material.links = {m_elementId};
    QString materialId = m_book.addMaterial(material, kind);
    if (materialId.isEmpty()) {
        //: In Polish: Nie zapisano obrazu
        dialogs::MessageDialog::warning(
            this, tr("Picture Not Saved"),
            tr("The picture could not be copied into the materials of the book."));
        return {};
    }
    emit materialAdded(materialId);
    return materialId;
}

void CardPage::addPortraitAction(QMenu* menu, const QString& materialId) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const QList<const core::ProjectElement*> pictures = m_book.pictureMaterials();
    const bool picture = std::any_of(
        pictures.cbegin(), pictures.cend(),
        [&materialId](const core::ProjectElement* each) { return each->id == materialId; });
    if (element == nullptr || !picture) {
        return;
    }
    menu->addSeparator();
    //: In the menu of a picture linked with a card. In Polish: Ustaw jako &portret
    QAction* portrait = menu->addAction(tr("Set as &Portrait"));
    portrait->setObjectName(QStringLiteral("cardSetPortrait"));
    portrait->setEnabled(!m_book.isReadOnly() && element->card.portraitImage() != materialId);
    connect(portrait, &QAction::triggered, this,
            [this, materialId]() { setPortrait(materialId); });
}

bool CardPage::dropMaterials(const IncomingMaterials& incoming, const QPoint& at) {
    if (m_book.findElement(m_elementId) == nullptr || m_book.isReadOnly() ||
        !incoming.hasMaterials()) {
        return false;
    }
    // One picture on the portrait: the portrait
    if (m_portrait->rect().contains(m_portrait->mapFrom(this, at)) && dropPortrait(incoming)) {
        return true;
    }
    emit requestAddMaterials(incoming, m_elementId);
    return true;
}

bool CardPage::dropPortrait(const IncomingMaterials& incoming) {
    const qsizetype count = incoming.files.size() + incoming.addresses.size() +
                            (incoming.picture.isNull() ? 0 : 1);
    if (count != 1) {
        return false;
    }
    if (incoming.files.size() == 1 && core::MaterialKinds::ofFile(incoming.files.first()) ==
                                          QLatin1String(core::MaterialKinds::PICTURE)) {
        addPortraitFile(incoming.files.first());
        return true;
    }
    if (incoming.picture.isNull()) {
        return false;
    }
    const QDateTime now = QDateTime::currentDateTime();
    core::NewMaterial material;
    material.title = IncomingMaterials::pictureTitle(now);
    material.fileName = IncomingMaterials::pictureFileName(now);
    material.data = IncomingMaterials::pngOf(incoming.picture);
    addPortrait(material);
    return true;
}

void CardPage::dragEnterEvent(QDragEnterEvent* event) {
    if (m_book.findElement(m_elementId) != nullptr && !m_book.isReadOnly() &&
        IncomingMaterials::of(event->mimeData()).hasMaterials()) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void CardPage::dropEvent(QDropEvent* event) {
    if (dropMaterials(IncomingMaterials::of(event->mimeData()), event->position().toPoint())) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dropEvent(event);
}

void CardPage::removePortrait() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly() || element->card.portraits.isEmpty()) {
        return;
    }
    core::CardData card = element->card;
    card.portraits.removeAt(std::clamp<qsizetype>(m_shownPortrait, 0, card.portraits.size() - 1));
    m_shownPortrait = std::max<qsizetype>(0, m_shownPortrait - 1);
    saveCard(card);
}

void CardPage::chooseColor() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QColor current(element->card.color);
    //: What the color window chooses: %1 the name of a card. In Polish: Kolor karty „%1”
    const std::optional<QColor> chosen = dialogs::ColorDialog::getColor(
        current.isValid() ? current : QColor(m_book.newCardColor()), this,
        tr("The color of the card \"%1\"").arg(element->title));
    // The book may have changed while the window was open
    element = m_book.findElement(m_elementId);
    if (!chosen || !chosen->isValid() || element == nullptr) {
        return;
    }
    core::CardData card = element->card;
    card.color = chosen->name();
    if (card.color.compare(element->card.color, Qt::CaseInsensitive) != 0) {
        saveCard(card);
    }
}

void CardPage::saveCard(const core::CardData& card) {
    if (!m_book.setCard(m_elementId, card)) {
        //: In Polish: Nie zapisano karty
        dialogs::MessageDialog::warning(this, tr("Card Not Saved"), tr("Failed to save changes."));
    }
}

void CardPage::saveName() {
    if (m_updating || m_elementId.isEmpty()) {
        return;
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QString previous = element->title;
    const QString title = m_nameEdit->text().trimmed();
    if (title.isEmpty() || title == previous) {
        if (m_nameEdit->text() != previous) {
            m_nameEdit->setText(previous);
        }
        return;
    }
    if (!m_book.renameElement(m_elementId, title)) {
        m_nameEdit->setText(previous);
        //: In Polish: Nie udało się zmienić nazwy
        dialogs::MessageDialog::warning(this, tr("Rename Failed"), tr("Failed to save changes."));
        return;
    }
    emit titleChanged(m_elementId);
}

void CardPage::saveShort() {
    if (m_updating || m_elementId.isEmpty()) {
        return;
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const QString text = m_shortEdit->toPlainText();
    if (element == nullptr || m_book.isReadOnly() || element->description == text) {
        return;
    }
    if (!m_book.setDescription(m_elementId, text)) {
        //: In Polish: Nie zapisano opisu
        dialogs::MessageDialog::warning(this, tr("Description Not Saved"),
                                        tr("Failed to save changes."));
    }
}

void CardPage::saveValue(const QString& fieldId, const QString& value) {
    if (m_updating || m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || element->card.value(fieldId) == value) {
        return;
    }
    if (!m_book.setCardValue(m_elementId, fieldId, value)) {
        dialogs::MessageDialog::warning(this, tr("Card Not Saved"), tr("Failed to save changes."));
    }
}

void CardPage::saveCardText(const QString& fieldId, const QString& text) {
    if (m_updating || m_elementId.isEmpty() || m_book.isReadOnly() ||
        m_book.cardFieldText(m_elementId, fieldId) == text) {
        return;
    }
    if (!m_book.setCardFieldText(m_elementId, fieldId, text)) {
        dialogs::MessageDialog::warning(this, tr("Card Not Saved"), tr("Failed to save changes."));
    }
}

QString CardPage::subtitleOf(const core::ProjectManager& book,
                            const core::ProjectElement& element) {
    const QString kind = core::fillWords(QStringLiteral("{Kind}"),
                                         {{QStringLiteral("kind"), wordsOf(book, element)}});
    const QString value = card::subtitleValue(book, element);
    //: The kind of a card under its name, with the value of the field beside it: "Character ·
    //: main". In Polish: %1 · %2
    return value.isEmpty() ? kind : tr("%1 · %2").arg(kind, value);
}

QStringList CardPage::suggestedCards(const QString& fieldId) const {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const core::KindRef kind = element != nullptr ? m_book.kindOf(*element) : core::KindRef{};
    const core::CardField* field = m_book.cardField(kind, fieldId);
    if (field == nullptr || kind.package == nullptr) {
        return {};
    }
    return m_book.cardTitles(kind.package->id, field->cardKind, m_elementId);
}

void CardPage::fitNameFont() {
    if (!m_full || m_nameEdit == nullptr) {
        return;
    }
    QFont font = this->font();
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * NAME_SCALE);
    } else {
        font.setPixelSize(qRound(font.pixelSize() * NAME_SCALE));
    }
    font.setBold(true);
    m_nameEdit->setFont(font);
}

bool CardPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_portrait && opensMenu(event)) {
        m_portrait->showMenu();
        return true;
    }
    if (watched == m_infoGroup && event->type() == QEvent::Resize) {
        card::placeAtTitle(m_infoGroup, m_settingsButton);
    } else if (event->type() == QEvent::FocusOut) {
        saveFocused(watched);
    } else if (event->type() == QEvent::FocusIn) {
        fillSuggestions(watched);
    }
    // A field naming a card: its list at a click or with Alt+Down
    if (auto* line = qobject_cast<QLineEdit*>(watched);
        line != nullptr && line->completer() != nullptr && m_fieldOf.contains(watched)) {
        if (event->type() == QEvent::MouseButtonRelease &&
            static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton) {
            showAllCards(line);
        } else if (event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<const QKeyEvent*>(event);
            if (key->key() == Qt::Key_Down && key->modifiers() == Qt::AltModifier) {
                showAllCards(line);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CardPage::showAllCards(QLineEdit* line) {
    QCompleter* completer = line->completer();
    if (completer == nullptr || line->isReadOnly() || !line->isEnabled()) {
        return;
    }
    fillSuggestions(line);
    completer->setCompletionPrefix(QString());
    completer->complete();
}

void CardPage::saveFocused(const QObject* watched) {
    if (watched == m_shortEdit) {
        saveShort();
        return;
    }
    const auto* lines = qobject_cast<const QPlainTextEdit*>(watched);
    if (lines != nullptr && m_fieldOf.contains(watched)) {
        saveValue(m_fieldOf.value(watched), lines->toPlainText());
    }
}

void CardPage::fillSuggestions(const QObject* watched) {
    const auto* line = qobject_cast<const QLineEdit*>(watched);
    const QCompleter* completer = line != nullptr ? line->completer() : nullptr;
    auto* model =
        completer != nullptr ? qobject_cast<QStringListModel*>(completer->model()) : nullptr;
    if (model != nullptr && m_fieldOf.contains(watched)) {
        model->setStringList(suggestedCards(m_fieldOf.value(watched)));
    }
}

void CardPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::FontChange) {
        fitNameFont();
    }
    QWidget::changeEvent(event);
}

}  // namespace kalahari::gui
