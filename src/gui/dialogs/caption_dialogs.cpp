// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file caption_dialogs.cpp
/// @brief The windows of the numbered captions: Insert Caption, Caption Numbering and Insert
///        Cross-Reference

#include "kalahari/gui/dialogs/caption_dialogs.h"

#include "kalahari/editor/text_link.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings_dialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The width of the lists of choices, in average characters
constexpr int CHOICE_WIDTH_CHARS = 18;

/// The width of the caption's text, in average characters
constexpr int TEXT_WIDTH_CHARS = 36;

/// The height of the list of captions, in lines
constexpr int TARGET_LINES = 6;

/// The list of the captions of Insert Cross-Reference: as high as its lines want it (the
/// list's own hint is higher than a small screen allows), higher with the window
class TargetList : public QListWidget {
public:
    explicit TargetList(QWidget* parent)
        : QListWidget(parent)
    {
        setMinimumHeight(linesHeight());
    }

    QSize sizeHint() const override
    {
        return {QListWidget::sizeHint().width(), linesHeight()};
    }

private:
    [[nodiscard]] int linesHeight() const
    {
        return fontMetrics().lineSpacing() * TARGET_LINES + 2 * frameWidth();
    }
};

/// A label of a field in a grid: its text, its buddy
QLabel* fieldLabel(const QString& text, QWidget* field, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

/// A sentence under the fields, in the color of hints
QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    return hint;
}

/// The number formats of captions, in the order of the list
const std::vector<editor::ListNumberFormat>& captionFormats()
{
    static const std::vector<editor::ListNumberFormat> formats{
        editor::ListNumberFormat::Decimal, editor::ListNumberFormat::LowerAlpha,
        editor::ListNumberFormat::UpperAlpha, editor::ListNumberFormat::LowerRoman,
        editor::ListNumberFormat::UpperRoman};
    return formats;
}

/// "1, 2, 3" in a number format
QString formatSample(editor::ListNumberFormat format)
{
    return QStringLiteral("%1, %2, %3")
        .arg(editor::listNumberText(1, format), editor::listNumberText(2, format),
             editor::listNumberText(3, format));
}

} // namespace

// =============================================================================
// Insert Caption
// =============================================================================

