/// @file paragraph_panel.cpp
/// @brief The Paragraph and Styles panel

#include "kalahari/gui/panels/paragraph_panel.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/style_coordinator.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/style_tag_delegate.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"

#include <QAction>
#include <QButtonGroup>
#include <QEvent>
#include <QFontInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief The setting of the palette: the styles in their look (true) or by their names
constexpr const char* STYLE_PREVIEW_KEY = "panels.stylePreview";

/// @brief The smallest and the largest size of a style's name in its look, as a share of
/// the panel's font: a heading stays readable in a narrow panel
constexpr double PREVIEW_MIN_SCALE = 0.9;
constexpr double PREVIEW_MAX_SCALE = 1.6;

/// @brief The time the panel waits for the cursor to stop before it shows the paragraph
constexpr int REFRESH_DELAY_MS = 100;

/// @brief The commands of the styles of the book, whose icons the palette's buttons have
constexpr const char* NEW_STYLE_ID = "format.style.new";
constexpr const char* MODIFY_STYLE_ID = "format.style.modify";
constexpr const char* DELETE_STYLE_ID = "format.style.delete";
constexpr const char* MANAGE_STYLES_ID = "format.style.manage";

/// @brief A name in the text of a menu item: an "&" of the name is not the item's key
QString menuName(QString name) {
    return name.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

/// @brief A number as the system writes numbers (12 345 in Polish)
QString numberText(qsizetype number) {
    return QLocale().toString(number);
}

/// @brief @p base in the look of style @p id: its size relative to Normal's, bold, italic,
/// underline and a font of its own, as the editor shows the style
QFont styleFont(const QFont& base, const editor::ParagraphStyleSet& styles, const QString& id) {
    QFont font = base;
    const QString normal = QString::fromLatin1(editor::NORMAL_STYLE_ID);
    const double normalSize = styles.property(normal, QStringLiteral("fontSize")).toDouble();
    const double ownSize = styles.property(id, QStringLiteral("fontSize")).toDouble();
    const double baseSize = base.pointSizeF() > 0 ? base.pointSizeF() : QFontInfo(base).pointSizeF();
    if (normalSize > 0 && ownSize > 0 && baseSize > 0) {
        font.setPointSizeF(baseSize * std::clamp(ownSize / normalSize, PREVIEW_MIN_SCALE,
                                                 PREVIEW_MAX_SCALE));
    }
    font.setBold(styles.property(id, QStringLiteral("bold")).toBool());
    font.setItalic(styles.property(id, QStringLiteral("italic")).toBool());
    font.setUnderline(styles.property(id, QStringLiteral("underline")).toBool());
    // Normal is shown in the editor's font, so a family of the package counts only where it
    // differs from Normal's
    const QString family = styles.property(id, QStringLiteral("fontFamily")).toString();
    if (!family.isEmpty() &&
        family != styles.property(normal, QStringLiteral("fontFamily")).toString()) {
        font.setFamilies({family});
    }
    return font;
}

}  // namespace

ParagraphPanel::ParagraphPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("paragraphPanel"));

    // As narrow as the other panels on the right: the parts go on in the next line where the
    // panel is too narrow for them
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(createBand());

    m_emptyLabel = new QLabel(
        tr("No text with a cursor is in front. Click in the text of a chapter."), this);
    m_emptyLabel->setObjectName(QStringLiteral("paragraphEmptyLabel"));
    m_emptyLabel->setWordWrap(true);
    layout->addWidget(m_emptyLabel);

    layout->addWidget(createParagraphRows());
    layout->addWidget(createPalette(), 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(REFRESH_DELAY_MS);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (isVisible()) {
            refresh();
        } else {
            m_stale = true;
        }
    });

    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &ParagraphPanel::applyIconSize);
    applyIconSize();

    setShowsPreview(core::SettingsManager::getInstance().get<bool>(STYLE_PREVIEW_KEY, false));
    refresh();
}

ParagraphPanel::~ParagraphPanel() = default;

