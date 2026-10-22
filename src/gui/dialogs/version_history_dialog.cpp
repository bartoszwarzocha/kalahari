/// @file version_history_dialog.cpp
/// @brief The window of the versions of a text: read, compare, bring back, take a fragment

#include "kalahari/gui/dialogs/version_history_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/version_name_dialog.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/version_texts.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int WINDOW_WIDTH = 1000;   ///< Width of the window where the screen allows
constexpr int WINDOW_HEIGHT = 680;   ///< Height of the window where the screen allows
constexpr double SCREEN_PART = 0.9;  ///< The part of the screen the window takes at most
constexpr int LIST_WIDTH = 300;      ///< Width of the list at first
constexpr int LIST_MINIMUM_WIDTH = 220;
constexpr int PREVIEW_MINIMUM_WIDTH = 320;
constexpr int ROW_PADDING = 6;       ///< Around the text of a row of the list
constexpr int ROW_LINE_GAP = 2;      ///< Between the two lines of a row
constexpr int ROW_ICON_GAP = 8;      ///< Between the icon of a row and its text

constexpr int FILE_ROLE = Qt::UserRole;          ///< The .kml file of the version of a row
constexpr int TITLE_ROLE = Qt::UserRole + 1;     ///< Its first line: description or origin
constexpr int DETAILS_ROLE = Qt::UserRole + 2;   ///< Its second line: when, words
constexpr int NAMED_ROLE = Qt::UserRole + 3;     ///< It has a description: bold first line

/// @brief A row of the list in two lines, the first bold for a version with a description
class VersionRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        const QWidget* widget = option.widget;
        const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

        // The background of the row and its focus, without its text and icon
        const QIcon icon = item.icon;
        item.text.clear();
        item.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group = option.state.testFlag(QStyle::State_Active)
                                               ? QPalette::Active
                                               : QPalette::Inactive;
        const QColor text =
            option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        const QColor details =
            selected ? text : SettingsPage::noteColorOn(option.palette.color(group, QPalette::Base));

        const QRect area = option.rect.adjusted(ROW_PADDING, ROW_PADDING, -ROW_PADDING,
                                                -ROW_PADDING);
        const int iconSize = option.decorationSize.width();
        QFont titleFont = option.font;
        titleFont.setBold(index.data(NAMED_ROLE).toBool());
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics detailsMetrics(option.font);

        painter->save();
        if (!icon.isNull()) {
            const QRect iconRect(area.left(), area.top() + (titleMetrics.height() - iconSize) / 2,
                                 iconSize, iconSize);
            icon.paint(painter, iconRect, Qt::AlignCenter,
                       selected ? QIcon::Selected : QIcon::Normal);
        }
        const int left = area.left() + iconSize + ROW_ICON_GAP;
        const int width = std::max(0, area.right() - left + 1);
        painter->setFont(titleFont);
        painter->setPen(text);
        painter->drawText(QRect(left, area.top(), width, titleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(TITLE_ROLE).toString(),
                                                  Qt::ElideRight, width));
        painter->setFont(option.font);
        painter->setPen(details);
        painter->drawText(QRect(left, area.top() + titleMetrics.height() + ROW_LINE_GAP, width,
                                detailsMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          detailsMetrics.elidedText(index.data(DETAILS_ROLE).toString(),
                                                    Qt::ElideRight, width));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QFont bold = option.font;
        bold.setBold(true);
        const QFontMetrics titleMetrics(bold);
        const QFontMetrics detailsMetrics(option.font);
        const int textWidth =
            std::max(titleMetrics.horizontalAdvance(index.data(TITLE_ROLE).toString()),
                     detailsMetrics.horizontalAdvance(index.data(DETAILS_ROLE).toString()));
        return {2 * ROW_PADDING + option.decorationSize.width() + ROW_ICON_GAP + textWidth,
                2 * ROW_PADDING + titleMetrics.height() + ROW_LINE_GAP + detailsMetrics.height()};
    }
};

/// @brief The keys of the next and the previous change: as Next To Do in the editor, Alt with
/// an arrow (Cmd+Option on macOS, where Option with an arrow moves by paragraphs)
QKeyCombination changeKeys(bool next) {
    const Qt::Key key = next ? Qt::Key_Down : Qt::Key_Up;
#ifdef Q_OS_MACOS
    return QKeyCombination(Qt::ControlModifier | Qt::AltModifier, key);
#else
    return QKeyCombination(Qt::AltModifier, key);
#endif
}

