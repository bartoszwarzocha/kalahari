/// @file field_dialogs.cpp
/// @brief The windows of the fields: Insert > Date && Time... and Insert > Field...

#include "kalahari/gui/dialogs/field_dialogs.h"

#include "kalahari/gui/settings_dialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The width of the lists, in average characters
constexpr int LIST_WIDTH_CHARS = 30;

/// The lines of the hint of Insert > Field... (the window keeps its height for every field)
constexpr int HINT_LINES = 3;

/// The number the formats of the word count show
constexpr int EXAMPLE_WORDS = 12345;

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

    QSize sizeHint() const override
    {
        return {fontMetrics().averageCharWidth() * LIST_WIDTH_CHARS + 2 * frameWidth(),
                linesHeight()};
    }

private:
    [[nodiscard]] int linesHeight() const
    {
        return sizeHintForRow(0) > 0 ? sizeHintForRow(0) * m_lines + 2 * frameWidth()
                                     : fontMetrics().lineSpacing() * m_lines + 2 * frameWidth();
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

}  // namespace

QString fieldFormatText(editor::FieldType type, const QString& format, const QDateTime& now,
                        const QString& language)
{
    switch (type) {
        case editor::FieldType::BookTitle:
        case editor::FieldType::Author:
        case editor::FieldType::TextTitle:
            return format == QLatin1String("upper")
                       //: A format of the fields of the titles and the author. In Polish:
                       //: WIELKIMI LITERAMI
                       ? QCoreApplication::translate("kalahari::gui::dialogs::FieldDialog",
                                                     "IN CAPITALS")
                       //: In Polish: Jak we właściwościach
                       : QCoreApplication::translate("kalahari::gui::dialogs::FieldDialog",
                                                     "As in the Properties");
        case editor::FieldType::Page:
        case editor::FieldType::Pages:
            if (format == QLatin1String("i")) {
                return QStringLiteral("i, ii, iii");
            }
            if (format == QLatin1String("I")) {
                return QStringLiteral("I, II, III");
            }
            return QStringLiteral("1, 2, 3");
        case editor::FieldType::Words: {
            // As the field writes the words: "12 345" in Polish, "12,345" in English
            editor::FieldContext context;
            context.language = language;
            editor::FieldCounts counts;
            counts.words = EXAMPLE_WORDS;
            return editor::textFieldText({type, format}, context, counts);
        }
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

    m_fields = new LinesList(static_cast<int>(editor::FIELD_TYPES.size()), this);
    for (size_t i = 0; i < editor::FIELD_TYPES.size(); ++i) {
        const editor::FieldType type = editor::FIELD_TYPES.at(i);
        const QString name = m_data.names.at(i).isEmpty() ? editor::fieldTypeTitle(type)
                                                          : m_data.names.at(i);
        auto* item = new QListWidgetItem(name, m_fields);
        item->setData(Qt::UserRole, static_cast<int>(type));
    }
    m_fields->setCurrentRow(static_cast<int>(m_data.field.type));
    m_fields->setMinimumHeight(m_fields->sizeHint().height());
    //: The list of the fields. In Polish: &Pole
    addField(tr("&Field"), m_fields);

    m_formats = new QComboBox(this);
    //: In Polish: F&ormat
    addField(tr("F&ormat"), m_formats);

    // As in the text: on the gray background of the fields, in the color of the text
    m_preview = new QLabel(this);
    m_preview->setTextFormat(Qt::PlainText);
    m_preview->setMargin(fontMetrics().averageCharWidth() / 2);
    if (m_data.background.isValid() || m_data.text.isValid()) {
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
    auto* previewRow = new QHBoxLayout();
    previewRow->addWidget(m_preview);
    previewRow->addStretch(1);
    //: How the field shows in the text. In Polish: W tekście
    auto* previewLabel = new QLabel(tr("In the text:"), this);
    auto* previewBox = new QVBoxLayout();
    previewBox->setSpacing(fontMetrics().lineSpacing() / 4);
    previewBox->addWidget(previewLabel);
    previewBox->addLayout(previewRow);
    contentLayout()->addLayout(previewBox);

    m_hint = hintLabel(QString(), this);
    m_hint->setMinimumHeight(m_hint->fontMetrics().lineSpacing() * HINT_LINES);
    m_hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    contentLayout()->addWidget(m_hint);

    showFormats(m_data.field.format);
    connect(m_fields, &QListWidget::currentRowChanged, this, [this]() { showFormats(QString()); });
    connect(m_formats, &QComboBox::currentIndexChanged, this, &FieldDialog::showPreview);
    connect(m_fields, &QListWidget::itemActivated, this, [this]() { accept(); });
    m_fields->setFocus();
}

editor::TextField FieldDialog::field() const
{
    editor::TextField field;
    field.type = chosenType();
    field.format = editor::normalizedFieldFormat(field.type, m_formats->currentData().toString());
    return field;
}

editor::FieldType FieldDialog::chosenType() const
{
    const QListWidgetItem* item = m_fields->currentItem();
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
    QString hint;
    switch (chosen.type) {
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
            hint = chosen.format == QLatin1String("time") ||
                           chosen.format == QLatin1String("date-time")
                       //: In Polish: Godzina otwarcia tekstu albo wysunięcia go na wierzch.
                       ? tr("The time the text was opened or brought to the front.")
                       //: In Polish: Dzisiejsza data: zmienia się, gdy tekst zostanie otwarty
                       //: innego dnia.
                       : tr("Today's date: it changes when the text is opened on another day.");
            break;
    }
    //: In Polish: Szare tło pól można wyłączyć w Ustawieniach.
    m_hint->setText(hint + QLatin1Char(' ') +
                    tr("The gray background of the fields can be turned off in the Settings."));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