QWidget* ParagraphPanel::createBand() {
    auto* band = new QWidget(this);
    band->setObjectName(QStringLiteral("paragraphBand"));
    // The place goes under the title in a narrow panel
    auto* flow = new FlowLayout(band, 12, 2);
    flow->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_foldButton = new QToolButton(band);
    m_foldButton->setObjectName(QStringLiteral("paragraphFoldButton"));
    m_foldButton->setAutoRaise(true);
    m_foldButton->setArrowType(Qt::DownArrow);
    m_foldButton->setToolTip(tr("Fold"));
    m_foldButton->hide();
    connect(m_foldButton, &QToolButton::clicked, this, [this]() { setFolded(!m_folded); });
    row->addWidget(m_foldButton);

    m_titleLabel = new QLabel(tr("Current Paragraph"), band);
    m_titleLabel->setObjectName(QStringLiteral("paragraphTitle"));
    QFont bold = m_titleLabel->font();
    bold.setBold(true);
    m_titleLabel->setFont(bold);
    row->addWidget(m_titleLabel);
    flow->addLayout(row);

    m_placeLabel = new QLabel(band);
    m_placeLabel->setObjectName(QStringLiteral("paragraphPlace"));
    flow->addWidget(m_placeLabel);
    return band;
}

QWidget* ParagraphPanel::createParagraphRows() {
    m_rows = new QWidget(this);
    auto* form = new QFormLayout(m_rows);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);
    // The buttons under their label in a narrow panel
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // An object at the cursor: its kind
    m_kindTitle = rowLabel(tr("Kind:"));
    m_kindLabel = new QLabel(m_rows);
    m_kindLabel->setObjectName(QStringLiteral("paragraphKind"));
    form->addRow(m_kindTitle, m_kindLabel);

    // Bold, italic and underline together, then Clear Formatting after a line
    m_formatBar = commandBar({"format.bold", "format.italic", "format.underline", nullptr,
                              "format.clearFormatting"});
    m_formatBar->setObjectName(QStringLiteral("paragraphFormatBar"));
    form->addRow(rowLabel(tr("Formatting:")), m_formatBar);

    m_alignBar = commandBar(
        {"format.alignLeft", "format.alignCenter", "format.alignRight", "format.justify"});
    m_alignBar->setObjectName(QStringLiteral("paragraphAlignBar"));
    form->addRow(rowLabel(tr("Alignment:")), m_alignBar);

    // A scene break looks as the Settings say
    m_noteLabel = new QLabel(
        tr("The look of the scene breaks: Edit > Settings..., group Editor > General."), m_rows);
    m_noteLabel->setObjectName(QStringLiteral("paragraphNote"));
    m_noteLabel->setWordWrap(true);
    form->addRow(m_noteLabel);
    return m_rows;
}