/// @brief @p text with its first letter small, for the middle of a line: "saved by you"
QString inLine(QString text) {
    if (!text.isEmpty()) {
        text[0] = text.at(0).toLower();
    }
    return text;
}

/// @brief @p text in color @p color, with a line through it or under it, for a label
QString markedText(const QString& text, const QColor& color, bool struckOut) {
    return QStringLiteral("<span style=\"color: %1; text-decoration: %2;\">%3</span>")
        .arg(color.name(), struckOut ? QStringLiteral("line-through") : QStringLiteral("underline"),
             text.toHtmlEscaped());
}

}  // namespace

VersionHistoryDialog::VersionHistoryDialog(Text text, SaveVersion saveVersion, QWidget* parent)
    : KalahariDialog(parent)
    , m_text(std::move(text))
    , m_saveVersion(std::move(saveVersion))
{
    auto& art = core::ArtProvider::getInstance();

    //: The window of File > Version History... In Polish: Historia wersji
    const QString heading = tr("Version History");
    //: %1: the title of the text, %2: the title of its book. In Polish: %1 z książki „%2”, od
    //: najnowszej wersji. Wybierz wersję, żeby ją przeczytać, porównać, przywrócić albo wziąć z
    //: niej fragment.
    const QString description =
        tr("%1 from the book \"%2\", from the newest version. Choose a version to read it, "
           "compare it, restore it or take a fragment of it.")
            .arg(m_text.title, m_text.bookTitle);
    setHeading(heading, description);
    setHeadingIcon(QStringLiteral("file.versionHistory"));
    setCompactHeading(true);

    // The list, with what changes it under it
    auto* listColumn = new QWidget(this);
    auto* listLayout = new QVBoxLayout(listColumn);
    listLayout->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(listColumn);
    m_list->setObjectName(QStringLiteral("versionList"));
    //: In Polish: Wersje
    m_list->setAccessibleName(tr("Versions"));
    m_list->setItemDelegate(new VersionRowDelegate(m_list));
    const int iconSize = art.getIconSize(core::IconContext::TreeView);
    m_list->setIconSize(QSize(iconSize, iconSize));
    m_list->setUniformItemSizes(false);
    m_list->setTextElideMode(Qt::ElideRight);
    listLayout->addWidget(m_list, 1);

    //: In Polish: Pokaż wersje zapisane &samoczynnie
    m_automaticBox = new QCheckBox(tr("Show the versions saved by the &program"), listColumn);
    m_automaticBox->setObjectName(QStringLiteral("showAutomaticVersions"));
    m_automaticBox->setChecked(true);
    listLayout->addWidget(m_automaticBox);

    //: In Polish: Zapisz &bieżący tekst jako wersję...
    m_saveButton = new QPushButton(art.getIcon(QStringLiteral("file.saveVersion"),
                                               core::IconContext::Button),
                                   tr("&Save the Current Text as a Version..."), listColumn);
    m_saveButton->setObjectName(QStringLiteral("saveCurrentVersion"));
    m_saveButton->setAutoDefault(false);
    listLayout->addWidget(m_saveButton, 0, Qt::AlignLeft);

    // The preview, with how it shows the version over it
    auto* previewColumn = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewColumn);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto* tools = new QHBoxLayout();
    //: In Polish: &Tekst wersji
    m_textButton = new QPushButton(tr("Version &Text"), previewColumn);
    m_textButton->setObjectName(QStringLiteral("versionTextMode"));
    //: In Polish: &Zmiany do bieżącego tekstu
    m_changesButton = new QPushButton(tr("&Changes to the Current Text"), previewColumn);
    m_changesButton->setObjectName(QStringLiteral("versionChangesMode"));
    auto* modes = new QButtonGroup(this);
    for (QPushButton* button : {m_textButton, m_changesButton}) {
        button->setCheckable(true);
        button->setAutoDefault(false);
        modes->addButton(button);
        tools->addWidget(button);
    }
    m_changesButton->setChecked(true);
    tools->addStretch(1);
    m_previousButton = new QToolButton(previewColumn);
    m_previousButton->setObjectName(QStringLiteral("previousChange"));
    m_previousButton->setIcon(art.getIcon(QStringLiteral("common.arrowUpward"),
                                          core::IconContext::Button));
    //: %1: its keys. In Polish: Poprzednia zmiana (%1)
    m_previousButton->setToolTip(
        tr("Previous Change (%1)").arg(ShortcutRules::keysText(changeKeys(false))));
    m_previousButton->setAccessibleName(tr("Previous Change"));
    m_nextButton = new QToolButton(previewColumn);
    m_nextButton->setObjectName(QStringLiteral("nextChange"));
    m_nextButton->setIcon(art.getIcon(QStringLiteral("common.arrowDownward"),
                                      core::IconContext::Button));
    //: %1: its keys. In Polish: Następna zmiana (%1)
    m_nextButton->setToolTip(tr("Next Change (%1)").arg(ShortcutRules::keysText(changeKeys(true))));
    m_nextButton->setAccessibleName(tr("Next Change"));
    tools->addWidget(m_previousButton);
    tools->addWidget(m_nextButton);
    previewLayout->addLayout(tools);

    m_summaryLabel = new QLabel(previewColumn);
    m_summaryLabel->setObjectName(QStringLiteral("versionSummary"));
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setWordWrap(true);
    previewLayout->addWidget(m_summaryLabel);
    m_legendLabel = new QLabel(previewColumn);
    m_legendLabel->setObjectName(QStringLiteral("versionLegend"));
    m_legendLabel->setTextFormat(Qt::RichText);
    m_legendLabel->setWordWrap(true);
    previewLayout->addWidget(m_legendLabel);

    m_preview = new editor::BookEditor(previewColumn);
    m_preview->setObjectName(QStringLiteral("versionPreview"));
    //: In Polish: Podgląd tekstu wersji, tylko do czytania
    m_preview->setAccessibleName(tr("Preview of the text of the version, read-only"));
    m_preview->setMinimumWidth(PREVIEW_MINIMUM_WIDTH);
    setUpPreview();
    previewLayout->addWidget(m_preview, 1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(listColumn);
    splitter->addWidget(previewColumn);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    listColumn->setMinimumWidth(LIST_MINIMUM_WIDTH);
    splitter->setSizes({LIST_WIDTH, WINDOW_WIDTH - LIST_WIDTH});
    contentLayout()->addWidget(splitter, 1);

    // The buttons: the description and deleting at the left; restoring, inserting and Close,
    // the default, at the right
    //: In Polish: Zmień &opis...
    m_renameButton = addSideButton(tr("Change D&escription..."));
    m_renameButton->setIcon(art.getIcon(QStringLiteral("edit.rename"), core::IconContext::Button));
    //: In Polish: &Usuń...
    m_deleteButton = addSideButton(tr("&Delete..."));
    m_deleteButton->setIcon(art.getIcon(QStringLiteral("edit.delete"), core::IconContext::Button));
    //: In Polish: &Wstaw zaznaczony fragment
    m_insertButton = buttonBox()->addButton(tr("&Insert Selected Fragment"),
                                            QDialogButtonBox::ActionRole);
    m_insertButton->setObjectName(QStringLiteral("insertVersionFragment"));
    m_insertButton->setIcon(art.getIcon(QStringLiteral("edit.paste"), core::IconContext::Button));
    //: In Polish: &Przywróć tę wersję
    m_restoreButton = buttonBox()->addButton(tr("&Restore This Version"),
                                             QDialogButtonBox::ActionRole);
    m_restoreButton->setObjectName(QStringLiteral("restoreVersion"));
    m_restoreButton->setIcon(art.getIcon(QStringLiteral("version.restore"),
                                         core::IconContext::Button));
    for (QPushButton* button : {m_insertButton, m_restoreButton}) {
        button->setAutoDefault(false);
    }
    acceptButton()->hide();
    acceptButton()->setDefault(false);
    acceptButton()->setAutoDefault(false);
    cancelButton()->setText(tr("Close"));
    cancelButton()->setDefault(true);

    // What the writer does
    connect(m_list, &QListWidget::currentItemChanged, this, [this]() { showSelected(); });
    connect(m_automaticBox, &QCheckBox::toggled, this, [this]() { fillList(QString()); });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (m_saveVersion) {
            m_saveVersion(this);
        }
        reload();
        if (!m_versions.isEmpty()) {
            selectVersion(m_versions.first().file);
        }
    });
    connect(m_textButton, &QPushButton::clicked, this, [this]() { showChanges(false); });
    connect(m_changesButton, &QPushButton::clicked, this, [this]() { showChanges(true); });
    connect(m_previousButton, &QToolButton::clicked, this, [this]() { goToChange(false); });
    connect(m_nextButton, &QToolButton::clicked, this, [this]() { goToChange(true); });
    connect(m_preview, &editor::BookEditor::selectionChanged, this,
            &VersionHistoryDialog::updateButtons);
    connect(m_renameButton, &QPushButton::clicked, this, &VersionHistoryDialog::renameSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &VersionHistoryDialog::deleteSelected);
    connect(m_insertButton, &QPushButton::clicked, this, &VersionHistoryDialog::insertSelected);
    connect(m_restoreButton, &QPushButton::clicked, this, &VersionHistoryDialog::restoreSelected);

    // The keys: F2 and Delete in the list, Alt with an arrow anywhere in the window
    auto* renameKey = new QShortcut(QKeySequence(Qt::Key_F2), m_list);
    renameKey->setContext(Qt::WidgetShortcut);
    connect(renameKey, &QShortcut::activated, this, &VersionHistoryDialog::renameSelected);
    auto* deleteKey = new QShortcut(QKeySequence(Qt::Key_Delete), m_list);
    deleteKey->setContext(Qt::WidgetShortcut);
    connect(deleteKey, &QShortcut::activated, this, &VersionHistoryDialog::deleteSelected);
    auto* nextKey = new QShortcut(QKeySequence(changeKeys(true)), this);
    connect(nextKey, &QShortcut::activated, this, [this]() { goToChange(true); });
    auto* previousKey = new QShortcut(QKeySequence(changeKeys(false)), this);
    connect(previousKey, &QShortcut::activated, this, [this]() { goToChange(false); });

    // Another change of the versions (Save keeps one on saving) shows in the list
    connect(&core::ProjectManager::getInstance(), &core::ProjectManager::versionsChanged, this,
            [this](const QString& elementId) {
                if (elementId == m_text.elementId) {
                    reload();
                }
            });

    // Tab: the list, the preview, the buttons
    setTabOrder(m_list, m_preview);
    setTabOrder(m_preview, m_textButton);
    setTabOrder(m_textButton, m_changesButton);
    setTabOrder(m_changesButton, m_previousButton);
    setTabOrder(m_previousButton, m_nextButton);
    setTabOrder(m_nextButton, m_automaticBox);
    setTabOrder(m_automaticBox, m_saveButton);

    // The writer's own changes of a book open read-only: none
    if (m_text.readOnly) {
        for (QPushButton* button :
             {m_saveButton, m_renameButton, m_deleteButton, m_insertButton, m_restoreButton}) {
            button->setEnabled(false);
            button->setToolTip(readOnlyReason());
        }
    }

    reload();
    m_list->setFocus();

    // Large enough to read and compare, within the screen
    QSize size(WINDOW_WIDTH, WINDOW_HEIGHT);
    if (const QScreen* screen = this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * SCREEN_PART);
    }
    resize(size);
}