InsertCaptionDialog::InsertCaptionDialog(Data shown, QWidget* parent)
    : KalahariDialog(parent),
      m_data(std::move(shown)),
      m_labels(m_data.labels)
{
    //: In Polish: Wstaw podpis / Podpis obrazu z numerem nadawanym przez program.
    setHeading(tr("Insert Caption"), tr("The caption of the image, with a number the program gives."));
    setHeadingIcon(QStringLiteral("insert.caption"));
    setAcceptText(tr("OK"));
    //: The tooltip of the gear of the windows of images. In Polish: Ustawienia obrazów
    addSettingsButton(QString::fromLatin1(SettingsDialog::IMAGES_PAGE), tr("Image Settings"));

    // A label of an older caption the book's list does not have
    const QString wanted = editor::normalizedLabel(m_data.caption.label);
    if (!wanted.isEmpty() && !m_labels.contains(wanted)) {
        m_labels << wanted;
    }

    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    const int fieldWidth = fontMetrics().averageCharWidth() * CHOICE_WIDTH_CHARS;

    // The caption: its label and number, which the program gives, then the writer's text
    m_prefix = new QLabel(this);
    m_text = new QLineEdit(this);
    m_text->setText(m_data.caption.text);
    m_text->setMinimumWidth(fontMetrics().averageCharWidth() * TEXT_WIDTH_CHARS);
    auto* captionRow = new QHBoxLayout();
    captionRow->addWidget(m_prefix);
    captionRow->addWidget(m_text, 1);
    //: In Polish: Po&dpis
    grid->addWidget(fieldLabel(tr("&Caption"), m_text, this), 0, 0);
    grid->addLayout(captionRow, 0, 1);

    // The label, and the labels of the book
    m_label = new QComboBox(this);
    m_label->setFixedWidth(fieldWidth);
    //: In Polish: &Nowa etykieta…
    m_newLabel = new QPushButton(tr("&New Label..."), this);
    //: Takes a label off the list of the book. In Polish: &Usuń
    m_removeLabel = new QPushButton(tr("&Remove"), this);
    auto* labelRow = new QHBoxLayout();
    labelRow->addWidget(m_label);
    labelRow->addWidget(m_newLabel);
    labelRow->addWidget(m_removeLabel);
    labelRow->addStretch(1);
    //: In Polish: &Etykieta
    grid->addWidget(fieldLabel(tr("&Label"), m_label, this), 1, 0);
    grid->addLayout(labelRow, 1, 1);

    // Under the image or above it
    m_position = new QComboBox(this);
    m_position->setFixedWidth(fieldWidth);
    //: In Polish: Pod obrazem
    m_position->addItem(tr("Below the image"), false);
    //: In Polish: Nad obrazem
    m_position->addItem(tr("Above the image"), true);
    m_position->setCurrentIndex(m_data.caption.above ? 1 : 0);
    auto* positionRow = new QHBoxLayout();
    positionRow->addWidget(m_position);
    positionRow->addStretch(1);
    //: In Polish: &Położenie
    grid->addWidget(fieldLabel(tr("&Position"), m_position, this), 2, 0);
    grid->addLayout(positionRow, 2, 1);
    contentLayout()->addLayout(grid);

    //: In Polish: &Bez etykiety i numeru – sam tekst
    m_unnumbered = new QCheckBox(tr("&Without a label and number – the text alone"), this);
    m_unnumbered->setChecked(m_data.caption.shown && !m_data.caption.numbered);
    contentLayout()->addWidget(m_unnumbered);

    // How the book numbers the label
    //: In Polish: Nu&meracja…
    m_numbering = new QPushButton(tr("Nu&mbering..."), this);
    m_numberingHint = hintLabel(QString(), this);
    auto* numberingRow = new QHBoxLayout();
    numberingRow->addWidget(m_numbering);
    numberingRow->addWidget(m_numberingHint, 1);
    contentLayout()->addLayout(numberingRow);

    //: In Polish: Etykieta to rodzaj obrazu; każda ma własną numerację („Mapa 1”, „Mapa 2” obok
    //: „Rysunek 1”, „Rysunek 2”). Podpis jest akapitem stylu „Podpis”.
    contentLayout()->addWidget(hintLabel(
        tr("A label is a kind of image, and each has its own numbering (\"Map 1\", \"Map 2\" "
           "next to \"Figure 1\", \"Figure 2\"). The caption is a paragraph in the style Caption."),
        this));
    contentLayout()->addStretch(1);

    // An image with a caption can lose it
    if (m_data.caption.shown) {
        //: In Polish: Usuń p&odpis
        QPushButton* remove = addSideButton(tr("Remove Capt&ion"));
        connect(remove, &QPushButton::clicked, this, [this]() {
            m_removed = true;
            accept();
        });
    }

    showLabels(wanted.isEmpty() ? m_labels.value(0) : wanted);
    connect(m_label, &QComboBox::currentIndexChanged, this, &InsertCaptionDialog::updateFields);
    connect(m_unnumbered, &QCheckBox::toggled, this, &InsertCaptionDialog::updateFields);
    connect(m_text, &QLineEdit::textChanged, this, &InsertCaptionDialog::updateFields);
    connect(m_newLabel, &QPushButton::clicked, this, &InsertCaptionDialog::addLabel);
    connect(m_removeLabel, &QPushButton::clicked, this, &InsertCaptionDialog::removeLabel);
    connect(m_numbering, &QPushButton::clicked, this, &InsertCaptionDialog::editNumbering);
    updateFields();

    m_text->setFocus();
    m_text->end(false);
}