QWidget* ParagraphPanel::createPalette() {
    m_palette = new QWidget(this);
    auto* layout = new QVBoxLayout(m_palette);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // The title, and Names and Preview together after it, or under it in a narrow panel
    auto* header = new FlowLayout(nullptr, 12, 4);
    auto* title = new QLabel(tr("Styles"), m_palette);
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    header->addWidget(title);
    auto* modeButtons = new QHBoxLayout();
    modeButtons->setContentsMargins(0, 0, 0, 0);
    modeButtons->setSpacing(0);

    // Names or Preview, one of them on
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    const QList<std::pair<QString, QString>> modes = {
        {tr("Names"), tr("The styles by their names")},
        {tr("Preview"), tr("The styles in their look")}};
    for (int i = 0; i < modes.size(); ++i) {
        auto* button = new QToolButton(m_palette);
        button->setObjectName(i == 0 ? QStringLiteral("styleNamesButton")
                                     : QStringLiteral("stylePreviewButton"));
        button->setText(modes.at(i).first);
        button->setToolTip(modes.at(i).second);
        button->setCheckable(true);
        m_modeGroup->addButton(button, i);
        modeButtons->addWidget(button);
    }
    header->addLayout(modeButtons);
    connect(m_modeGroup, &QButtonGroup::idClicked, this,
            [this](int id) { setShowsPreview(id == 1); });
    layout->addLayout(header);

    // New, Modify and Delete for the style chosen in the list, then Manage Styles...; they show
    // with the styles of the book (setStyleCoordinator())
    m_styleBar = new QToolBar(m_palette);
    m_styleBar->setObjectName(QStringLiteral("paragraphStyleBar"));
    m_styleBar->setMovable(false);
    m_styleBar->setFloatable(false);
    m_styleBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto& art = core::ArtProvider::getInstance();
    m_newStyleAction = art.createAction(QString::fromLatin1(NEW_STYLE_ID), tr("New Style..."),
                                        this, core::IconContext::Button);
    m_modifyStyleAction = art.createAction(QString::fromLatin1(MODIFY_STYLE_ID),
                                           tr("Modify Style..."), this, core::IconContext::Button);
    m_deleteStyleAction = art.createAction(QString::fromLatin1(DELETE_STYLE_ID),
                                           tr("Delete Style"), this, core::IconContext::Button);
    m_manageStylesAction = art.createAction(QString::fromLatin1(MANAGE_STYLES_ID),
                                            tr("Manage Styles..."), this,
                                            core::IconContext::Button);
    m_newStyleAction->setObjectName(QStringLiteral("paletteNewStyle"));
    m_modifyStyleAction->setObjectName(QStringLiteral("paletteModifyStyle"));
    m_deleteStyleAction->setObjectName(QStringLiteral("paletteDeleteStyle"));
    m_manageStylesAction->setObjectName(QStringLiteral("paletteManageStyles"));
    m_styleBar->addAction(m_newStyleAction);
    m_styleBar->addAction(m_modifyStyleAction);
    m_styleBar->addAction(m_deleteStyleAction);
    m_styleBar->addSeparator();
    m_styleBar->addAction(m_manageStylesAction);
    connect(m_newStyleAction, &QAction::triggered, this, [this]() {
        if (!m_styles) {
            return;
        }
        // A style the book does not have is made for its paragraphs
        const QString id = paletteStyle();
        if (isMissing(id)) {
            m_styles->createMissingStyle(id, window());
        } else {
            m_styles->newStyle(id, true, window());
        }
    });
    connect(m_modifyStyleAction, &QAction::triggered, this, [this]() {
        if (m_styles) {
            m_styles->modifyStyle(paletteStyle(), window());
        }
    });
    connect(m_deleteStyleAction, &QAction::triggered, this, [this]() {
        if (m_styles) {
            m_styles->deleteStyle(paletteStyle(), window());
        }
    });
    connect(m_manageStylesAction, &QAction::triggered, this, [this]() {
        if (m_styles) {
            m_styles->manageStyles(paletteStyle());
        }
    });
    m_styleBar->hide();
    layout->addWidget(m_styleBar);

    m_styleList = new QListWidget(m_palette);
    m_styleList->setObjectName(QStringLiteral("paragraphStyleList"));
    m_styleList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_styleList->setUniformItemSizes(false);
    // "own" after the writer's own styles, "changed" after the styles of the type the book
    // changed
    m_styleList->setItemDelegate(new StyleTagDelegate(m_styleList));
    // A click gives the style; Enter does it for the style chosen with the arrows (a double
    // click, or a single one where the system activates items so, gives the style once: the
    // paragraphs have it already)
    connect(m_styleList, &QListWidget::itemClicked, this, &ParagraphPanel::applyStyle);
    connect(m_styleList, &QListWidget::itemActivated, this, &ParagraphPanel::applyStyle);
    // The buttons act on the style chosen; the right mouse button, the menu key and Shift+F10
    // open its menu
    connect(m_styleList, &QListWidget::currentItemChanged, this,
            &ParagraphPanel::updateStyleActions);
    m_styleList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_styleList, &QWidget::customContextMenuRequested, this,
            &ParagraphPanel::showStyleMenu);
    layout->addWidget(m_styleList, 1);

    //: Under the list of styles. In Polish: Kliknięcie stylu nadaje go akapitowi z kursorem.
    //: Prawy przycisk myszy albo Shift+F10 otwiera menu stylu.
    m_styleHint = new QLabel(tr("A click on a style gives it to the paragraph at the cursor. "
                                "The right mouse button or Shift+F10 opens the menu of the "
                                "style."),
                             m_palette);
    m_styleHint->setObjectName(QStringLiteral("paragraphStyleHint"));
    m_styleHint->setWordWrap(true);
    m_styleHint->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_styleHint);
    return m_palette;
}

QLabel* ParagraphPanel::rowLabel(const QString& text) {
    // As high as its row, the text in the middle of the buttons
    auto* label = new QLabel(text, m_rows);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    return label;
}