void VersionHistoryDialog::setUpPreview() {
    editor::EditorAppearance appearance =
        m_text.editor != nullptr ? m_text.editor->appearance() : m_preview->appearance();
    // The text stays where the writer reads it
    appearance.typewriter.enabled = false;
    appearance.focusMode.enabled = false;
    m_preview->setAppearance(appearance);
    if (m_text.editor != nullptr) {
        m_preview->setParagraphStyles(m_text.editor->paragraphStyles());
        m_preview->setPaperScale(m_text.editor->paperScale());
    }
    m_preview->setWordCountRules(core::wordCountRules());
    m_preview->setReadOnly(true);
    //: In Polish: Podgląd wersji jest tylko do czytania
    m_preview->setReadOnlyReason(tr("The preview of a version can only be read"));

    // Copy and Select All in the preview and in its menu (the window's own: the main window's
    // keys do not reach a dialog)
    auto& art = core::ArtProvider::getInstance();
    auto* copy = new QAction(art.getIcon(QStringLiteral("edit.copy"), core::IconContext::Menu),
                             tr("&Copy"), m_preview);
    copy->setShortcuts(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetShortcut);
    connect(copy, &QAction::triggered, this, &VersionHistoryDialog::copySelected);
    auto* selectAll = new QAction(
        art.getIcon(QStringLiteral("edit.selectAll"), core::IconContext::Menu), tr("Select &All"),
        m_preview);
    selectAll->setShortcuts(QKeySequence::SelectAll);
    selectAll->setShortcutContext(Qt::WidgetShortcut);
    connect(selectAll, &QAction::triggered, m_preview, &editor::BookEditor::selectAll);
    m_preview->addAction(copy);
    m_preview->addAction(selectAll);
    m_preview->setContextMenuEditActions({nullptr, copy, nullptr, selectAll});
}