editor::ImageCaption InsertCaptionDialog::caption() const
{
    editor::ImageCaption caption;
    caption.numbered = !m_unnumbered->isChecked();
    caption.label = caption.numbered ? m_label->currentText() : QString();
    caption.text = m_text->text().trimmed();
    caption.above = m_position->currentData().toBool();
    caption.shown = !m_removed && (caption.numbered ? !caption.label.isEmpty() : !caption.text.isEmpty());
    return caption;
}

void InsertCaptionDialog::showLabels(const QString& current)
{
    const QSignalBlocker blocker(m_label);
    m_label->clear();
    m_label->addItems(m_labels);
    m_label->setCurrentIndex(std::max(0, static_cast<int>(m_labels.indexOf(current))));
}

void InsertCaptionDialog::addLabel()
{
    //: The window of a new label of captions. In Polish: Nowa etykieta / Etykieta
    const std::optional<QString> typed = TextInputDialog::getText(this, tr("New Label"), tr("Label"));
    if (!typed) {
        return;
    }
    const QString label = editor::normalizedLabel(*typed);
    if (label.isEmpty()) {
        return;
    }
    // A label the book has already, in other letters: that one
    const auto found = std::find_if(m_labels.cbegin(), m_labels.cend(), [&label](const QString& each) {
        return each.compare(label, Qt::CaseInsensitive) == 0;
    });
    // Chosen before the list grows: an append leaves the iterator behind
    const QString chosen = found == m_labels.cend() ? label : *found;
    if (!m_labels.contains(chosen)) {
        m_labels << chosen;
    }
    showLabels(chosen);
    m_unnumbered->setChecked(false);
    updateFields();
}

void InsertCaptionDialog::removeLabel()
{
    const QString label = m_label->currentText();
    if (m_labels.size() <= 1 || m_data.usedLabels.contains(label)) {
        return;
    }
    const int index = m_label->currentIndex();
    m_labels.removeAll(label);
    showLabels(m_labels.value(std::min(index, static_cast<int>(m_labels.size()) - 1)));
    updateFields();
}

void InsertCaptionDialog::editNumbering()
{
    if (m_data.editNumbering && m_data.editNumbering(m_label->currentText(), this)) {
        updateFields();  // the number may be another now
    }
}

void InsertCaptionDialog::updateFields()
{
    const bool numbered = !m_unnumbered->isChecked();
    const QString label = m_label->currentText();
    m_label->setEnabled(numbered);
    m_newLabel->setEnabled(numbered);

    // A label the captions of the book have stays, and one label always stays
    const bool used = m_data.usedLabels.contains(label);
    m_removeLabel->setEnabled(numbered && !used && m_labels.size() > 1);
    //: Why Remove is greyed out. In Polish: Tej etykiety używają podpisy w książce
    m_removeLabel->setToolTip(used ? tr("Captions of the book have this label") : QString());

    // "Figure 3." before the text
    const QString number = m_data.numberOf ? m_data.numberOf(label) : QString();
    m_prefix->setText(QStringLiteral("%1 %2.").arg(label, number));
    m_prefix->setVisible(numbered && !label.isEmpty());

    m_numbering->setEnabled(numbered && static_cast<bool>(m_data.editNumbering));
    m_numbering->setToolTip(m_data.editNumbering
                                ? QString()
                                //: In Polish: Plik spoza książki jest numerowany według
                                //: Ustawień (koło zębate).
                                : tr("A file outside a book is numbered as the Settings say "
                                     "(the gear)."));
    m_numberingHint->setText(numbered && m_data.numberingOf
                                 ? CaptionNumberingDialog::describe(m_data.numberingOf(label))
                                 : QString());

    // A caption without a number has its text
    acceptButton()->setEnabled(numbered ? !label.isEmpty() : !m_text->text().trimmed().isEmpty());
}

// =============================================================================
// Caption Numbering
// =============================================================================

