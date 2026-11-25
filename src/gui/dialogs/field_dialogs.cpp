// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file field_dialogs.cpp
/// @brief The windows of the fields: Insert > Date && Time... and Insert > Field...

#include "kalahari/gui/dialogs/field_dialogs.h"

#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include <QButtonGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace kalahari::gui::dialogs {

namespace {

/// The width of the lists, in average characters
constexpr int LIST_WIDTH_CHARS = 30;

/// The number the formats of the word count show
constexpr int EXAMPLE_WORDS = 12345;

/// The columns of the list of the own fields
constexpr int OWN_NAME_COLUMN = 0;
constexpr int OWN_VALUE_COLUMN = 1;
constexpr int OWN_PLACE_COLUMN = 2;

/// The lines the list of the own fields shows at least
constexpr int OWN_LINES = 5;

/// A list as high as its lines, the lines its own (the list's own hint is higher than a small
/// screen allows)
class LinesList : public QListWidget {
public:
    LinesList(int lines, QWidget* parent)
        : QListWidget(parent),
          m_lines(lines)
    {
        setMinimumHeight(linesHeight());
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {(fontMetrics().averageCharWidth() * LIST_WIDTH_CHARS) + (2 * frameWidth()),
                linesHeight()};
    }

    // Also as low as its lines when it has fewer than a list's own least height
    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {QListWidget::minimumSizeHint().width(), linesHeight()};
    }

private:
    [[nodiscard]] int linesHeight() const
    {
        const int line = sizeHintForRow(0) > 0 ? sizeHintForRow(0) : fontMetrics().lineSpacing();
        return (line * m_lines) + (2 * frameWidth());
    }

    int m_lines;
};

/// The own fields: as high as its header and some lines, not as a tree's default height
class LinesTree : public QTreeWidget {
public:
    LinesTree(int lines, QWidget* parent)
        : QTreeWidget(parent),
          m_lines(lines)
    {
        setMinimumHeight(linesHeight());
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {QTreeWidget::sizeHint().width(), linesHeight()};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {QTreeWidget::minimumSizeHint().width(), linesHeight()};
    }

private:
    [[nodiscard]] int linesHeight() const
    {
        const int line = fontMetrics().lineSpacing() + 4;
        return header()->sizeHint().height() + (line * m_lines) + (2 * frameWidth());
    }

    int m_lines;
};

/// A sentence under the fields, in the color of hints
QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    return hint;
}

/// The hint of Insert > Field...: as high as the longest of its texts at the width it gets, so
/// that the window keeps its height and nothing in it scrolls whichever field is chosen (a
/// font with wider letters wraps a text into more lines)
class FieldHint : public QLabel {
public:
    FieldHint(QStringList texts, QWidget* parent)
        : QLabel(parent),
          m_texts(std::move(texts))
    {
        setWordWrap(true);
        setForegroundRole(QPalette::PlaceholderText);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_measure.setWordWrap(true);
        m_measure.setAlignment(alignment());
    }

    // As wide as the widest of the texts wants: the window keeps its width too
    QSize sizeHint() const override
    {
        QSize hint = QLabel::sizeHint();
        for (const QString& text : m_texts) {
            hint = hint.expandedTo(measured(text).sizeHint());
        }
        return hint;
    }

    int heightForWidth(int width) const override
    {
        int height = QLabel::heightForWidth(width);
        for (const QString& text : m_texts) {
            height = std::max(height, measured(text).heightForWidth(width));
        }
        return height;
    }

private:
    /// A label of the same font with @p text, as this one would lay it out
    const QLabel& measured(const QString& text) const
    {
        if (m_measure.font() != font()) {
            m_measure.setFont(font());
        }
        m_measure.setText(text);
        return m_measure;
    }

    QStringList m_texts;
    mutable QLabel m_measure;  ///< Not shown: it only measures the texts
};

/// The text of a format of the fields of the titles and the author
QString titleFormatText(const QString& format)
{
    return format == QLatin1String("upper")
               //: A format of the fields of the titles and the author. In Polish:
               //: WIELKIMI LITERAMI
               ? QCoreApplication::translate("kalahari::gui::dialogs::FieldDialog", "IN CAPITALS")
               //: In Polish: Jak we właściwościach
               : QCoreApplication::translate("kalahari::gui::dialogs::FieldDialog",
                                             "As in the Properties");
}