QToolBar* ParagraphPanel::commandBar(const QList<const char*>& commandIds) {
    auto* bar = new QToolBar(this);
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const char* id : commandIds) {
        if (id == nullptr) {
            bar->addSeparator();
        } else if (QAction* action = registry.getAction(std::string(id))) {
            bar->addAction(action);
        }
    }
    return bar;
}

void ParagraphPanel::applyIconSize() {
    // The size of the buttons in panels (Settings > Appearance > Icons > Buttons)
    const int size = core::ArtProvider::getInstance().getIconSize(core::IconContext::Button);
    for (QToolBar* bar : {m_formatBar, m_alignBar, m_styleBar}) {
        bar->setIconSize(QSize(size, size));
        // All the buttons in sight: a narrow panel puts them under their label, not in a menu
        // of the toolbar's arrow
        bar->setMinimumWidth(0);
        bar->setMinimumWidth(bar->sizeHint().width());
    }
}

editor::BookEditor* ParagraphPanel::editor() const {
    return m_editor;
}

void ParagraphPanel::setEditor(editor::BookEditor* editor) {
    if (m_editor != nullptr) {
        disconnect(m_editor, nullptr, this, nullptr);
    }
    m_editor = editor;
    if (editor != nullptr) {
        connect(editor, &editor::BookEditor::cursorPositionChanged, this,
                &ParagraphPanel::scheduleRefresh);
        connect(editor, &editor::BookEditor::selectionChanged, this,
                &ParagraphPanel::scheduleRefresh);
        connect(editor, &editor::BookEditor::contentChanged, this,
                &ParagraphPanel::scheduleRefresh);
        // A style of the book added, changed or removed: the palette lists the styles anew
        connect(editor, &editor::BookEditor::paragraphStylesChanged, this, [this]() {
            m_styleList->clear();
            scheduleRefresh();
        });
        // A text closed while it is followed: the panel follows none
        connect(editor, &QObject::destroyed, this, [this]() {
            m_editor = nullptr;
            m_refreshTimer->stop();
            refresh();
        });
    }
    m_refreshTimer->stop();
    refresh();
}

void ParagraphPanel::setFoldable(bool foldable) {
    m_foldButton->setVisible(foldable);
    if (!foldable) {
        setFolded(false);
    }
}

void ParagraphPanel::setFolded(bool folded) {
    if (folded == m_folded) {
        return;
    }
    m_folded = folded;
    m_foldButton->setArrowType(folded ? Qt::RightArrow : Qt::DownArrow);
    m_foldButton->setToolTip(folded ? tr("Unfold") : tr("Fold"));
    showParagraph();  // the rows and the palette show or go
    emit foldedChanged(folded);
}

bool ParagraphPanel::showsPreview() const {
    return m_modeGroup->checkedId() == 1;
}

void ParagraphPanel::setShowsPreview(bool preview) {
    m_modeGroup->button(preview ? 1 : 0)->setChecked(true);
    auto& settings = core::SettingsManager::getInstance();
    if (settings.get<bool>(STYLE_PREVIEW_KEY, false) != preview) {
        settings.set<bool>(STYLE_PREVIEW_KEY, preview);
    }
    showStyleLook();
}

void ParagraphPanel::scheduleRefresh() {
    m_refreshTimer->start();
}

void ParagraphPanel::refresh() {
    m_refreshTimer->stop();
    m_stale = false;
    showParagraph();
    showStyles();
}

void ParagraphPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The other place of the panel may have changed the mode of the palette
    const bool preview =
        core::SettingsManager::getInstance().get<bool>(STYLE_PREVIEW_KEY, false);
    if (preview != showsPreview()) {
        setShowsPreview(preview);
    }
    if (m_stale) {
        refresh();
    }
}

void ParagraphPanel::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange && m_styleList != nullptr) {
        showStyleLook();
    }
}

