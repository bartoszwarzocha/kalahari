/// @file images_page.cpp
/// @brief Settings > Editor > Images: new images, the captions of new books, the copies of the
///        files and the manuscript for a publisher

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/core/settings_manager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

namespace kalahari::gui {

namespace {

using json = nlohmann::json;

/// The characters a choice with long items shows at least (the page fits a narrow window)
constexpr int MIN_CHOICE_CHARACTERS = 14;
constexpr int MIN_LABEL_CHARACTERS = 8;

json labelsValue(const QStringList& labels) {
    json value = json::array();
    for (const QString& label : labels) {
        value.push_back(label.toStdString());
    }
    return value;
}

QStringList labelsOf(const json& value) {
    QStringList labels;
    if (value.is_array()) {
        for (const json& item : value) {
            if (item.is_string()) {
                labels << QString::fromStdString(item.get<std::string>());
            }
        }
    }
    return labels;
}

}  // namespace

EditorImagesPage::EditorImagesPage(QWidget* parent)
    : SettingsPage(parent)
{
    auto& settings = core::SettingsManager::getInstance();

    // How a new image goes into the text
    //: Settings > Editor > Images. In Polish: Nowy obraz
    QFormLayout* image = addGroup(tr("New Image"));
    auto* layout = new QComboBox();
    layout->setObjectName(QStringLiteral("imageLayout"));
    //: In Polish: W tekście, we własnym akapicie
    layout->addItem(tr("In the text, in a paragraph of its own"), QStringLiteral("inline"));
    //: In Polish: Wokół ramki
    layout->addItem(tr("Square"), QStringLiteral("square"));
    //: In Polish: Nad i pod
    layout->addItem(tr("Top and bottom"), QStringLiteral("topBottom"));
    layout->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    layout->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    //: In Polish: Ukła&d:
    QLabel* layoutLabel = addField(image, tr("&Layout:"), layout, IMAGE_LAYOUT_KEY);
    markNotUsedYet(layout, layoutLabel);

    auto* alignment = new QComboBox();
    alignment->setObjectName(QStringLiteral("imageAlignment"));
    //: In Polish: Do lewej
    alignment->addItem(tr("Left"), QStringLiteral("left"));
    //: In Polish: Do środka
    alignment->addItem(tr("Center"), QStringLiteral("center"));
    //: In Polish: Do prawej
    alignment->addItem(tr("Right"), QStringLiteral("right"));
    //: In Polish: &Wyrównanie:
    addField(image, tr("&Alignment:"), alignment, IMAGE_ALIGNMENT_KEY);

    auto* width = new QComboBox();
    width->setObjectName(QStringLiteral("imageWidth"));
    //: In Polish: Jak oryginał, najwyżej szerokość tekstu
    width->addItem(tr("As the original, at most the width of the text"), 0);
    for (const int percent : {100, 75, 50}) {
        //: %1: a share of the width of the text in percent. In Polish: %1% szerokości tekstu
        width->addItem(tr("%1% of the width of the text").arg(percent), percent);
    }
    width->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    width->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    //: In Polish: &Szerokość:
    addField(image, tr("&Width:"), width, IMAGE_WIDTH_KEY);

    //: In Polish: Dodawaj &podpis z etykietą
    auto* caption = new QCheckBox(tr("Add a &caption with the label"));
    caption->setObjectName(QStringLiteral("imageCaption"));
    m_captionLabel = new QComboBox();
    m_captionLabel->setObjectName(QStringLiteral("imageCaptionLabel"));
    //: The label of the caption of a new image (for screen readers). In Polish: Etykieta
    //: podpisu nowego obrazu
    m_captionLabel->setAccessibleName(tr("Label of the caption of a new image"));
    m_captionLabel->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_captionLabel->setMinimumContentsLength(MIN_LABEL_CHARACTERS);
    // The label under the check box's text: the row is no wider than either (a narrow window)
    image->addRow(caption);
    auto* labelRow = new QWidget();
    auto* labelLayout = new QHBoxLayout(labelRow);
    labelLayout->setContentsMargins(0, 0, 0, 0);
    labelLayout->addWidget(m_captionLabel);
    labelLayout->addStretch();
    image->addRow(labelRow);
    alignWithText(labelRow, caption);
    bind(caption, IMAGE_CAPTION_KEY);
    const auto syncCaption = [this, caption]() { m_captionLabel->setEnabled(caption->isChecked()); };
    connect(caption, &QCheckBox::toggled, this, syncCaption);
    whenLoaded(syncCaption);

    // The captions of new books: their labels, how they are numbered, what a cross-reference
    // shows
    //: In Polish: Podpisy w nowych książkach
    QFormLayout* captions = addGroup(tr("Captions in New Books"));
    m_labels = new QListWidget();
    m_labels->setObjectName(QStringLiteral("captionLabels"));
    m_labels->setMaximumHeight(m_labels->fontMetrics().height() * 6);
    //: In Polish: Dod&aj...
    m_addLabel = new QPushButton(tr("A&dd..."));
    //: In Polish: Zmień &nazwę...
    m_renameLabel = new QPushButton(tr("Re&name..."));
    //: In Polish: &Usuń
    m_removeLabel = new QPushButton(tr("Remo&ve"));
    auto* buttons = new QVBoxLayout();
    buttons->addWidget(m_addLabel);
    buttons->addWidget(m_renameLabel);
    buttons->addWidget(m_removeLabel);
    buttons->addStretch();
    auto* labelsRow = new QHBoxLayout();
    labelsRow->addWidget(m_labels, 1);
    labelsRow->addLayout(buttons);
    //: In Polish: &Etykiety:
    auto* labelsLabel = new QLabel(tr("La&bels:"));
    labelsLabel->setBuddy(m_labels);
    captions->addRow(labelsLabel, labelsRow);
    connect(m_addLabel, &QPushButton::clicked, this, &EditorImagesPage::addLabel);
    connect(m_renameLabel, &QPushButton::clicked, this, &EditorImagesPage::renameLabel);
    connect(m_removeLabel, &QPushButton::clicked, this, &EditorImagesPage::removeLabel);
    connect(m_labels, &QListWidget::currentRowChanged, this, &EditorImagesPage::updateLabelButtons);
    connect(m_labels, &QListWidget::itemDoubleClicked, this, &EditorImagesPage::renameLabel);

    // The list: the labels in use (the default ones in the user's language are stored as none,
    // so that they follow the language)
    Binding labels;
    labels.key = []() { return std::string(CAPTION_LABELS_KEY); };
    labels.stored = []() { return labelsValue(settingsCaptionLabels()); };
    labels.shown = [this]() { return labelsValue(listedLabels()); };
    labels.show = [this](const json& value) { showLabels(labelsOf(value)); };
    labels.store = [&settings](const json& value) {
        const QStringList list = labelsOf(value);
        settings.set<json>(CAPTION_LABELS_KEY,
                           list == defaultCaptionLabels() ? json::array() : labelsValue(list));
    };
    bindCustom(std::move(labels));

    // The label of new images' captions: one of the list (none stored: the first)
    Binding captionLabel;
    captionLabel.key = []() { return std::string(IMAGE_CAPTION_LABEL_KEY); };
    captionLabel.stored = [&settings]() {
        const QString label = editor::normalizedLabel(
            QString::fromStdString(settings.get<std::string>(IMAGE_CAPTION_LABEL_KEY, "")));
        return json((label.isEmpty() ? settingsCaptionLabels().constFirst() : label).toStdString());
    };
    captionLabel.shown = [this]() { return json(m_captionLabel->currentText().toStdString()); };
    captionLabel.show = [this](const json& value) {
        const QString label = value.is_string() ? QString::fromStdString(value.get<std::string>())
                                                : QString();
        fillCaptionLabels();
        if (m_captionLabel->findText(label) < 0 && !label.isEmpty()) {
            m_captionLabel->addItem(label);  // a label no longer in the list stays shown
        }
        m_captionLabel->setCurrentIndex(std::max(0, m_captionLabel->findText(label)));
    };
    captionLabel.store = [&settings](const json& value) {
        settings.set<std::string>(IMAGE_CAPTION_LABEL_KEY,
                                  value.is_string() ? value.get<std::string>() : std::string());
    };
    bindCustom(std::move(captionLabel));

    auto* format = new QComboBox();
    format->setObjectName(QStringLiteral("captionFormat"));
    format->addItem(QStringLiteral("1, 2, 3"), QStringLiteral("1"));
    format->addItem(QStringLiteral("a, b, c"), QStringLiteral("a"));
    format->addItem(QStringLiteral("A, B, C"), QStringLiteral("A"));
    format->addItem(QStringLiteral("i, ii, iii"), QStringLiteral("i"));
    format->addItem(QStringLiteral("I, II, III"), QStringLiteral("I"));
    //: Numbered anew in each chapter, after the chapter's number ("2.3"). In Polish: Z numerem
    //: &rozdziału
    auto* withChapter = new QCheckBox(tr("With the c&hapter number"));
    withChapter->setObjectName(QStringLiteral("captionWithChapter"));
    auto* formatRow = new QHBoxLayout();
    formatRow->addWidget(format);
    formatRow->addWidget(withChapter);
    formatRow->addStretch();
    //: In Polish: &Format numeru:
    auto* formatLabel = new QLabel(tr("Number &format:"));
    formatLabel->setBuddy(format);
    captions->addRow(formatLabel, formatRow);
    bind(format, CAPTION_FORMAT_KEY);
    bind(withChapter, CAPTION_WITH_CHAPTER_KEY);

    auto* reference = new QComboBox();
    reference->setObjectName(QStringLiteral("captionReference"));
    const QString example = defaultCaptionLabels().constFirst();
    //: What a cross-reference shows: the number alone. In Polish: Sam numer – „2”
    reference->addItem(tr("The number alone – \"2\""), QStringLiteral("number"));
    //: %1: a label ("Figure"). In Polish: Etykietę i numer – „%1 2”
    reference->addItem(tr("The label and number – \"%1 2\"").arg(example), QStringLiteral("label"));
    //: In Polish: Cały podpis
    reference->addItem(tr("The whole caption"), QStringLiteral("caption"));
    //: In Polish: „powyżej” albo „poniżej”
    reference->addItem(tr("\"above\" or \"below\""), QStringLiteral("position"));
    reference->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    reference->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    //: In Polish: &Odsyłacz wstawia:
    addField(captions, tr("A cross-reference &inserts:"), reference, CAPTION_REFERENCE_KEY);
    addNote(captions, tr("A book that already exists has its own numbering: the window \"Caption "
                         "Numbering\" (the button in the window \"Insert Caption\")."));

    // The copies of the files
    //: In Polish: Pliki
    QFormLayout* files = addGroup(tr("Files"));
    addNote(files, tr("An image inserted into the book goes as a copy to its folder images/; the "
                      "original stays as it was."));
    //: In Polish: &Zmniejszaj kopie dużych obrazów
    auto* downscale = new QCheckBox(tr("Make copies of large images &smaller"));
    downscale->setObjectName(QStringLiteral("imageDownscale"));
    files->addRow(downscale);
    //: Before the resolution. In Polish: powyżej
    auto* above = new QLabel(tr("above"));
    auto* dpi = new QSpinBox();
    dpi->setObjectName(QStringLiteral("imageMaxDpi"));
    dpi->setRange(IMAGE_MIN_MAX_DPI, IMAGE_MAX_MAX_DPI);
    dpi->setSingleStep(50);
    //: Dots per inch, for screen readers. In Polish: Rozdzielczość w punktach na cal
    dpi->setAccessibleName(tr("Resolution in dots per inch"));
    //: In Polish: dpi w rozmiarze z tekstu
    auto* dpiText = new QLabel(tr("dpi at their size in the text"));
    dpiText->setWordWrap(true);  // in a narrow window in two lines
    auto* dpiRow = new QWidget();
    auto* downscaleRow = new QHBoxLayout(dpiRow);
    downscaleRow->setContentsMargins(0, 0, 0, 0);
    downscaleRow->addWidget(above);
    downscaleRow->addWidget(dpi);
    downscaleRow->addWidget(dpiText, 1);
    files->addRow(dpiRow);
    alignWithText(dpiRow, downscale);
    bind(downscale, IMAGE_DOWNSCALE_KEY);
    bind(dpi, IMAGE_MAX_DPI_KEY);
    const auto syncDpi = [downscale, dpiRow]() { dpiRow->setEnabled(downscale->isChecked()); };
    connect(downscale, &QCheckBox::toggled, this, syncDpi);
    whenLoaded(syncDpi);

    // The manuscript for a publisher
    //: In Polish: Maszynopis dla wydawnictwa
    QFormLayout* manuscript = addGroup(tr("Manuscript for a Publisher"));
    auto* choices = new QWidget();
    auto* choicesLayout = new QVBoxLayout(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Obrazy w tekście, jak w edytorze
    auto* inText = new QRadioButton(tr("Images in the text, as in the editor"));
    //: In Polish: Znaczniki w miejscach obrazów
    auto* markers = new QRadioButton(tr("Markers in the places of images"));
    choicesLayout->addWidget(inText);
    choicesLayout->addWidget(markers);
    //: In Polish: W tekście „[Tu rysunek 3]”, obrazy w osobnym folderze.
    alignWithText(addNote(choicesLayout, tr("\"[Figure 3 here]\" in the text, the images in a "
                                            "folder of their own.")),
                  markers);
    manuscript->addRow(choices);
    Binding inManuscript;
    inManuscript.key = []() { return std::string(IMAGE_MANUSCRIPT_KEY); };
    inManuscript.stored = [&settings]() {
        return json(settings.get<std::string>(IMAGE_MANUSCRIPT_KEY, "inline"));
    };
    inManuscript.shown = [markers]() { return json(markers->isChecked() ? "markers" : "inline"); };
    inManuscript.show = [inText, markers](const json& value) {
        const bool marked = value.is_string() && value.get<std::string>() == "markers";
        markers->setChecked(marked);
        inText->setChecked(!marked);
    };
    inManuscript.store = [&settings](const json& value) {
        settings.set<std::string>(IMAGE_MANUSCRIPT_KEY,
                                  value.is_string() ? value.get<std::string>() : "inline");
    };
    bindCustom(std::move(inManuscript));
    markNotUsedYet(choices);
    addNote(manuscript, tr("Many publishers want the illustrations apart, in full resolution, and "
                           "in the manuscript only the places for them."));

    // The colors of the fields and of the frames are on another page
    auto* colors = new QHBoxLayout();
    auto* colorsText = new QLabel(tr("The colors of the selection, the fields and the frames of "
                                     "images are on the page Editor > Colors."));
    colorsText->setWordWrap(true);
    colors->addWidget(colorsText, 1);
    //: Shows the page Editor > Colors. In Polish: Po&każ
    auto* showColors = new QPushButton(tr("Sh&ow"));
    showColors->setObjectName(QStringLiteral("showEditorColors"));
    colors->addWidget(showColors);
    pageLayout()->addLayout(colors);
    connect(showColors, &QPushButton::clicked, this, &EditorImagesPage::colorsPageAsked);
    pageLayout()->addStretch();

    whenLoaded([this]() { updateLabelButtons(); });
}

QStringList EditorImagesPage::listedLabels() const {
    QStringList labels;
    for (int row = 0; row < m_labels->count(); ++row) {
        labels << m_labels->item(row)->text();
    }
    return labels;
}

void EditorImagesPage::showLabels(const QStringList& labels) {
    m_labels->clear();
    m_labels->addItems(labels);
    m_labels->setCurrentRow(labels.isEmpty() ? -1 : 0);
    fillCaptionLabels();
    updateLabelButtons();
}

void EditorImagesPage::fillCaptionLabels() {
    const QString current = m_captionLabel->currentText();
    const QSignalBlocker blocker(m_captionLabel);
    m_captionLabel->clear();
    m_captionLabel->addItems(listedLabels());
    if (const int index = m_captionLabel->findText(current); index >= 0) {
        m_captionLabel->setCurrentIndex(index);
    } else if (!current.isEmpty() && m_captionLabel->count() > 0) {
        m_captionLabel->setCurrentIndex(0);  // its label was renamed or removed
    }
}

void EditorImagesPage::addLabel() {
    //: The window of a new label of captions. In Polish: Nowa etykieta / Etykieta
    const std::optional<QString> typed =
        dialogs::TextInputDialog::getText(this, tr("New Label"), tr("Label"));
    if (!typed) {
        return;
    }
    const QString label = editor::normalizedLabel(*typed);
    if (label.isEmpty()) {
        return;
    }
    const QList<QListWidgetItem*> found = m_labels->findItems(label, Qt::MatchFixedString);
    if (!found.isEmpty()) {
        m_labels->setCurrentItem(found.constFirst());  // it is there already
        return;
    }
    m_labels->addItem(label);
    m_labels->setCurrentRow(m_labels->count() - 1);
    fillCaptionLabels();
    updateLabelButtons();
}

void EditorImagesPage::renameLabel() {
    QListWidgetItem* item = m_labels->currentItem();
    if (item == nullptr) {
        return;
    }
    //: In Polish: Zmiana nazwy etykiety / Etykieta
    const std::optional<QString> typed = dialogs::TextInputDialog::getText(
        this, tr("Rename Label"), tr("Label"), item->text());
    if (!typed) {
        return;
    }
    const QString label = editor::normalizedLabel(*typed);
    const QString old = item->text();
    if (label.isEmpty() || label == old) {
        return;
    }
    const QList<QListWidgetItem*> found = m_labels->findItems(label, Qt::MatchFixedString);
    if (!found.isEmpty() && found.constFirst() != item) {
        m_labels->setCurrentItem(found.constFirst());  // the name of another label
        return;
    }
    const bool captionLabel = m_captionLabel->currentText() == old;
    item->setText(label);
    fillCaptionLabels();
    if (captionLabel) {
        m_captionLabel->setCurrentIndex(std::max(0, m_captionLabel->findText(label)));
    }
}

void EditorImagesPage::removeLabel() {
    if (m_labels->count() <= 1) {
        return;  // one label always stays
    }
    delete m_labels->takeItem(m_labels->currentRow());
    fillCaptionLabels();
    updateLabelButtons();
}

void EditorImagesPage::updateLabelButtons() {
    const bool selected = m_labels->currentItem() != nullptr;
    m_renameLabel->setEnabled(selected);
    m_removeLabel->setEnabled(selected && m_labels->count() > 1);
}

}  // namespace kalahari::gui