// =============================================================================
// The list
// =============================================================================

void VersionHistoryDialog::reload(const QString& versionFile) {
    const std::optional<core::ChapterVersions> versions =
        core::ProjectManager::getInstance().versionsOf(m_text.elementId);
    m_versions = versions ? versions->list() : QList<core::ChapterVersion>();
    // A version deleted or renamed has another text in the list; one kept again has its file
    QHash<QString, QString> kept;
    for (const core::ChapterVersion& version : std::as_const(m_versions)) {
        if (m_kml.contains(version.file)) {
            kept.insert(version.file, m_kml.value(version.file));
        }
    }
    m_kml = kept;
    fillList(versionFile);
}

void VersionHistoryDialog::fillList(const QString& versionFile) {
    const std::optional<core::ChapterVersion> selected = selectedVersion();
    QString wanted = versionFile;
    if (wanted.isEmpty() && selected) {
        wanted = selected->file;
    }
    const int oldRow = m_list->currentRow();

    auto& art = core::ArtProvider::getInstance();
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const core::ChapterVersion& version : std::as_const(m_versions)) {
            if (version.isAutomatic() && !m_automaticBox->isChecked()) {
                continue;
            }
            const bool named = !version.name.trimmed().isEmpty();
            const QString title = versionTitle(version, m_text.kind);
            QStringList details{versionTime(version.created), versionWords(version)};
            if (named) {
                details << inLine(versionOrigin(version, m_text.kind));
            }
            const QString detailsLine = details.join(QStringLiteral(" · "));
            auto* item = new QListWidgetItem(
                art.getIcon(versionIconId(version.origin), core::IconContext::TreeView),
                title + QStringLiteral(", ") + detailsLine, m_list);
            item->setData(FILE_ROLE, version.file);
            item->setData(TITLE_ROLE, title);
            item->setData(DETAILS_ROLE, detailsLine);
            item->setData(NAMED_ROLE, named);
            item->setToolTip(title + QLatin1Char('\n') + detailsLine);
        }
    }

    // The version chosen before, else the one in its row (after a delete), else the newest
    int row = -1;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(FILE_ROLE).toString() == wanted) {
            row = i;
            break;
        }
    }
    if (row < 0 && m_list->count() > 0) {
        row = std::clamp(oldRow, 0, m_list->count() - 1);
    }
    m_list->setCurrentRow(row);
    if (row < 0 || m_list->item(row)->data(FILE_ROLE).toString() != m_shownFile) {
        showSelected();
    } else {
        updateButtons();
    }
}

