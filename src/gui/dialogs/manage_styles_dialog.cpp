// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file manage_styles_dialog.cpp
/// @brief Format > Text Style > Manage Styles...

#include "kalahari/gui/dialogs/manage_styles_dialog.h"

#include "kalahari/gui/dialogs/style_dialog.h"
#include "kalahari/gui/style_draft.h"
#include "kalahari/gui/widgets/style_tag_delegate.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

/// The list and the preview, in pixels
constexpr int LIST_WIDTH = 220;
constexpr int PREVIEW_HEIGHT = 110;
constexpr int DETAILS_WIDTH = 300;
constexpr int COLUMN_SPACING = 12;
constexpr int FIELD_SPACING = 6;

/// The font of the headings of the groups, as a share of the list's
constexpr qreal HEADING_SHARE = 0.85;

/// Numbers the same to a hundredth
constexpr double SAME_NUMBER = 0.005;

QString normalId() {
    return QString::fromLatin1(editor::NORMAL_STYLE_ID);
}

/// The look of a style: its properties, and the program's where no style sets one
QVariantMap lookOf(const editor::ParagraphStyleSet& set, const QString& id) {
    const editor::ParagraphStyle* style = set.find(id);
    QVariantMap look;
    for (const QString& name : StyleDraft::propertyNames()) {
        const QVariant value = style != nullptr ? style->properties.value(name) : QVariant();
        look.insert(name, value.isValid() ? value : StyleDraft::defaultValue(name));
    }
    return look;
}

}  // namespace

ManageStylesDialog::ManageStylesDialog(StyleCoordinator& styles, const QString& selectedId,
                                       QWidget* parent)
    : KalahariDialog(parent)
    , m_styles(styles) {
    //: The window Format > Text Style > Manage Styles. In Polish: Zarządzaj stylami
    setHeading(tr("Manage Styles"),
               //: In Polish: Style akapitów tej książki: style typu „%1” i jej własne.
               tr("The paragraph styles of this book: the styles of the type \"%1\" and its "
                  "own.")
                   .arg(m_styles.typeName()));
    setHeadingIcon(QStringLiteral("format.style.manage"));
    setCompactHeading(true);
    //: In Polish: Zamknij
    setAcceptText(tr("Close"));
    cancelButton()->hide();

    auto* columns = new QHBoxLayout();
    columns->setSpacing(COLUMN_SPACING);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("manageStylesList"));
    m_list->setItemDelegate(new StyleTagDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setMinimumWidth(LIST_WIDTH);
    //: In Polish: Style
    m_list->setAccessibleName(tr("Styles"));
    m_list->installEventFilter(this);
    columns->addWidget(m_list);

    // The style selected: its name and kind, preview, settings and use
    auto* details = new QVBoxLayout();
    details->setSpacing(FIELD_SPACING);
    auto* title = new QHBoxLayout();
    m_name = new QLabel(this);
    QFont bold = m_name->font();
    bold.setBold(true);
    m_name->setFont(bold);
    m_kind = new QLabel(this);
    m_kind->setForegroundRole(QPalette::PlaceholderText);
    m_kind->setWordWrap(true);
    title->addWidget(m_name);
    title->addWidget(m_kind, 1);
    details->addLayout(title);
    m_preview = new StylePreview(this);
    m_preview->setMinimumSize(DETAILS_WIDTH, PREVIEW_HEIGHT);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    details->addWidget(m_preview);
    m_settings = new QLabel(this);
    m_settings->setTextFormat(Qt::RichText);
    m_settings->setWordWrap(true);
    m_settings->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details->addWidget(m_settings);
    m_usage = new QLabel(this);
    m_usage->setForegroundRole(QPalette::PlaceholderText);
    m_usage->setWordWrap(true);
    details->addWidget(m_usage);
    m_undoHint = new QLabel(this);
    m_undoHint->setForegroundRole(QPalette::PlaceholderText);
    m_undoHint->setWordWrap(true);
    details->addWidget(m_undoHint);
    details->addStretch(1);
    columns->addLayout(details, 1);
    contentLayout()->addLayout(columns, 1);

    // The buttons of the style selected, at the left under the list
    //: In Polish: &Nowy...
    m_newButton = addSideButton(tr("&New..."));
    //: In Polish: &Zmień...
    m_modifyButton = addSideButton(tr("&Modify..."));
    //: In Polish: &Usuń
    m_deleteButton = addSideButton(tr("&Delete"));
    //: In Polish: P&rzywróć styl typu
    m_resetButton = addSideButton(tr("&Reset to the Type's Style"));
    connect(m_newButton, &QPushButton::clicked, this, &ManageStylesDialog::newStyle);
    connect(m_modifyButton, &QPushButton::clicked, this, &ManageStylesDialog::modifyStyle);
    connect(m_deleteButton, &QPushButton::clicked, this, &ManageStylesDialog::deleteStyle);
    connect(m_resetButton, &QPushButton::clicked, this, &ManageStylesDialog::resetStyle);
    connect(m_list, &QListWidget::itemActivated, this, &ManageStylesDialog::modifyStyle);
    connect(m_list, &QListWidget::currentItemChanged, this, &ManageStylesDialog::showStyle);

    // The keys of the buttons
    const auto shortcut = [this](const QKeySequence& keys, void (ManageStylesDialog::*slot)()) {
        auto* key = new QShortcut(keys, this);
        connect(key, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence(Qt::Key_Insert), &ManageStylesDialog::newStyle);
    shortcut(QKeySequence(Qt::Key_F2), &ManageStylesDialog::modifyStyle);
    shortcut(QKeySequence(Qt::Key_Delete), &ManageStylesDialog::deleteStyle);
    shortcut(QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Z)),
             &ManageStylesDialog::undo);

    // Every change of the styles shows at once, here or elsewhere
    connect(&m_styles, &StyleCoordinator::stylesChanged, this, [this]() {
        m_usageOf = m_styles.usage();
        fillList();
    });
    connect(&m_styles, &StyleCoordinator::undoChanged, this, &ManageStylesDialog::showStyle);

    m_usageOf = m_styles.usage();
    fillList();
    selectStyle(selectedId.isEmpty() ? normalId() : selectedId);
    if (m_list->currentItem() == nullptr) {
        selectStyle(normalId());
    }
    m_list->setFocus();
}