/// The text of a format of the fields of the page and the pages: "i, ii, iii"
QString pageFormatText(const QString& format)
{
    if (format == QLatin1String("i")) {
        return QStringLiteral("i, ii, iii");
    }
    if (format == QLatin1String("I")) {
        return QStringLiteral("I, II, III");
    }
    return QStringLiteral("1, 2, 3");
}

/// The text of a format of the field of the words, as the field writes the words: "12 345" in
/// Polish, "12,345" in English
QString wordsFormatText(const QString& format, const QString& language)
{
    editor::FieldContext context;
    context.language = language;
    editor::FieldCounts counts;
    counts.words = EXAMPLE_WORDS;
    return editor::textFieldText({.type = editor::FieldType::Words, .format = format}, context,
                                 counts);
}

}  // namespace

QString fieldFormatText(editor::FieldType type, const QString& format, const QDateTime& now,
                        const QString& language)
{
    switch (type) {
        case editor::FieldType::BookTitle:
        case editor::FieldType::Author:
        case editor::FieldType::TextTitle:
        case editor::FieldType::Custom:
            return titleFormatText(format);
        case editor::FieldType::Page:
        case editor::FieldType::Pages:
            return pageFormatText(format);
        case editor::FieldType::Words:
            return wordsFormatText(format, language);
        case editor::FieldType::Date:
            return editor::dateText(now, format, language);
    }
    return format;
}

// =============================================================================
// Insert > Date & Time...
// =============================================================================

DateTimeDialog::DateTimeDialog(Data shown, QWidget* parent)
    : KalahariDialog(parent),
      m_data(std::move(shown))
{
    if (!m_data.now.isValid()) {
        m_data.now = QDateTime::currentDateTime();
    }
    //: In Polish: Data i godzina / Dzisiejsza data albo godzina w języku książki.
    setHeading(tr("Date and Time"), tr("Today's date or time in the language of the book."));
    setHeadingIcon(QStringLiteral("insert.dateTime"));
    //: In Polish: Wstaw
    setAcceptText(tr("Insert"));

    const QStringList formats = editor::fieldFormats(editor::FieldType::Date);
    m_formats = new LinesList(static_cast<int>(formats.size()), this);
    for (const QString& format : formats) {
        auto* item = new QListWidgetItem(
            fieldFormatText(editor::FieldType::Date, format, m_data.now, m_data.language),
            m_formats);
        item->setData(Qt::UserRole, format);
    }
    const qsizetype chosen = formats.indexOf(
        editor::normalizedFieldFormat(editor::FieldType::Date, m_data.format));
    m_formats->setCurrentRow(static_cast<int>(std::max<qsizetype>(0, chosen)));
    m_formats->setMinimumHeight(m_formats->sizeHint().height());
    //: In Polish: &Format
    addField(tr("&Format"), m_formats);

    //: In Polish: &Aktualizuj automatycznie (wstawia pole)
    m_asField = new QCheckBox(tr("&Update automatically (inserts a field)"), this);
    m_asField->setChecked(m_data.asField && m_data.whyNoField.isEmpty());
    if (!m_data.whyNoField.isEmpty()) {
        m_asField->setEnabled(false);
        m_asField->setToolTip(m_data.whyNoField);
    }
    contentLayout()->addWidget(m_asField);

    // The keys that insert the date and the time without the window, as the writer has them
    if (!m_data.dateKeys.isEmpty() && !m_data.timeKeys.isEmpty()) {
        //: %1 and %2 are the keys: Alt+Shift+D and Alt+Shift+T. In Polish: %1 i %2 wstawiają
        //: od razu datę i godzinę tekstem – datę w ostatnio wybranym formacie.
        contentLayout()->addWidget(hintLabel(
            tr("%1 and %2 insert the date and the time at once as text, the date in the format "
               "chosen last.")
                .arg(m_data.dateKeys, m_data.timeKeys),
            this));
    }

    connect(m_formats, &QListWidget::itemActivated, this, [this]() { accept(); });
    m_formats->setFocus();
}

QString DateTimeDialog::format() const
{
    const QListWidgetItem* item = m_formats->currentItem();
    return editor::normalizedFieldFormat(editor::FieldType::Date,
                                         item != nullptr ? item->data(Qt::UserRole).toString()
                                                         : QString());
}

bool DateTimeDialog::asField() const
{
    return m_asField->isEnabled() && m_asField->isChecked();
}

// =============================================================================
// Insert > Field...
// =============================================================================