bool VersionHistoryDialog::selectVersion(const QString& versionFile) {
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(FILE_ROLE).toString() == versionFile) {
            m_list->setCurrentRow(i);
            return true;
        }
    }
    return false;
}

std::optional<core::ChapterVersion> VersionHistoryDialog::selectedVersion() const {
    const QListWidgetItem* item = m_list != nullptr ? m_list->currentItem() : nullptr;
    if (item == nullptr) {
        return std::nullopt;
    }
    const QString file = item->data(FILE_ROLE).toString();
    for (const core::ChapterVersion& version : m_versions) {
        if (version.file == file) {
            return version;
        }
    }
    return std::nullopt;
}

// =============================================================================
// The preview
// =============================================================================

void VersionHistoryDialog::showChanges(bool changes) {
    m_showsChanges = changes;
    m_changesButton->setChecked(changes);
    m_textButton->setChecked(!changes);
    m_shownFile.clear();
    showSelected();
}

void VersionHistoryDialog::showSelected() {
    const std::optional<core::ChapterVersion> version = selectedVersion();
    m_change = -1;
    m_preview->setSpokenWord(0, 0, 0);
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), m_text.kind}};

    if (!version) {
        m_shownFile.clear();
        m_preview->fromKml(QString());
        m_legendLabel->hide();
        if (m_versions.isEmpty()) {
            //: %1: the keys of File > Save Version (Alt+Shift+S). In Polish: {Kind}
            //: {kind:s=nie ma|p=nie mają} jeszcze wersji. Zapisz bieżący tekst jako wersję
            //: przyciskiem pod listą albo %1 w edytorze.
            m_summaryLabel->setText(
                core::fillWords(tr("The {kind} {kind:s=has|p=have} no versions yet. Save the "
                                   "current text as a version with the button under the list, "
                                   "or with %1 in the editor."),
                                nouns)
                    .arg(saveVersionKeys()));
        } else {
            //: In Polish: Brak wersji zapisanych przez Ciebie. Wersje zapisane samoczynnie są
            //: ukryte.
            m_summaryLabel->setText(tr("No versions saved by you. The versions saved by the "
                                       "program are hidden."));
        }
        updateButtons();
        return;
    }

    m_shownFile = version->file;
    const std::optional<QString> kml = versionKml(*version);
    if (!kml) {
        m_preview->fromKml(QString());
        m_legendLabel->hide();
        //: In Polish: Nie można odczytać pliku tej wersji.
        m_summaryLabel->setText(tr("The file of this version cannot be read."));
        updateButtons();
        return;
    }

    if (!m_showsChanges) {
        m_preview->fromKml(*kml);
        m_legendLabel->hide();
        //: %1: the words of the version, %2: of the text as it is now. In Polish: Tekst
        //: wersji: %1 · bieżący tekst: %2
        m_summaryLabel->setText(
            tr("Version text: %1 · current text: %2")
                .arg(versionWords(*version),
                     wordCountText(m_text.editor != nullptr
                                       ? static_cast<int>(m_text.editor->wordCount())
                                       : 0)));
        updateButtons();
        return;
    }

    const editor::VersionComparison* comparison = comparisonOf(*version);
    m_preview->fromKml(comparison->kml);
    const editor::ComparisonColors colors = comparisonColors();
    // The marks as they are on the window's background
    const QColor window = palette().color(QPalette::Window);
    const bool optimize = core::ThemeManager::getInstance().optimizesEditorColors();
    //: The legend of the changes: %1 is "struck out" in its look. In Polish: %1 – jest w tej
    //: wersji, nie ma w bieżącym tekście
    const QString deleted =
        tr("%1 – in this version, not in the current text")
            .arg(markedText(tr("struck out"), shownColor(colors.deleted, window, optimize), true));
    //: The legend of the changes: %1 is "underlined" in its look. In Polish: %1 – dopisane
    //: później
    const QString added =
        tr("%1 – written later")
            .arg(markedText(tr("underlined"), shownColor(colors.added, window, optimize), false));
    m_legendLabel->setText(deleted + QStringLiteral("&nbsp;&nbsp;&nbsp;") + added);
    m_legendLabel->show();
    if (comparison->isSame()) {
        //: In Polish: Bieżący tekst jest taki sam jak tekst tej wersji.
        m_summaryLabel->setText(tr("The current text is the same as the text of this version."));
    } else {
        showChange(0);
    }
    updateButtons();
}