CaptionNumberingDialog::CaptionNumberingDialog(const QString& label,
                                               const editor::CaptionNumbering& numbering,
                                               QWidget* parent)
    : KalahariDialog(parent),
      m_label(label)
{
    const editor::CaptionNumbering current = editor::normalizedNumbering(numbering);
    //: In Polish: Numeracja podpisów / Jak ta książka numeruje obrazy z etykietą „%1”.
    setHeading(tr("Caption Numbering"),
               tr("How this book numbers the images with the label \"%1\".").arg(label));
    setHeadingIcon(QStringLiteral("insert.caption"));
    setAcceptText(tr("OK"));

    m_format = new QComboBox(this);
    for (const editor::ListNumberFormat format : captionFormats()) {
        m_format->addItem(formatSample(format), static_cast<int>(format));
    }
    m_format->setCurrentIndex(std::max(0, m_format->findData(static_cast<int>(current.format))));
    auto* formatRow = new QHBoxLayout();
    //: In Polish: &Format
    formatRow->addWidget(fieldLabel(tr("&Format"), m_format, this));
    formatRow->addWidget(m_format);
    formatRow->addStretch(1);
    contentLayout()->addLayout(formatRow);

    //: In Polish: &Ciągła w całej książce
    m_continuous = new QRadioButton(tr("&Continuous through the whole book"), this);
    //: In Polish: Od &nowa w każdym rozdziale
    m_perChapter = new QRadioButton(tr("Anew in each c&hapter"), this);
    auto* modes = new QButtonGroup(this);
    modes->addButton(m_continuous);
    modes->addButton(m_perChapter);
    (current.perChapter ? m_perChapter : m_continuous)->setChecked(true);
    contentLayout()->addWidget(m_continuous);
    contentLayout()->addWidget(m_perChapter);

    // The chapter's number first, under "anew in each chapter"
    const int indent = style()->pixelMetric(QStyle::PM_IndicatorWidth) +
                       style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);
    //: In Polish: Z numerem &rozdziału
    m_withChapter = new QCheckBox(tr("With the chapter's &number"), this);
    m_withChapter->setChecked(current.withChapter);
    auto* chapterRow = new QHBoxLayout();
    chapterRow->addSpacing(indent);
    chapterRow->addWidget(m_withChapter);
    chapterRow->addStretch(1);
    contentLayout()->addLayout(chapterRow);

    m_separator = new QComboBox(this);
    //: The separators between the chapter's number and the caption's. In Polish: . (kropka)
    m_separator->addItem(tr(". (period)"), QStringLiteral("."));
    //: In Polish: – (półpauza)
    m_separator->addItem(tr("– (en dash)"), QStringLiteral("–"));
    //: In Polish: : (dwukropek)
    m_separator->addItem(tr(": (colon)"), QStringLiteral(":"));
    m_separator->setCurrentIndex(std::max(0, m_separator->findData(current.separator)));
    //: In Polish: &Separator
    m_separatorLabel = fieldLabel(tr("&Separator"), m_separator, this);
    auto* separatorRow = new QHBoxLayout();
    separatorRow->addSpacing(indent);
    separatorRow->addWidget(m_separatorLabel);
    separatorRow->addWidget(m_separator);
    separatorRow->addStretch(1);
    contentLayout()->addLayout(separatorRow);

    m_example = hintLabel(QString(), this);
    contentLayout()->addWidget(m_example);
    contentLayout()->addStretch(1);

    connect(m_format, &QComboBox::currentIndexChanged, this, &CaptionNumberingDialog::updateFields);
    connect(m_perChapter, &QRadioButton::toggled, this, &CaptionNumberingDialog::updateFields);
    connect(m_withChapter, &QCheckBox::toggled, this, &CaptionNumberingDialog::updateFields);
    connect(m_separator, &QComboBox::currentIndexChanged, this, &CaptionNumberingDialog::updateFields);
    updateFields();
    m_format->setFocus();
}

editor::CaptionNumbering CaptionNumberingDialog::numbering() const
{
    editor::CaptionNumbering numbering;
    numbering.format = static_cast<editor::ListNumberFormat>(m_format->currentData().toInt());
    numbering.perChapter = m_perChapter->isChecked();
    numbering.withChapter = numbering.perChapter && m_withChapter->isChecked();
    numbering.separator = m_separator->currentData().toString();
    return editor::normalizedNumbering(numbering);
}

