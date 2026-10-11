/// @file user_project_dialog.cpp
/// @brief The second step of a new user project: the kinds of elements of the book

#include "kalahari/gui/dialogs/user_project_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/widgets/kind_palette.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr qreal WIDTH_EMS = 80.0;         ///< Eight tiles in a row, as in the mockup
constexpr qreal HEIGHT_EMS = 61.0;        ///< As high as the mockup
constexpr int MINIMUM_PALETTE_LINES = 8;  ///< Of the palette on a small screen
constexpr int BAR_SPACING = 12;           ///< Between the texts of the bar over the palette
constexpr int TOOLS_MARGIN_X = 12;        ///< At the sides of the box of the tools
constexpr int TOOLS_MARGIN_Y = 8;         ///< Above and below the texts of the box
constexpr int TOOLS_SPACING = 8;          ///< Between the icon and the texts of the box
constexpr int TOOLS_LINE_SPACING = 2;     ///< Between the title and the sentence of the box
constexpr qreal TOOLS_FILL = 0.04;        ///< Mid in the background of the box
constexpr qreal TOOLS_BORDER = 0.3;       ///< Mid in the border of the box

/// The parts of the book, whose groups have the names of the book's sections
constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                    core::BookPlace::Back};

/// @brief The box of the tools of the types: a little darker than the window, with a border
class ToolsBox : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        QPainter painter(this);
        const QColor window = palette().color(QPalette::Window);
        const QColor mid = palette().color(QPalette::Mid);
        painter.setPen(QPen(mixedColor(window, mid, TOOLS_BORDER), 1));
        painter.setBrush(mixedColor(window, mid, TOOLS_FILL));
        painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }
};

/// @brief A link of the bar over the palette
QLabel* newLink(const QString& text, QWidget* parent) {
    auto* link = new QLabel(parent);
    link->setTextFormat(Qt::RichText);
    link->setText(QStringLiteral("<a href=\"#\">%1</a>").arg(text.toHtmlEscaped()));
    link->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    return link;
}

/// @brief The kinds of the base package of @p registry, with an element of its main text kind
/// in the main section, so that the writer can start writing at once
core::PaletteChoice baseChoice(const core::BookTypeRegistry& registry) {
    const QString base = QString::fromLatin1(core::ProjectManager::BASE_PACKAGE);
    core::PaletteChoice choice;
    for (const core::KindRef& kind : registry.allKinds()) {
        if (kind.package->id == base) {
            choice.kinds.append(kind);
        }
    }
    if (const core::BookTypePackage* package = registry.package(base)) {
        const core::KindRef primary = registry.findKind(base, package->primaryKind);
        if (primary && primary.kind->allows(core::BookPlace::Main)) {
            choice.elements.append(core::StartElement{primary, core::BookPlace::Main});
        }
    }
    return choice;
}

} // anonymous namespace