QString VersionHistoryDialog::summary() const {
    return m_summaryLabel->text();
}

void VersionHistoryDialog::goToChange(bool next) {
    if (!m_showsChanges) {
        return;
    }
    const auto found = m_comparisons.constFind(m_shownFile);
    if (m_shownFile.isEmpty() || found == m_comparisons.cend() || found->isSame()) {
        return;
    }
    const auto count = static_cast<int>(found->changes.size());
    showChange(m_change < 0 ? (next ? 0 : count - 1)
                            : (m_change + (next ? 1 : count - 1)) % count);
}

void VersionHistoryDialog::showChange(int index) {
    const auto found = m_comparisons.constFind(m_shownFile);
    if (found == m_comparisons.cend() || index < 0 || index >= found->changes.size()) {
        return;
    }
    const editor::VersionComparison& comparison = *found;
    m_change = index;

    // The cursor at the start of the change, in view, and the change marked in its paragraph
    const editor::ComparisonRange range = comparison.changes.at(index);
    if (const QTextDocument* document = m_preview->textDocument()) {
        const QTextBlock block = document->findBlock(range.start);
        const int offset = range.start - block.position();
        const int length = std::max(
            0, std::min(range.end, block.position() + block.length() - 1) - range.start);
        m_preview->clearSelection();
        m_preview->setCursorPosition({block.blockNumber(), offset});
        m_preview->ensureCursorVisible();
        m_preview->setSpokenWord(block.blockNumber(), offset, length);
    }
    //: %1: the change shown, %2: of how many, %3 and %4: the words deleted and added. In
    //: Polish: Zmiana %1 z %2 · usunięte słowa: %3 · dopisane: %4
    m_summaryLabel->setText(tr("Change %1 of %2 · deleted words: %3 · added: %4")
                                .arg(index + 1)
                                .arg(comparison.changes.size())
                                .arg(comparison.deletedWords)
                                .arg(comparison.addedWords));
}