void ParagraphPanel::showParagraph() {
    const editor::BookEditor* text = m_editor;
    const bool selected = text != nullptr && text->hasSelection();
    m_titleLabel->setText(selected ? tr("Selected Paragraphs") : tr("Current Paragraph"));

    QString place;
    editor::TextObject object = editor::TextObject::None;
    if (text != nullptr) {
        const qsizetype count = static_cast<qsizetype>(text->paragraphCount());
        if (selected) {
            const editor::SelectionRange range = text->selection().normalized();
            place = tr("%1 of %2")
                        .arg(numberText(range.end.paragraph - range.start.paragraph + 1),
                             numberText(count));
        } else {
            object = text->currentObject();
            place = tr("paragraph %1 of %2")
                        .arg(numberText(text->cursorPosition().paragraph + 1), numberText(count));
        }
    }
    m_placeLabel->setText(place);

    // Folded, only the band shows; without a text, why nothing does
    m_emptyLabel->setVisible(!m_folded && text == nullptr);
    m_rows->setVisible(!m_folded && text != nullptr);
    m_palette->setVisible(!m_folded);

    const bool atObject = object != editor::TextObject::None;
    m_kindTitle->setVisible(atObject);
    m_kindLabel->setVisible(atObject);
    m_noteLabel->setVisible(object == editor::TextObject::SceneBreak);
    switch (object) {
        case editor::TextObject::HorizontalRule:
            m_kindLabel->setText(tr("Horizontal Line"));
            break;
        case editor::TextObject::SceneBreak:
            m_kindLabel->setText(tr("Scene Break"));
            break;
        case editor::TextObject::PageBreak:
            m_kindLabel->setText(tr("Page Break"));
            break;
        case editor::TextObject::FigureList:
            m_kindLabel->setText(tr("List of Figures"));
            break;
        case editor::TextObject::None:
            m_kindLabel->clear();
            break;
    }
}

void ParagraphPanel::showStyles() {
    const editor::BookEditor* text = m_editor;
    m_updating = true;

    // The styles of the text, as Format > Text Style lists them, tagged as the book has them;
    // built again when they change
    struct Listed {
        QString id;
        QString name;
        editor::StyleOrigin origin = editor::StyleOrigin::Type;
        bool missing = false;
    };
    QList<Listed> styles;
    if (text != nullptr) {
        const editor::ParagraphStyleSet& set = text->paragraphStyles();
        for (const editor::ParagraphStyle& style : set.styles()) {
            styles.append({style.id, style.name.isEmpty() ? style.id : style.name, style.origin});
        }
        // Then the styles of paragraphs from another book that this book does not have, by
        // their ids: they look as Normal until the book has them
        QStringList missing;
        const QHash<QString, int> counts = text->paragraphStyleCounts();
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (!it.key().isEmpty() && !set.contains(it.key())) {
                missing.append(it.key());
            }
        }
        missing.sort();
        for (const QString& id : missing) {
            styles.append({id, id, editor::StyleOrigin::Type, true});
        }
    }
    bool same = m_styleList->count() == styles.size();
    for (int i = 0; same && i < styles.size(); ++i) {
        const QListWidgetItem* item = m_styleList->item(i);
        same = item->data(Qt::UserRole).toString() == styles.at(i).id &&
               item->text() == styles.at(i).name &&
               item->data(StyleTagDelegate::ORIGIN_ROLE).toInt() ==
                   static_cast<int>(styles.at(i).origin) &&
               item->data(StyleTagDelegate::MISSING_ROLE).toBool() == styles.at(i).missing;
    }
    if (!same) {
        m_styleList->clear();
        for (const Listed& style : styles) {
            auto* item = new QListWidgetItem(style.name, m_styleList);
            item->setData(Qt::UserRole, style.id);
            if (style.missing) {
                StyleTagDelegate::tagMissing(item);
            } else {
                StyleTagDelegate::tagItem(item, style.origin);
            }
        }
        showStyleLook();
    }

    // The style of the paragraphs selected; none when they differ, at an object, without a
    // text
    const QString current = text != nullptr ? text->selectedParagraphStyle() : QString();
    QListWidgetItem* currentItem = nullptr;
    for (int i = 0; i < m_styleList->count(); ++i) {
        if (!current.isEmpty() && m_styleList->item(i)->data(Qt::UserRole).toString() == current) {
            currentItem = m_styleList->item(i);
            break;
        }
    }
    if (currentItem != nullptr) {
        m_styleList->setCurrentItem(currentItem);
        m_styleList->scrollToItem(currentItem);
    } else {
        m_styleList->clearSelection();
        m_styleList->setCurrentItem(nullptr);
    }

    // A book open read-only, or an object at the cursor, takes no style
    const bool readOnly = text != nullptr && text->isReadOnly();
    const bool atObject = text != nullptr && !text->hasSelection() &&
                          text->currentObject() != editor::TextObject::None;
    m_styleList->setEnabled(text != nullptr && !readOnly && !atObject);
    if (readOnly) {
        m_styleList->setToolTip(readOnlyReason());
    } else if (atObject) {
        m_styleList->setToolTip(tr("Click in the text of a paragraph to give it a style"));
    } else {
        m_styleList->setToolTip(
            tr("A click on a style gives it to the paragraph at the cursor or to the selected "
               "paragraphs"));
    }
    m_updating = false;
    updateStyleActions();
}