FieldDialog::FieldDialog(Data shown, QWidget* parent)
    : KalahariDialog(parent),
      m_data(std::move(shown))
{
    if (!m_data.now.isValid()) {
        m_data.now = QDateTime::currentDateTime();
    }
    //: Insert > Field..., and Edit Field... of a field's context menu. In Polish: Pole /
    //: Wartość, która odświeża się sama.
    setHeading(tr("Field"), tr("A value that refreshes by itself."));
    // OK when it changes a field
    setAcceptText(m_data.editing ? tr("OK") : tr("Insert"));
    setHeadingIcon(QStringLiteral("insert.field"));
    //: The tooltip of the gear of Insert > Field.... In Polish: Ustawienia pól
    addSettingsButton(QString::fromLatin1(SettingsDialog::FIELDS_PAGE), tr("Field Settings"));

    const int firstTab = addTabs();
    contentLayout()->addWidget(m_tabs);
    addDetails();

    m_tabs->setCurrentIndex(firstTab);
    showTab(firstTab);
    showFormats(m_data.field.format);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int tab) {
        showTab(tab);
        showFormats(QString());
    });
    connect(m_formats, &QComboBox::currentIndexChanged, this, &FieldDialog::showPreview);
    if (firstTab == m_ownTab) {
        m_ownList->setFocus();
    } else {
        m_lists.at(static_cast<size_t>(firstTab))->setFocus();
    }
    updateButtons();
}

int FieldDialog::addTabs()
{
    // The fields in tabs, by what they show: the book, the text, the pages, the date
    const std::array<std::pair<QString, std::vector<editor::FieldType>>, 4> tabs{{
        //: A tab of Insert > Field...: the book's title and author. In Polish: &Książka
        {tr("&Book"), {editor::FieldType::BookTitle, editor::FieldType::Author}},
        //: A tab of Insert > Field...: the text's title and words. In Polish: &Tekst
        {tr("&Text"), {editor::FieldType::TextTitle, editor::FieldType::Words}},
        //: A tab of Insert > Field...: the page number and count. In Polish: &Strony
        {tr("&Pages"), {editor::FieldType::Page, editor::FieldType::Pages}},
        //: A tab of Insert > Field...: the date. In Polish: &Data
        {tr("&Date"), {editor::FieldType::Date}},
    }};
    size_t lines = 1;
    for (const auto& tab : tabs) {
        lines = std::max(lines, tab.second.size());
    }
    m_tabs = new QTabWidget(this);
    int firstTab = 0;
    for (const auto& [title, types] : tabs) {
        auto* page = new QWidget(m_tabs);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setSpacing(contentLayout()->spacing());
        QListWidget* const list = fieldList(types, static_cast<int>(lines), page);
        if (std::ranges::find(types, m_data.field.type) != types.end()) {
            firstTab = static_cast<int>(m_lists.size());
        }
        pageLayout->addWidget(list);
        m_tabs->addTab(page, title);
        m_lists.push_back(list);
        connect(list, &QListWidget::currentRowChanged, this, [this]() { showFormats(QString()); });
        connect(list, &QListWidget::itemActivated, this, [this]() { accept(); });
    }

    // The own fields of the book and of its series, the last tab
    //: A tab of Insert > Field...: the writer's own fields. In Polish: &Własne
    m_ownTab = m_tabs->addTab(ownFieldsTab(OWN_LINES), tr("O&wn"));
    if (m_data.field.type == editor::FieldType::Custom) {
        firstTab = m_ownTab;
    }
    return firstTab;
}