QString CaptionNumberingDialog::describe(const editor::CaptionNumbering& numbering)
{
    const editor::CaptionNumbering normal = editor::normalizedNumbering(numbering);
    const QString format = formatSample(normal.format);
    if (!normal.perChapter) {
        //: How Insert Caption says the numbering of a label. In Polish: Format numeru: %1 ·
        //: ciągła w całej książce
        return tr("Number format: %1 · through the whole book").arg(format);
    }
    if (!normal.withChapter) {
        //: In Polish: Format numeru: %1 · od nowa w każdym rozdziale
        return tr("Number format: %1 · anew in each chapter").arg(format);
    }
    //: %2 is an example: "2.3". In Polish: Format numeru: %1 · od nowa w każdym rozdziale, z
    //: numerem rozdziału (%2)
    return tr("Number format: %1 · anew in each chapter, with the chapter's number (%2)")
        .arg(format, editor::captionNumberText(3, normal, 2));
}

void CaptionNumberingDialog::updateFields()
{
    const bool perChapter = m_perChapter->isChecked();
    m_withChapter->setEnabled(perChapter);
    const bool withChapter = perChapter && m_withChapter->isChecked();
    m_separatorLabel->setEnabled(withChapter);
    m_separator->setEnabled(withChapter);

    // The numbers of the third and fourth images of a label, in the second chapter
    const editor::CaptionNumbering shown = numbering();
    //: %1 is the label, %2 and %3 its numbers: "Figure 2.3, Figure 2.4". In Polish:
    //: Przykład: %1 %2, %1 %3
    m_example->setText(tr("Example: %1 %2, %1 %3")
                           .arg(m_label, editor::captionNumberText(3, shown, 2),
                                editor::captionNumberText(4, shown, 2)));
}

// =============================================================================
// Insert Cross-Reference
// =============================================================================

InsertCrossReferenceDialog::InsertCrossReferenceDialog(Data shown, QWidget* parent)
    : KalahariDialog(parent),
      m_data(std::move(shown))
{
    //: In Polish: Wstaw odsyłacz / Numer obrazu, który zmienia się razem z nim.
    setHeading(tr("Insert Cross-Reference"),
               tr("The number of an image, which changes together with it."));
    setHeadingIcon(QStringLiteral("insert.crossReference"));
    //: In Polish: Wstaw
    setAcceptText(tr("Insert"));

    auto* grid = new QGridLayout();
    grid->setColumnStretch(2, 1);
    m_kind = new QComboBox(this);
    m_kind->addItems(m_data.labels);
    m_kind->setFixedWidth(fontMetrics().averageCharWidth() * CHOICE_WIDTH_CHARS);
    //: The label of the captions the list shows. In Polish: &Rodzaj
    grid->addWidget(fieldLabel(tr("&Kind"), m_kind, this), 0, 0);
    grid->addWidget(m_kind, 0, 1);

    m_shows = new QComboBox(this);
    for (const editor::ReferenceShows shows :
         {editor::ReferenceShows::Number, editor::ReferenceShows::LabelAndNumber,
          editor::ReferenceShows::Caption, editor::ReferenceShows::Position}) {
        m_shows->addItem(QString(), static_cast<int>(shows));
    }
    m_shows->setCurrentIndex(std::max(0, m_shows->findData(static_cast<int>(m_data.shows))));
    //: What the field shows. In Polish: &Wstaw
    grid->addWidget(fieldLabel(tr("&Insert"), m_shows, this), 1, 0);
    grid->addWidget(m_shows, 1, 1, 1, 2);
    contentLayout()->addLayout(grid);

    m_targets = new TargetList(this);
    //: In Polish: &Do którego podpisu
    addField(tr("&Which caption"), m_targets);

    //: %1 is the key held for the click: "Ctrl+" (on macOS "⌘"). In Polish: &Jako łącze –
    //: %1kliknięcie przenosi do obrazu
    m_link = new QCheckBox(tr("As a &link – %1click goes to the image").arg(editor::linkClickKeys()),
                           this);
    m_link->setChecked(true);
    contentLayout()->addWidget(m_link);

    // The caption chosen first: the last one before the cursor, else the first after it
    const Target* first = m_data.targets.empty() ? nullptr : &m_data.targets.front();
    for (const Target& target : m_data.targets) {
        if (target.before) {
            first = &target;
        }
    }
    if (first != nullptr) {
        m_kind->setCurrentIndex(std::max(0, static_cast<int>(m_data.labels.indexOf(first->label))));
    }
    showTargets(first != nullptr ? first->id : QString());

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this]() { showTargets(QString()); });
    connect(m_targets, &QListWidget::currentRowChanged, this, &InsertCrossReferenceDialog::updateChoices);
    connect(m_targets, &QListWidget::itemActivated, this, [this]() {
        if (chosenTarget() != nullptr) {
            accept();
        }
    });
    m_targets->setFocus();
}