QString ManageStylesDialog::selectedStyle() const {
    const QListWidgetItem* item = m_list->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString();
}

void ManageStylesDialog::selectStyle(const QString& id) {
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (!id.isEmpty() && item->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentItem(item);
            m_list->scrollToItem(item);
            return;
        }
    }
}

void ManageStylesDialog::fillList() {
    const QString selected = selectedStyle();
    const int selectedRow = m_list->currentRow();
    const editor::ParagraphStyleSet set = m_styles.styles();
    QSignalBlocker blocker(m_list);
    m_list->clear();

    QFont headingFont = m_list->font();
    headingFont.setBold(true);
    const qreal listPoints = headingFont.pointSizeF() > 0 ? headingFont.pointSizeF()
                                                          : QFontInfo(headingFont).pointSizeF();
    headingFont.setPointSizeF(listPoints * HEADING_SHARE);
    const auto addHeading = [this, &headingFont](const QString& text) {
        auto* heading = new QListWidgetItem(text, m_list);
        heading->setFlags(Qt::NoItemFlags);
        heading->setFont(headingFont);
        heading->setForeground(m_list->palette().color(QPalette::PlaceholderText));
    };
    const auto addStyle = [this](const editor::ParagraphStyle& style) {
        auto* item = new QListWidgetItem(style.name.isEmpty() ? style.id : style.name, m_list);
        item->setData(Qt::UserRole, style.id);
        StyleTagDelegate::tagItem(item, style.origin);
    };

    //: The heading of the styles of the book's type in Manage Styles. In Polish: Style typu „%1”
    addHeading(tr("Styles of the Type \"%1\"").arg(m_styles.typeName()));
    for (const editor::ParagraphStyle& style : set.styles()) {
        if (style.origin != editor::StyleOrigin::Own) {
            addStyle(style);
        }
    }
    const auto& all = set.styles();
    if (std::any_of(all.cbegin(), all.cend(), [](const editor::ParagraphStyle& style) {
            return style.origin == editor::StyleOrigin::Own;
        })) {
        //: The heading of the writer's own styles in Manage Styles. In Polish: Własne style tej
        //: książki
        addHeading(tr("Own Styles of This Book"));
        for (const editor::ParagraphStyle& style : all) {
            if (style.origin == editor::StyleOrigin::Own) {
                addStyle(style);
            }
        }
    }

    // The style selected stays; a deleted one gives its place to the one after it
    selectStyle(selected);
    if (m_list->currentItem() == nullptr) {
        for (int row = std::clamp(selectedRow, 0, m_list->count() - 1); row >= 0; --row) {
            if ((m_list->item(row)->flags() & Qt::ItemIsSelectable) != 0) {
                m_list->setCurrentRow(row);
                break;
            }
        }
    }
    blocker.unblock();
    showStyle();
}