std::optional<QString> VersionHistoryDialog::versionKml(const core::ChapterVersion& version) {
    if (const auto cached = m_kml.constFind(version.file); cached != m_kml.cend()) {
        return *cached;
    }
    const std::optional<core::ChapterVersions> versions =
        core::ProjectManager::getInstance().versionsOf(m_text.elementId);
    std::optional<QString> kml = versions ? versions->text(version.file) : std::nullopt;
    if (kml) {
        m_kml.insert(version.file, *kml);
    }
    return kml;
}

const editor::VersionComparison* VersionHistoryDialog::comparisonOf(
    const core::ChapterVersion& version) {
    if (const auto cached = m_comparisons.constFind(version.file);
        cached != m_comparisons.cend()) {
        return &*cached;
    }
    const std::optional<QString> kml = versionKml(version);
    const auto inserted = m_comparisons.insert(
        version.file,
        editor::compareVersions(kml.value_or(QString()), m_text.kml, comparisonColors()));
    return &*inserted;
}

editor::ComparisonColors VersionHistoryDialog::comparisonColors() const {
    // As the editor shows the marks on its paper: readable on it when the theme says so
    const editor::EditorAppearance& appearance = m_preview->appearance();
    const QColor paper = appearance.colors.background(appearance.colorMode);
    const auto& themes = core::ThemeManager::getInstance();
    const bool optimize = themes.optimizesEditorColors();
    return {shownColor(themes.editorColor(editor::DELETED_COLOR_KEY, QColor(0xc6, 0x28, 0x28)),
                       paper, optimize),
            shownColor(themes.editorColor(editor::ADDED_COLOR_KEY, QColor(0x2e, 0x7d, 0x32)),
                       paper, optimize)};
}

// =============================================================================
// The buttons
// =============================================================================

void VersionHistoryDialog::updateButtons() {
    const bool chosen = selectedVersion().has_value() && !m_shownFile.isEmpty();
    const bool changesShown = m_showsChanges && chosen;
    const auto found = chosen ? m_comparisons.constFind(m_shownFile) : m_comparisons.cend();
    const bool hasChanges = changesShown && found != m_comparisons.cend() && !found->isSame();
    m_previousButton->setEnabled(hasChanges);
    m_nextButton->setEnabled(hasChanges);
    if (m_text.readOnly) {
        return;  // the buttons that change something say why they are greyed out
    }
    m_renameButton->setEnabled(chosen);
    m_deleteButton->setEnabled(chosen);
    m_restoreButton->setEnabled(chosen && m_kml.contains(m_shownFile));
    //: In Polish: Tekst wersji zastąpi bieżący tekst. Bieżący tekst program najpierw zapisze
    //: jako wersję, a Ctrl+Z w edytorze cofnie przywrócenie.
    m_restoreButton->setToolTip(tr("The text of the version takes the place of the current "
                                   "text. The program keeps the current text as a version "
                                   "first, and Ctrl+Z in the editor undoes the restoring."));
    const bool fragment = chosen && !selectedFragment().isEmpty();
    m_insertButton->setEnabled(fragment);
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), m_text.kind}};
    m_insertButton->setToolTip(
        fragment
            //: In Polish: Wstaw zaznaczony fragment w miejscu kursora w {kind:locative}
            ? core::fillWords(tr("Insert the selected fragment at the cursor in the {kind}"),
                              nouns)
            //: In Polish: Zaznacz fragment w podglądzie
            : tr("Select a fragment in the preview"));
}