void ParagraphPanel::setStyleCoordinator(StyleCoordinator* styles) {
    if (m_styles != nullptr) {
        disconnect(m_styles, nullptr, this, nullptr);
    }
    m_styles = styles;
    m_styleBar->setVisible(styles != nullptr);
    if (styles != nullptr) {
        // The book's styles changed, or a change was undone: the buttons of the style anew
        connect(styles, &StyleCoordinator::stylesChanged, this,
                &ParagraphPanel::updateStyleActions);
    }
    updateStyleActions();
}

QString ParagraphPanel::paletteStyle() const {
    if (const QListWidgetItem* item = m_styleList->currentItem()) {
        return item->data(Qt::UserRole).toString();
    }
    if (m_editor == nullptr) {
        return QString();
    }
    // Paragraphs of different styles selected: the style of the one with the cursor
    const QString selected = m_editor->selectedParagraphStyle();
    return selected.isEmpty() ? m_editor->currentParagraphStyle() : selected;
}

bool ParagraphPanel::isMissing(const QString& id) const {
    return m_editor != nullptr && !id.isEmpty() && !m_editor->paragraphStyles().contains(id);
}

QString ParagraphPanel::styleName(const QString& id) const {
    const editor::ParagraphStyle* style =
        m_editor != nullptr ? m_editor->paragraphStyles().find(id) : nullptr;
    return style != nullptr && !style->name.isEmpty() ? style->name : id;
}

void ParagraphPanel::updateStyleActions() {
    if (m_styles == nullptr) {
        for (QAction* action : {m_newStyleAction, m_modifyStyleAction, m_deleteStyleAction,
                                m_manageStylesAction}) {
            action->setEnabled(false);
        }
        return;
    }
    // Manage Styles... needs a book that can change; the others a text with a style too
    const QString whyNotChange = m_styles->whyNotChange();
    const QString id = paletteStyle();
    QString reason = whyNotChange;
    if (reason.isEmpty() && id.isEmpty()) {
        //: Why the buttons of a style in the palette are greyed out. In Polish: Na wierzchu nie
        //: ma tekstu z kursorem
        reason = tr("No text with a cursor is in front");
    }
    const QString name = styleName(id);
    const auto offer = [](QAction* action, const QString& why, const QString& toolTip) {
        action->setEnabled(why.isEmpty());
        action->setToolTip(why.isEmpty() ? toolTip : why);
    };
    // A style the book does not have: New makes it, for its paragraphs
    const bool missing = reason.isEmpty() && isMissing(id);
    offer(m_newStyleAction, reason,
          missing
              //: The tooltip of New Style in the palette for a style the book does not have.
              //: In Polish: Utwórz styl „%1” dla jego akapitów…
              ? tr("Create the style \"%1\" for its paragraphs...").arg(name)
              //: The tooltip of New Style in the palette. In Polish: Nowy styl na podstawie
              //: „%1”…
              : tr("New style based on \"%1\"...").arg(name));
    //: In Polish: Zmień styl „%1”…
    offer(m_modifyStyleAction,
          missing ? m_styles->whyNotDelete(id) : reason,
          tr("Modify the style \"%1\"...").arg(name));
    //: In Polish: Usuń styl „%1”
    offer(m_deleteStyleAction, reason.isEmpty() ? m_styles->whyNotDelete(id) : reason,
          tr("Delete the style \"%1\"").arg(name));
    offer(m_manageStylesAction, whyNotChange, tr("Manage Styles..."));
}