editor::CrossReference InsertCrossReferenceDialog::reference() const
{
    editor::CrossReference reference;
    if (const Target* target = chosenTarget()) {
        reference.target = target->id;
    }
    reference.shows = static_cast<editor::ReferenceShows>(m_shows->currentData().toInt());
    reference.link = m_link->isChecked();
    return reference;
}

void InsertCrossReferenceDialog::showTargets(const QString& chosen)
{
    const QString kind = m_kind->currentText();
    const QSignalBlocker blocker(m_targets);
    m_targets->clear();
    int row = -1;
    int lastBefore = -1;
    for (int i = 0; i < static_cast<int>(m_data.targets.size()); ++i) {
        const Target& target = m_data.targets[static_cast<size_t>(i)];
        if (target.label != kind) {
            continue;
        }
        auto* item = new QListWidgetItem(target.text, m_targets);
        item->setData(Qt::UserRole, i);
        if (!target.where.isEmpty()) {
            //: Where a caption of another text is. In Polish: W tekście: %1
            item->setToolTip(tr("In the text: %1").arg(target.where));
        }
        if (target.id == chosen) {
            row = m_targets->count() - 1;
        }
        if (target.before) {
            lastBefore = m_targets->count() - 1;
        }
    }
    if (row < 0) {
        row = lastBefore >= 0 ? lastBefore : 0;
    }
    m_targets->setCurrentRow(m_targets->count() > 0 ? row : -1);
    updateChoices();
}

const InsertCrossReferenceDialog::Target* InsertCrossReferenceDialog::chosenTarget() const
{
    const QListWidgetItem* item = m_targets->currentItem();
    if (item == nullptr) {
        return nullptr;
    }
    const int index = item->data(Qt::UserRole).toInt();
    return index >= 0 && index < static_cast<int>(m_data.targets.size())
               ? &m_data.targets[static_cast<size_t>(index)]
               : nullptr;
}

void InsertCrossReferenceDialog::updateChoices()
{
    // What each choice inserts, with the caption chosen: "3", "Figure 3"
    const Target* target = chosenTarget();
    const QString number = target != nullptr ? target->number : QStringLiteral("2");
    const QString label = target != nullptr ? target->label : m_kind->currentText();
    //: In Polish: Sam numer – „%1”
    m_shows->setItemText(0, tr("The number alone – \"%1\"").arg(number));
    //: In Polish: Etykieta i numer – „%1 %2”
    m_shows->setItemText(1, tr("The label and the number – \"%1 %2\"").arg(label, number));
    //: In Polish: Cały podpis
    m_shows->setItemText(2, tr("The whole caption"));
    //: In Polish: „powyżej” albo „poniżej”
    m_shows->setItemText(3, tr("\"above\" or \"below\""));
    acceptButton()->setEnabled(target != nullptr);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