QWidget* FieldDialog::ownFieldsTab(int lines)
{
    auto* page = new QWidget(m_tabs);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setSpacing(contentLayout()->spacing());

    m_ownList = new LinesTree(lines, page);
    m_ownList->setObjectName(QStringLiteral("ownFields"));
    //: The list of the own fields of Insert > Field.... In Polish: Własne pola
    m_ownList->setAccessibleName(tr("Own Fields"));
    m_ownList->setRootIsDecorated(false);
    m_ownList->setUniformRowHeights(true);
    m_ownList->setAllColumnsShowFocus(true);
    //: Columns of the own fields: their name, value and where they are saved. In Polish:
    //: Nazwa / Wartość / Zapisane w
    m_ownList->setHeaderLabels({tr("Name"), tr("Value"), tr("Saved in")});
    m_ownList->header()->setStretchLastSection(false);
    m_ownList->header()->setSectionResizeMode(OWN_NAME_COLUMN, QHeaderView::ResizeToContents);
    m_ownList->header()->setSectionResizeMode(OWN_VALUE_COLUMN, QHeaderView::Stretch);
    m_ownList->header()->setSectionResizeMode(OWN_PLACE_COLUMN, QHeaderView::ResizeToContents);
    pageLayout->addWidget(m_ownList, 1);

    m_ownNote = hintLabel(QString(), page);
    pageLayout->addWidget(m_ownNote);

    auto* buttons = new QHBoxLayout();
    //: In Polish: &Nowe pole...
    m_newOwn = new QPushButton(tr("&New Field..."), page);
    //: In Polish: &Zmień...
    m_changeOwn = new QPushButton(tr("C&hange..."), page);
    //: In Polish: &Usuń
    m_deleteOwn = new QPushButton(tr("De&lete"), page);
    for (QPushButton* button : {m_newOwn, m_changeOwn, m_deleteOwn}) {
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    pageLayout->addLayout(buttons);

    connect(m_newOwn, &QPushButton::clicked, this, &FieldDialog::newOwnField);
    connect(m_changeOwn, &QPushButton::clicked, this, &FieldDialog::changeOwnField);
    connect(m_deleteOwn, &QPushButton::clicked, this, &FieldDialog::deleteOwnField);
    connect(m_ownList, &QTreeWidget::currentItemChanged, this, [this]() {
        if (m_tabs->currentIndex() == m_ownTab) {
            showFormats(QString());
        }
        updateButtons();
    });
    connect(m_ownList, &QTreeWidget::itemActivated, this, [this]() {
        if (!chosenOwnField().isEmpty()) {
            accept();
        }
    });
    fillOwnFields(m_data.field.type == editor::FieldType::Custom ? m_data.field.key
                                                                  : QString());
    return page;
}

void FieldDialog::fillOwnFields(const QString& chosen)
{
    const QString keep = chosen.isEmpty() ? chosenOwnField() : chosen;
    const QList<OwnField> fields = m_data.ownFields ? m_data.ownFields() : QList<OwnField>{};
    {
        const QSignalBlocker blocker(m_ownList);
        m_ownList->clear();
        for (const OwnField& own : fields) {
            auto* item = new QTreeWidgetItem(m_ownList);
            item->setText(OWN_NAME_COLUMN, own.name);
            if (own.value.trimmed().isEmpty()) {
                //: An own field without a value; %1: what the text shows. In Polish: bez
                //: wartości – w tekście %1
                item->setText(OWN_VALUE_COLUMN,
                              tr("no value – in the text %1").arg(QStringLiteral("[%1]").arg(own.name)));
                item->setForeground(OWN_VALUE_COLUMN, palette().placeholderText());
            } else {
                item->setText(OWN_VALUE_COLUMN, own.value.simplified());
            }
            //: Where an own field is saved: in the book alone, or in its series (all the
            //: volumes). In Polish: książce / serii
            item->setText(OWN_PLACE_COLUMN, own.series ? tr("series") : tr("book"));
            item->setData(OWN_NAME_COLUMN, Qt::UserRole, own.id);
            item->setData(OWN_NAME_COLUMN, Qt::UserRole + 1, own.name);
            if (!own.description.isEmpty()) {
                for (int column = 0; column < m_ownList->columnCount(); ++column) {
                    item->setToolTip(column, own.description);
                }
            }
            if (own.id == keep) {
                m_ownList->setCurrentItem(item);
            }
        }
        if (m_ownList->currentItem() == nullptr && m_ownList->topLevelItemCount() > 0) {
            m_ownList->setCurrentItem(m_ownList->topLevelItem(0));
        }
    }
    QString note;
    if (!m_data.ownFields) {
        //: In Polish: Tekst spoza książki nie ma własnych pól.
        note = tr("A text outside a book has no own fields.");
    } else if (fields.isEmpty()) {
        //: In Polish: Książka nie ma jeszcze własnych pól. „Nowe pole...” dodaje pole, np. imię
        //: narratora albo nazwę wydawnictwa.
        note = tr("The book has no own fields yet. New Field... adds one, such as the name of "
                  "the narrator or of the publisher.");
    } else if (!m_data.whyOwnFieldsFixed.isEmpty()) {
        note = m_data.whyOwnFieldsFixed;
    }
    m_ownNote->setText(note);
    m_ownNote->setVisible(!note.isEmpty());
    if (m_tabs->currentIndex() == m_ownTab) {
        showFormats(QString());
    }
    updateButtons();
}

QString FieldDialog::chosenOwnField() const
{
    const QTreeWidgetItem* item = m_ownList != nullptr ? m_ownList->currentItem() : nullptr;
    return item != nullptr ? item->data(OWN_NAME_COLUMN, Qt::UserRole).toString() : QString();
}

void FieldDialog::updateButtons()
{
    const bool canChange = m_data.ownFields && m_data.whyOwnFieldsFixed.isEmpty();
    const bool chosen = !chosenOwnField().isEmpty();
    m_newOwn->setEnabled(canChange && m_data.newOwnField);
    m_changeOwn->setEnabled(canChange && chosen && m_data.changeOwnField);
    m_deleteOwn->setEnabled(canChange && chosen && m_data.deleteOwnField);
    for (QPushButton* button : {m_newOwn, m_changeOwn, m_deleteOwn}) {
        button->setToolTip(button->isEnabled() ? QString() : m_data.whyOwnFieldsFixed);
    }
    if (acceptButton() != nullptr) {
        acceptButton()->setEnabled(m_tabs->currentIndex() != m_ownTab || chosen);
    }
}

void FieldDialog::newOwnField()
{
    if (!m_newOwn->isEnabled()) {
        return;
    }
    if (const QString id = m_data.newOwnField(); !id.isEmpty()) {
        fillOwnFields(id);
    }
    m_ownList->setFocus();
}

void FieldDialog::changeOwnField()
{
    if (!m_changeOwn->isEnabled()) {
        return;
    }
    const QString id = chosenOwnField();
    if (m_data.changeOwnField(id)) {
        fillOwnFields(id);
    }
    m_ownList->setFocus();
}

void FieldDialog::deleteOwnField()
{
    if (!m_deleteOwn->isEnabled()) {
        return;
    }
    const int row = m_ownList->indexOfTopLevelItem(m_ownList->currentItem());
    if (m_data.deleteOwnField(chosenOwnField())) {
        fillOwnFields(QString());
        if (QTreeWidgetItem* next = m_ownList->topLevelItem(
                std::min(row, m_ownList->topLevelItemCount() - 1))) {
            m_ownList->setCurrentItem(next);
        }
    }
    m_ownList->setFocus();
}

QListWidget* FieldDialog::fieldList(const std::vector<editor::FieldType>& types, int lines,
                                    QWidget* page) const
{
    auto* list = new LinesList(lines, page);
    //: The list of the fields of a tab of Insert > Field.... In Polish: Pola
    list->setAccessibleName(tr("Fields"));
    for (const editor::FieldType type : types) {
        const QString& name = m_data.names.at(static_cast<size_t>(type));
        auto* item =
            new QListWidgetItem(name.isEmpty() ? editor::fieldTypeTitle(type) : name, list);
        item->setData(Qt::UserRole, static_cast<int>(type));
        if (type == m_data.field.type) {
            list->setCurrentItem(item);
        }
    }
    if (list->currentRow() < 0) {
        list->setCurrentRow(0);
    }
    return list;
}

void FieldDialog::addDetails()
{
    // The format, how the field looks and the hint: in the tab shown, under its list, the
    // names of the format and the look on their left
    m_details = new QWidget(this);
    auto* details = new QGridLayout(m_details);
    details->setContentsMargins(0, 0, 0, 0);
    details->setVerticalSpacing(contentLayout()->spacing());
    details->setColumnStretch(1, 1);
    m_formats = new QComboBox(m_details);
    //: In Polish: F&ormat:
    auto* formatLabel = new QLabel(tr("F&ormat:"), m_details);
    formatLabel->setBuddy(m_formats);
    details->addWidget(formatLabel, 0, 0);
    details->addWidget(m_formats, 0, 1);

    m_preview = new QLabel(m_details);
    m_preview->setTextFormat(Qt::PlainText);
    m_preview->setMargin(fontMetrics().averageCharWidth() / 2);
    colorPreview();
    //: How the field shows in the text. In Polish: W tekście:
    details->addWidget(new QLabel(tr("In the text:"), m_details), 1, 0);
    details->addWidget(m_preview, 1, 1, Qt::AlignLeft);

    QStringList hints;
    for (const editor::FieldType type : editor::FIELD_TYPES) {
        for (const QString& format : editor::fieldFormats(type)) {
            hints << hintText({.type = type, .format = format});
        }
    }
    hints << hintText({.type = editor::FieldType::Custom, .format = QString()});
    hints.removeDuplicates();
    m_hint = new FieldHint(hints, m_details);
    details->addWidget(m_hint, 2, 0, 1, 2);
}

void FieldDialog::colorPreview()
{
    // As in the text: on the gray background of the fields, in the color of the text
    if (!m_data.background.isValid() && !m_data.text.isValid()) {
        return;
    }
    QPalette palette = m_preview->palette();
    if (m_data.background.isValid()) {
        palette.setColor(QPalette::Window, m_data.background);
        m_preview->setAutoFillBackground(true);
    }
    if (m_data.text.isValid()) {
        palette.setColor(QPalette::WindowText, m_data.text);
    }
    m_preview->setPalette(palette);
}

void FieldDialog::showTab(int tab)
{
    if (tab < 0 || tab >= m_tabs->count()) {
        return;
    }
    const QWidget* const page = m_tabs->widget(tab);
    if (const QWidget* const was = m_details->parentWidget(); was != page && was != nullptr) {
        if (QLayout* const layout = was->layout()) {
            layout->removeWidget(m_details);
        }
    }
    page->layout()->addWidget(m_details);
    m_details->show();
    // The keys go to the list of the tab, unless they move between the tabs
    if (!m_tabs->tabBar()->hasFocus() && m_tabs->isVisible()) {
        if (tab == m_ownTab) {
            m_ownList->setFocus();
        } else {
            m_lists.at(static_cast<size_t>(tab))->setFocus();
        }
    }
    updateButtons();
}

editor::TextField FieldDialog::field() const
{
    editor::TextField field;
    field.type = chosenType();
    field.format = editor::normalizedFieldFormat(field.type, m_formats->currentData().toString());
    if (field.type == editor::FieldType::Custom) {
        if (const QTreeWidgetItem* item = m_ownList->currentItem()) {
            field.key = item->data(OWN_NAME_COLUMN, Qt::UserRole).toString();
            field.name = item->data(OWN_NAME_COLUMN, Qt::UserRole + 1).toString();
        }
    }
    return field;
}

editor::FieldType FieldDialog::chosenType() const
{
    const int tab = m_tabs->currentIndex();
    if (tab == m_ownTab) {
        return editor::FieldType::Custom;
    }
    const QListWidgetItem* item = tab >= 0 && static_cast<size_t>(tab) < m_lists.size()
                                      ? m_lists.at(static_cast<size_t>(tab))->currentItem()
                                      : nullptr;
    const int type = item != nullptr ? item->data(Qt::UserRole).toInt() : 0;
    return editor::FIELD_TYPES.at(static_cast<size_t>(
        std::clamp(type, 0, static_cast<int>(editor::FIELD_TYPES.size()) - 1)));
}

void FieldDialog::showFormats(const QString& format)
{
    const editor::FieldType type = chosenType();
    {
        const QSignalBlocker blocker(m_formats);
        m_formats->clear();
        for (const QString& name : editor::fieldFormats(type)) {
            m_formats->addItem(fieldFormatText(type, name, m_data.now, m_data.language), name);
        }
        m_formats->setCurrentIndex(std::max(
            0, m_formats->findData(editor::normalizedFieldFormat(type, format))));
    }
    showPreview();
}

void FieldDialog::showPreview()
{
    const editor::TextField chosen = field();
    m_preview->setText(m_data.shown ? m_data.shown(chosen)
                                    : editor::textFieldText(chosen, editor::FieldContext{},
                                                            editor::FieldCounts{}));
    m_hint->setText(hintText(chosen));
}

QString FieldDialog::hintText(const editor::TextField& field)
{
    QString hint;
    switch (field.type) {
        case editor::FieldType::BookTitle:
            //: In Polish: Tytuł zmienia się razem z właściwościami książki.
            hint = tr("The title changes together with the book's properties.");
            break;
        case editor::FieldType::Author:
            hint = tr("The author changes together with the book's properties.");
            break;
        case editor::FieldType::TextTitle:
            hint = tr("The title changes together with the title of the text.");
            break;
        case editor::FieldType::Page:
            //: In Polish: Numer strony widać w widoku „Układ strony”; w widoku „Ciągły” pole
            //: pokazuje #.
            hint = tr("The page number shows in the Page Layout view; in the Continuous view the "
                      "field shows #.");
            break;
        case editor::FieldType::Pages:
            hint = tr("The page count shows in the Page Layout view; in the Continuous view the "
                      "field shows #.");
            break;
        case editor::FieldType::Words:
            hint = tr("The count changes as you write, as the status bar's does.");
            break;
        case editor::FieldType::Date:
            hint = field.format == QLatin1String("time") ||
                           field.format == QLatin1String("date-time")
                       //: In Polish: Godzina otwarcia tekstu albo wysunięcia go na wierzch.
                       ? tr("The time the text was opened or brought to the front.")
                       //: In Polish: Dzisiejsza data: zmienia się, gdy tekst zostanie otwarty
                       //: innego dnia.
                       : tr("Today's date: it changes when the text is opened on another day.");
            break;
        case editor::FieldType::Custom:
            //: In Polish: Nowa wartość zmienia to pole w całej książce, także w eksporcie i
            //: wydruku.
            hint = tr("A new value changes the field in the whole book, in export and printing "
                      "too.");
            break;
    }
    //: In Polish: Szare tło pól można wyłączyć w Ustawieniach.
    return hint + QLatin1Char(' ') +
           tr("The gray background of the fields can be turned off in the Settings.");
}

// =============================================================================
// New Field... and Change... of an own field
// =============================================================================

OwnFieldDialog::OwnFieldDialog(const OwnField& field, bool inSeries, NameCheck checkName,
                               QWidget* parent)
    : KalahariDialog(parent),
      m_field(field),
      m_checkName(std::move(checkName))
{
    const bool isNew = field.id.isEmpty();
    //: The window of a new own field and of a changed one. In Polish: Nowe pole / Zmień pole /
    //: Nazwa i wartość, które pole pokazuje w tekście.
    setHeading(isNew ? tr("New Field") : tr("Change Field"),
               tr("The name and the value the field shows in the text."));
    setHeadingIcon(QStringLiteral("insert.field"));
    if (isNew) {
        //: In Polish: Dodaj
        setAcceptText(tr("Add"));
    }

    m_name = new QLineEdit(field.name, this);
    //: An example of the name of an own field. In Polish: np. Imię narratora
    m_name->setPlaceholderText(tr("e.g. Narrator's Name"));
    //: In Polish: &Nazwa:
    addField(tr("&Name:"), m_name);

    m_value = new QLineEdit(field.series ? field.seriesValue : field.value, this);
    //: In Polish: może zostać puste – pole pokaże wtedy swoją nazwę
    m_value->setPlaceholderText(tr("may stay empty: the field then shows its name"));
    //: In Polish: &Wartość:
    addField(tr("&Value:"), m_value);

    m_description = new QLineEdit(field.description, this);
    //: In Polish: krótki opis do podpowiedzi
    m_description->setPlaceholderText(tr("a short description for the tooltip"));
    //: In Polish: &Opis:
    addField(tr("&Description:"), m_description);

    if (inSeries) {
        auto* places = new QWidget(this);
        auto* placesLayout = new QVBoxLayout(places);
        placesLayout->setContentsMargins(0, 0, 0, 0);
        //: Where a new own field is saved. In Polish: w tej &książce
        m_inBook = new QRadioButton(tr("in this &book"), places);
        //: In Polish: w &serii – wspólne dla wszystkich tomów
        m_inSeries = new QRadioButton(tr("in the &series: the same for all the volumes"), places);
        auto* group = new QButtonGroup(places);
        group->addButton(m_inBook);
        group->addButton(m_inSeries);
        (field.series ? m_inSeries : m_inBook)->setChecked(true);
        // A field made stays where it is: its fields in the texts of the other volumes
        m_inBook->setEnabled(isNew);
        m_inSeries->setEnabled(isNew);
        placesLayout->addWidget(m_inBook);
        placesLayout->addWidget(m_inSeries);
        //: In Polish: Zapisz w:
        addField(tr("Save in:"), places);

        auto* own = new QWidget(this);
        auto* ownLayout = new QHBoxLayout(own);
        ownLayout->setContentsMargins(0, 0, 0, 0);
        //: A field of the series: this volume has a value of its own. In Polish: &Inna wartość
        //: w tej książce:
        m_ownValue = new QCheckBox(tr("&Another value in this book:"), own);
        m_ownValue->setChecked(field.series && field.ownValue);
        m_volumeValue = new QLineEdit(field.series && field.ownValue ? field.value : QString(), own);
        ownLayout->addWidget(m_ownValue);
        ownLayout->addWidget(m_volumeValue, 1);
        contentLayout()->addWidget(own);
        connect(m_inSeries, &QRadioButton::toggled, this, &OwnFieldDialog::updateValues);
        connect(m_ownValue, &QCheckBox::toggled, this, &OwnFieldDialog::updateValues);
    }

    m_problem = hintLabel(QString(), this);
    m_problem->hide();
    contentLayout()->addWidget(m_problem);
    connect(m_name, &QLineEdit::textEdited, m_problem, &QLabel::hide);
    updateValues();
    m_name->setFocus();
}

void OwnFieldDialog::updateValues()
{
    if (m_inSeries == nullptr) {
        return;
    }
    const bool series = m_inSeries->isChecked();
    m_ownValue->setVisible(series);
    m_volumeValue->setVisible(series);
    m_volumeValue->setEnabled(m_ownValue->isChecked());
}

OwnField OwnFieldDialog::field() const
{
    OwnField field = m_field;
    field.name = m_name->text().simplified();
    field.description = m_description->text().trimmed();
    field.series = m_inSeries != nullptr && m_inSeries->isChecked();
    if (field.series) {
        field.seriesValue = m_value->text();
        field.ownValue = m_ownValue->isChecked();
        field.value = field.ownValue ? m_volumeValue->text() : field.seriesValue;
    } else {
        field.value = m_value->text();
        field.seriesValue.clear();
        field.ownValue = false;
    }
    return field;
}

void OwnFieldDialog::accept()
{
    const QString name = m_name->text().simplified();
    QString problem;
    if (name.isEmpty()) {
        //: In Polish: Pole musi mieć nazwę.
        problem = tr("The field needs a name.");
    } else if (m_checkName) {
        problem = m_checkName(name);
    }
    if (!problem.isEmpty()) {
        m_problem->setText(problem);
        m_problem->show();
        m_name->setFocus();
        m_name->selectAll();
        return;
    }
    KalahariDialog::accept();
}

// =============================================================================
// Delete of an own field used in the texts
// =============================================================================

DeleteOwnFieldDialog::DeleteOwnFieldDialog(const OwnField& field, int uses, int texts,
                                           const QString& undoKeys, QWidget* parent)
    : KalahariDialog(parent)
{
    //: In Polish: Usuń pole / Usunąć pole „%1”?
    setHeading(tr("Delete Field"), tr("Delete the field \u201c%1\u201d?").arg(field.name));
    setHeadingIcon(QStringLiteral("insert.field"));
    //: In Polish: &Usuń pole
    setAcceptText(tr("&Delete Field"));

    //: How often an own field is in the texts; the next sentence says in how many. In Polish:
    //: Pole występuje %n raz, / %n razy, / %n razy,
    m_message = tr("The field is in the text %n times,", nullptr, uses) + QLatin1Char(' ') +
                //: In Polish: w %n tekście. / w %n tekstach. / w %n tekstach.
                tr("in %n texts.", nullptr, texts);
    auto* says = new QLabel(m_message, this);
    says->setWordWrap(true);
    contentLayout()->addWidget(says);

    const QString value = field.value.simplified();
    //: In Polish: &Zamień te miejsca na zwykły tekst „%1”
    m_asText = new QRadioButton(
        tr("&Replace these places with the plain text \u201c%1\u201d").arg(value), this);
    //: In Polish: Zostaw je &jako puste pole „%1” do poprawienia
    m_keep = new QRadioButton(
        tr("&Leave them as the empty field \u201c%1\u201d, to fix")
            .arg(QStringLiteral("[%1]").arg(field.name)),
        this);
    auto* group = new QButtonGroup(this);
    group->addButton(m_asText);
    group->addButton(m_keep);
    if (value.isEmpty()) {
        // A field without a value: no text to put in its places
        m_asText->setEnabled(false);
        m_keep->setChecked(true);
    } else {
        m_asText->setChecked(true);
    }
    contentLayout()->addWidget(m_asText);
    contentLayout()->addWidget(m_keep);
    //: %1: the keys of Undo (Ctrl+Z). In Polish: %1 w otwartym tekście i „Cofnij” w komunikacie
    //: na dole okna przywracają pole wszędzie.
    contentLayout()->addWidget(hintLabel(
        tr("%1 in an open text and Undo in the message at the bottom of the window bring the "
           "field back everywhere.")
            .arg(undoKeys.isEmpty() ? tr("Undo") : undoKeys),
        this));

    //: In Polish: &Kopiuj
    QPushButton* copy = addSideButton(tr("&Copy"));
    connect(copy, &QPushButton::clicked, this, [this]() {
        if (QClipboard* clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(message());
        }
    });
    (m_asText->isEnabled() ? m_asText : m_keep)->setFocus();
}

bool DeleteOwnFieldDialog::asText() const
{
    return m_asText->isEnabled() && m_asText->isChecked();
}

QString DeleteOwnFieldDialog::message() const
{
    return headingDescription() + QLatin1Char('\n') + m_message;
}

}  // namespace kalahari::gui::dialogs