UserProjectDialog::UserProjectDialog(const core::BookTypeRegistry& registry, QWidget* parent)
    : KalahariDialog(parent)
    , m_registry(registry)
{
    setHeadingIcon(QStringLiteral("template.empty"));

    // The number of the checked kinds, and the links that check many kinds at once
    auto* bar = new QHBoxLayout();
    bar->setSpacing(BAR_SPACING);
    m_countLabel = new QLabel(this);
    bar->addWidget(m_countLabel);
    bar->addStretch(1);
    //: A link over the palette of a new user project. In Polish: Zaznacz wszystkie
    QLabel* allLink = newLink(tr("Check All"), this);
    bar->addWidget(allLink);
    //: A link over the palette: the kinds of the package "Base" and only them. In Polish:
    //: Tylko „Podstawa”
    QLabel* baseLink = newLink(tr("Only \"Base\""), this);
    bar->addWidget(baseLink);
    //: A link over the palette of a new user project. In Polish: Odznacz wszystkie
    QLabel* noneLink = newLink(tr("Uncheck All"), this);
    bar->addWidget(noneLink);
    contentLayout()->addLayout(bar);

    // The palette, which scrolls and gets the room of a resized window
    m_palette = new KindPalette(registry);
    auto* area = new QScrollArea(this);
    area->setWidget(m_palette);
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setMinimumHeight(MINIMUM_PALETTE_LINES * fontMetrics().lineSpacing());
    //: The list of the kinds of elements in the window of a new user project. In Polish:
    //: Paleta rodzajów
    area->setAccessibleName(tr("Palette of Kinds"));
    contentLayout()->addWidget(area, 1);

    // The tools of the types, which no type has yet
    auto* tools = new ToolsBox(this);
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(TOOLS_MARGIN_X, TOOLS_MARGIN_Y, TOOLS_MARGIN_X,
                                    TOOLS_MARGIN_Y);
    toolsLayout->setSpacing(TOOLS_SPACING);
    auto* toolsIcon = new QLabel(tools);
    toolsIcon->setAlignment(Qt::AlignTop);
    setNoteIcon(toolsIcon, QStringLiteral("help.about"));
    toolsLayout->addWidget(toolsIcon);
    auto* toolsTexts = new QVBoxLayout();
    toolsTexts->setSpacing(TOOLS_LINE_SPACING);
    //: The tools that book types will have, e.g. a plot board. In Polish: Narzędzia typów
    auto* toolsTitle = new QLabel(tr("Tools of the Types"), tools);
    QFont bold;  // only the weight: the rest follows the font of the window
    bold.setWeight(QFont::Bold);
    toolsTitle->setFont(bold);
    toolsTexts->addWidget(toolsTitle);
    //: In Polish: Żaden typ nie ma jeszcze narzędzi. Gdy powstaną (np. tablica fabuły
    //: i słownik książki), wybierzesz je tutaj.
    auto* toolsText = new QLabel(tr("No type has tools yet. When they come (e.g. a plot board "
                                    "and a book dictionary), you will choose them here."),
                                 tools);
    toolsText->setWordWrap(true);
    toolsText->setForegroundRole(QPalette::PlaceholderText);
    toolsTexts->addWidget(toolsText);
    toolsLayout->addLayout(toolsTexts, 1);
    contentLayout()->addWidget(tools);

    // Back at the left; Skip, Create Book and Cancel at the right
    //: The button that returns to the first step of a new book. In Polish: < Wstecz
    m_backButton = addSideButton(tr("< Back"));
    connect(m_backButton, &QPushButton::clicked, this, [this]() { done(BACK); });
    //: The button that creates the book without the changes made in the palette. In Polish:
    //: Pomiń
    m_skipButton = buttonBox()->addButton(tr("Skip"), QDialogButtonBox::YesRole);
    //: The tooltip of Skip. In Polish: Tworzy książkę z rodzajami „Podstawy”, bez zmian
    //: wprowadzonych w tym oknie.
    m_skipButton->setToolTip(
        tr("Creates the book with the kinds of \"Base\", without the changes made in this "
           "window."));
    //: In Polish: Utwórz książkę
    setAcceptText(tr("Create Book"));
    // Adding a button lays out the box again and shows the buttons in it, Apply too; the box
    // then hands the focus to its first button, Skip, and Create Book takes it back, so that
    // Enter creates the book with the kinds checked in the palette
    applyButton()->hide();
    acceptButton()->setFocus();
    // The box tells which button it was before it accepts the window
    connect(buttonBox(), &QDialogButtonBox::clicked, this,
            [this](QAbstractButton* button) { m_skipped = button == m_skipButton; });

    connect(allLink, &QLabel::linkActivated, m_palette, &KindPalette::checkAll);
    connect(baseLink, &QLabel::linkActivated, this, [this]() {
        m_palette->checkOnly(QString::fromLatin1(core::ProjectManager::BASE_PACKAGE));
    });
    connect(noneLink, &QLabel::linkActivated, m_palette, &KindPalette::uncheckAll);
    connect(m_palette, &KindPalette::choiceChanged, this, &UserProjectDialog::updateCount);

    // The kinds of the base package at first; Skip creates the book as the palette is now
    m_palette->setChoice(baseChoice(registry));
    m_firstChoice = m_palette->choice();

    // The groups of a book without sections, until setBook() names its sections
    setBook(QString(), QString(), std::nullopt);
    //: The group of the kinds of the Workshop in the palette. In Polish: Warsztat
    m_palette->setGroupName(core::BookPlace::Workshop, tr("Workshop"));

    // New icon colors or sizes (a change of theme or icon settings) redraw the icon of the box
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, toolsIcon,
            [toolsIcon]() { setNoteIcon(toolsIcon, QStringLiteral("help.about")); });

    // The size of the mockup at the font of the window; on a smaller screen the window is
    // smaller and the palette scrolls
    const int em = QFontInfo(font()).pixelSize();
    resize(qRound(WIDTH_EMS * em), qRound(HEIGHT_EMS * em));
}

void UserProjectDialog::setBook(const QString& title, const QString& language,
                                const std::optional<core::BookSections>& sections)
{
    m_title = title;
    //: The heading of the second step of a new user project; %1: the title of the book. In
    //: Polish: Rodzaje elementów książki „%1”
    setHeading(tr("Kinds of Elements of the Book \"%1\"").arg(title),
               //: In Polish: Zaznacz rodzaje, które ma mieć książka. Elementy z zaznaczonym
               //: polem „Utwórz od razu” powstaną razem z nią.
               tr("Check the kinds the book is to have. The elements with \"Create now\" "
                  "checked are created with it."));
    //: The window title of the second step of a new user project. In Polish: Nowa książka –
    //: projekt użytkownika
    setWindowTitle(tr("New Book – User Project"));

    // The groups have the names of the sections, as the Navigator shows them; a book without
    // sections has a beginning, a content and an end
    core::ProjectBook book;
    if (sections && sections->shown) {
        book.language = language;
        book.setSections(*sections);
    }
    for (const core::BookPlace place : BOOK_PLACES) {
        m_palette->setGroupName(place, sections && sections->shown
                                           ? book.sectionName(place)
                                           : SectionWords::groupName(place));
    }
}

void UserProjectDialog::setOpeningQuestion(NewBookQuestion question)
{
    m_openingQuestion = std::move(question);
}

void UserProjectDialog::accept()
{
    const core::PaletteChoice choice = m_skipped ? m_firstChoice : m_palette->choice();
    m_skipped = false;

    // The question about the open book comes before the book is created; its Cancel returns
    // to this window
    bool opens = true;
    if (m_openingQuestion) {
        switch (m_openingQuestion(this, m_title)) {
            case NewBookOpening::Open:
                break;
            case NewBookOpening::DontOpen:
                opens = false;
                break;
            case NewBookOpening::Cancel:
                return;
        }
    }
    m_choice = choice;
    m_opens = opens;
    KalahariDialog::accept();
}

void UserProjectDialog::updateCount()
{
    const int count = m_palette->checkedCount();
    //: Over the palette of a new user project. In Polish: Zaznaczone rodzaje: %1
    m_countLabel->setText(tr("Checked kinds: %1").arg(count));
    acceptButton()->setEnabled(count > 0);
    //: The tooltip of Create Book when no kind is checked. In Polish: Zaznacz co najmniej
    //: jeden rodzaj.
    acceptButton()->setToolTip(count > 0 ? QString() : tr("Check at least one kind."));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