void ParagraphPanel::showStyleMenu(const QPoint& pos) {
    // The style under the mouse; from the keys, the style chosen
    if (QListWidgetItem* item = m_styleList->itemAt(pos)) {
        m_styleList->setCurrentItem(item);
    }
    QMenu menu(this);
    fillStyleMenu(menu);
    if (!menu.isEmpty()) {
        menu.exec(m_styleList->viewport()->mapToGlobal(pos));
    }
}

void ParagraphPanel::fillStyleMenu(QMenu& menu) {
    const QString id = paletteStyle();
    if (id.isEmpty()) {
        return;
    }
    const QString name = menuName(styleName(id));

    // What a click does, in bold
    //: The first item of the menu of a style in the palette. In Polish: &Nadaj styl „%1”
    QAction* apply = menu.addAction(tr("&Apply the Style \"%1\"").arg(name));
    QFont bold = apply->font();
    bold.setBold(true);
    apply->setFont(bold);
    apply->setEnabled(m_styleList->isEnabled());
    connect(apply, &QAction::triggered, this, [this, id]() { giveStyle(id); });
    if (m_styles == nullptr) {
        return;
    }

    menu.addSeparator();
    const auto add = [&menu](QAction* button, const QString& text) {
        QAction* item = menu.addAction(button->icon(), text);
        item->setEnabled(button->isEnabled());
        connect(item, &QAction::triggered, button, &QAction::trigger);
        return item;
    };
    // A style the book does not have can only be made
    if (isMissing(id)) {
        //: In the menu of a style the book does not have. In Polish: &Utwórz styl „%1”…
        add(m_newStyleAction, tr("&Create the Style \"%1\"...").arg(name));
        menu.addSeparator();
        add(m_manageStylesAction, tr("Manage &Styles..."));
        return;
    }
    //: In Polish: N&owy styl na podstawie „%1”…
    add(m_newStyleAction, tr("&New Style Based on \"%1\"...").arg(name));
    //: In Polish: &Zmień styl „%1”…
    add(m_modifyStyleAction, tr("&Modify the Style \"%1\"...").arg(name));
    // A style of the type comes back as the type has it; the writer's own has no such
    const editor::ParagraphStyle* style =
        m_editor != nullptr ? m_editor->paragraphStyles().find(id) : nullptr;
    if (style != nullptr && style->origin != editor::StyleOrigin::Own) {
        //: In Polish: &Przywróć styl typu
        QAction* reset = menu.addAction(tr("&Reset to the Type's Style"));
        reset->setEnabled(m_styles->whyNotChange().isEmpty() &&
                          m_styles->whyNotReset(id).isEmpty());
        connect(reset, &QAction::triggered, this, [this, id]() {
            if (m_styles) {
                m_styles->resetStyle(id);
            }
        });
    }
    //: In Polish: &Usuń styl „%1”
    add(m_deleteStyleAction, tr("&Delete the Style \"%1\"").arg(name));
    menu.addSeparator();
    //: In Polish: Zarządzaj &stylami…
    add(m_manageStylesAction, tr("Manage &Styles..."));
}

void ParagraphPanel::showStyleLook() {
    const bool preview = showsPreview();
    const QFont base = m_styleList->font();
    for (int i = 0; i < m_styleList->count(); ++i) {
        QListWidgetItem* item = m_styleList->item(i);
        item->setFont(preview && m_editor
                          ? styleFont(base, m_editor->paragraphStyles(),
                                      item->data(Qt::UserRole).toString())
                          : base);
    }
}

void ParagraphPanel::applyStyle(QListWidgetItem* item) {
    if (m_updating || item == nullptr) {
        return;
    }
    giveStyle(item->data(Qt::UserRole).toString());
}

void ParagraphPanel::giveStyle(const QString& id) {
    if (!m_editor) {
        return;
    }
    // As Format > Text Style: the paragraphs of the cursor or the selection (a chapter of a
    // book open read-only refuses it, and says why); the writer goes on writing
    m_editor->applyParagraphStyle(id);
    m_editor->setFocus(Qt::OtherFocusReason);
    refresh();
}

} // namespace gui
} // namespace kalahari