QString VersionHistoryDialog::selectedFragment() const {
    const QTextCursor selection = m_preview->selectionCursor();
    if (selection.isNull() || !selection.hasSelection()) {
        return QString();
    }
    const auto kml = m_kml.constFind(m_shownFile);
    if (kml == m_kml.cend()) {
        return QString();
    }
    // The text of the version: in its changes, without what was written later
    int start = selection.selectionStart();
    int end = selection.selectionEnd();
    if (m_showsChanges) {
        const auto comparison = m_comparisons.constFind(m_shownFile);
        if (comparison == m_comparisons.cend()) {
            return QString();
        }
        const editor::ComparisonRange range = comparison->olderRange(start, end);
        if (range.isEmpty()) {
            return QString();
        }
        start = range.start;
        end = range.end;
    }
    return editor::kmlFragment(*kml, start, end);
}

void VersionHistoryDialog::copySelected() {
    if (m_showsChanges) {
        const QString fragment = selectedFragment();
        if (!fragment.isEmpty()) {
            QGuiApplication::clipboard()->setMimeData(
                editor::ClipboardHandler::mimeDataOf(fragment).release());
            return;
        }
    }
    m_preview->copy();
}

void VersionHistoryDialog::renameSelected() {
    const std::optional<core::ChapterVersion> version = selectedVersion();
    if (!version || m_text.readOnly) {
        return;
    }
    VersionNameDialog dialog(versionTime(version->created),
                             versionOrigin(*version, m_text.kind), version->name, this);
    if (dialog.exec() != QDialog::Accepted || dialog.name() == version->name.trimmed()) {
        return;
    }
    if (!core::ProjectManager::getInstance().renameVersion(m_text.elementId, version->file,
                                                           dialog.name())) {
        //: In Polish: Nie można zmienić opisu
        MessageDialog::error(this, tr("Description Not Changed"),
                             //: In Polish: Nie można zapisać nowego opisu wersji na dysku.
                             tr("The new description of the version cannot be written on the "
                                "disk."));
    }
    reload(version->file);
}

void VersionHistoryDialog::deleteSelected() {
    const std::optional<core::ChapterVersion> version = selectedVersion();
    if (!version || m_text.readOnly) {
        return;
    }
    //: %1: when the version was kept (today, 14:52). In Polish: Usunąć wersję z %1?
    const QString question = tr("Delete the version of %1?").arg(versionTime(version->created));
    //: %1: the version's description or where it comes from. In Polish: %1. Usunięcia nie można
    //: cofnąć.
    const QString text = tr("%1. Deleting it cannot be undone.")
                             .arg(versionTitle(*version, m_text.kind));
    if (!MessageDialog::confirm(this, question, text, tr("&Delete"), MessageDialog::Kind::Warning,
                                MessageDialog::DefaultButton::Cancel)) {
        return;
    }
    if (!core::ProjectManager::getInstance().removeVersion(m_text.elementId, version->file)) {
        //: In Polish: Nie usunięto wersji
        MessageDialog::error(this, tr("Version Not Deleted"),
                             //: In Polish: Nie można zapisać listy wersji na dysku.
                             tr("The list of the versions cannot be written on the disk."));
    }
    reload();
}

void VersionHistoryDialog::restoreSelected() {
    const std::optional<core::ChapterVersion> version = selectedVersion();
    if (!version || m_text.readOnly) {
        return;
    }
    const std::optional<QString> kml = versionKml(*version);
    if (!kml) {
        return;
    }
    m_choice = Choice::Restore;
    m_chosen = *version;
    m_chosenKml = *kml;
    accept();
}

void VersionHistoryDialog::insertSelected() {
    const std::optional<core::ChapterVersion> version = selectedVersion();
    const QString fragment = selectedFragment();
    if (!version || fragment.isEmpty() || m_text.readOnly) {
        return;
    }
    m_choice = Choice::Insert;
    m_chosen = *version;
    m_chosenKml = fragment;
    accept();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