void ManageStylesDialog::showStyle() {
    const QString id = selectedStyle();
    const editor::ParagraphStyleSet set = m_styles.styles();
    const editor::ParagraphStyle* style = set.find(id);
    if (style == nullptr) {
        m_name->clear();
        m_kind->clear();
        m_settings->clear();
        m_usage->clear();
        for (QPushButton* button : {m_modifyButton, m_deleteButton, m_resetButton}) {
            button->setEnabled(false);
        }
    } else {
        m_name->setText(style->name);
        switch (style->origin) {
            case editor::StyleOrigin::Own:
                //: In Polish: własny styl tej książki
                m_kind->setText(tr("a style of this book's own"));
                break;
            case editor::StyleOrigin::Changed:
                //: In Polish: styl typu „%1”, zmieniony w tej książce
                m_kind->setText(tr("a style of the type \"%1\", changed in this book")
                                    .arg(m_styles.typeName()));
                break;
            case editor::StyleOrigin::Type:
                //: In Polish: styl typu „%1”
                m_kind->setText(tr("a style of the type \"%1\"").arg(m_styles.typeName()));
                break;
        }
        m_preview->showStyle(lookOf(set, normalId()), lookOf(set, id),
                             lookOf(set, style->nextStyle.isEmpty() ? id : style->nextStyle));
        m_settings->setText(settingsText(set, *style));

        const StyleCoordinator::Usage used = m_usageOf.value(id);
        m_usage->setText(
            used.paragraphs == 0
                //: In Polish: Nie używa go żaden akapit.
                ? tr("No paragraph uses it.")
                //: In Polish: Używa go %n akapit %1. / Używają go %n akapity %1. / Używa go %n
                //: akapitów %1.
                : tr("%n paragraph(s) %1 use it.", nullptr, used.paragraphs)
                      //: Part of how many paragraphs use a style. In Polish: w %n tekście / w %n
                      //: tekstach / w %n tekstach
                      .arg(tr("in %n text(s)", nullptr, used.texts)));

        const bool canChange = m_styles.whyNotChange().isEmpty();
        m_modifyButton->setEnabled(canChange);
        const QString whyNotDelete = m_styles.whyNotDelete(id);
        m_deleteButton->setEnabled(canChange && whyNotDelete.isEmpty());
        m_deleteButton->setToolTip(whyNotDelete.isEmpty()
                                       //: In Polish: Usuń styl „%1” (Delete)
                                       ? tr("Delete the style \"%1\" (Delete)").arg(style->name)
                                       : whyNotDelete);
        const QString whyNotReset = m_styles.whyNotReset(id);
        m_resetButton->setEnabled(canChange && whyNotReset.isEmpty());
        m_resetButton->setToolTip(whyNotReset.isEmpty()
                                      //: In Polish: Styl „%1” taki jak w typie „%2”
                                      ? tr("The style \"%1\" as the type \"%2\" has it")
                                            .arg(style->name, m_styles.typeName())
                                      : whyNotReset);
        //: In Polish: Nowy styl na podstawie „%1” (Insert)
        m_newButton->setToolTip(tr("A new style based on \"%1\" (Insert)").arg(style->name));
        //: In Polish: Zmień styl „%1” (Enter, F2)
        m_modifyButton->setToolTip(tr("Modify the style \"%1\" (Enter, F2)").arg(style->name));
    }
    m_newButton->setEnabled(m_styles.whyNotChange().isEmpty());

    // What Ctrl+Z undoes
    const bool canUndo = m_styles.canUndo();
    //: In Polish: Ctrl+Z cofa %1.
    m_undoHint->setText(canUndo ? tr("Ctrl+Z undoes %1.").arg(m_styles.undoText()) : QString());
    m_undoHint->setVisible(canUndo);
}

QString ManageStylesDialog::settingsText(const editor::ParagraphStyleSet& set,
                                         const editor::ParagraphStyle& style) const {
    const QVariantMap look = lookOf(set, style.id);
    const auto value = [&look](const char* name) { return look.value(QLatin1String(name)); };
    const auto phrase = [&value](const char* name) {
        return StyleDraft::phrase(QLatin1String(name), value(name));
    };
    const auto nameOf = [&set](const QString& id) {
        const editor::ParagraphStyle* found = set.find(id);
        //: A name of a style in quotes. In Polish: „%1”
        return tr("\"%1\"").arg(found != nullptr && !found->name.isEmpty() ? found->name : id);
    };
    const QString separator = tr(", ");

    QList<std::pair<QString, QString>> rows;
    //: In Polish: Na podstawie
    rows.append({tr("Based on"), style.baseStyle.isEmpty() ? QStringLiteral("–")
                                                           : nameOf(style.baseStyle)});
    //: In Polish: Następny akapit / ten sam styl
    rows.append({tr("Next paragraph"),
                 style.nextStyle.isEmpty() ? tr("the same style") : nameOf(style.nextStyle)});

    QStringList font = {QStringLiteral("%1 %2 pt").arg(
        value("fontFamily").toString(), QLocale().toString(value("fontSize").toDouble(), 'g', 4))};
    for (const char* name : {"bold", "italic", "underline"}) {
        if (value(name).toBool()) {
            font.append(phrase(name));
        }
    }
    //: In Polish: Czcionka
    rows.append({tr("Font"), font.join(separator)});

    QStringList paragraph = {phrase("alignment"), phrase("firstLineIndent")};
    for (const char* name : {"leftMargin", "rightMargin"}) {
        if (std::abs(value(name).toDouble()) > SAME_NUMBER) {
            paragraph.append(phrase(name));
        }
    }
    //: In Polish: Akapit
    rows.append({tr("Paragraph"), paragraph.join(separator)});
    //: In Polish: Odstępy
    rows.append({tr("Spacing"), QStringList{phrase("spaceBefore"), phrase("spaceAfter"),
                                            phrase("lineHeight")}
                                    .join(separator)});

    QStringList breaks;
    for (const char* name : {"keepWithNext", "keepTogether", "pageBreakBefore"}) {
        if (value(name).toBool()) {
            breaks.append(phrase(name));
        }
    }
    if (!value("widowControl").toBool()) {
        breaks.append(phrase("widowControl"));
    }
    if (!breaks.isEmpty()) {
        //: In Polish: Podziały
        rows.append({tr("Breaks"), breaks.join(separator)});
    }
    if (!value("spellCheck").toBool()) {
        //: In Polish: Pisownia
        rows.append({tr("Spelling"), phrase("spellCheck")});
    }

    // Two columns: the part in the color of hints, then its settings
    const QString hint = palette().color(QPalette::PlaceholderText).name();
    QString html = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"1\">");
    for (const auto& [part, text] : rows) {
        html += QStringLiteral("<tr><td style=\"color:%1; padding-right:10px\">%2</td>"
                               "<td>%3</td></tr>")
                    .arg(hint, part.toHtmlEscaped(), text.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
    return html;
}

// =============================================================================
// The commands
// =============================================================================

void ManageStylesDialog::newStyle() {
    const QString id = m_styles.newStyle(selectedStyle(), false, this);
    if (!id.isEmpty()) {
        selectStyle(id);
    }
    m_list->setFocus();
}

void ManageStylesDialog::modifyStyle() {
    const QString id = selectedStyle();
    if (!id.isEmpty()) {
        m_styles.modifyStyle(id, this);
    }
    m_list->setFocus();
}

void ManageStylesDialog::deleteStyle() {
    const QString id = selectedStyle();
    if (!id.isEmpty() && m_deleteButton->isEnabled()) {
        m_styles.deleteStyle(id, this, false);
    }
    m_list->setFocus();
}

void ManageStylesDialog::resetStyle() {
    const QString id = selectedStyle();
    if (!id.isEmpty() && m_resetButton->isEnabled()) {
        m_styles.resetStyle(id, false);
    }
    m_list->setFocus();
}

void ManageStylesDialog::undo() {
    if (m_styles.canUndo()) {
        m_styles.undo();
    }
}

bool ManageStylesDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            key->modifiers() == Qt::NoModifier) {
            modifyStyle();
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

}  // namespace kalahari::gui::dialogs
