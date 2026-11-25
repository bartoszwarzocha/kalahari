// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file images_page.cpp
/// @brief Settings > Editor > Images: new images (with the room they keep from the text), the
///        captions of new books, the copies of the files and the manuscript for a publisher

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/panels/figures_panel.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/spread.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
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
    // How the text flows around it (as Image > Wrap Text)
    auto* layout = new QComboBox();
    layout->setObjectName(QStringLiteral("imageLayout"));
    for (const auto& [wrap, text] : imageWrapChoices()) {
        layout->addItem(text, QString::fromStdString(imageWrapSetting(wrap)));
    }
    layout->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    layout->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    //: In Polish: Ukła&d:
    addField(image, tr("&Layout:"), layout, IMAGE_LAYOUT_KEY);

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

    // The room it keeps from the text around it (Image Properties, Space Around)
    const auto space = [](const char* name) {
        auto* field = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_IMAGE_SPACE);
        field->setObjectName(QLatin1String(name));
        return field;
    };
    //: The room between a new image (or its caption above it) and the text above it. In
    //: Polish: Odstęp &górny:
    addField(image, tr("Space abov&e:"), space("imageSpaceAbove"), IMAGE_SPACE_ABOVE_KEY);
    //: In Polish: Odstęp do&lny:
    addField(image, tr("S&pace below:"), space("imageSpaceBelow"), IMAGE_SPACE_BELOW_KEY);
    //: The room between an image the text runs beside and that text. In Polish: Odstęp
    //: &boczny:
    addField(image, tr("Space beside the &text:"), space("imageSpaceBeside"),
             IMAGE_SPACE_BESIDE_KEY);

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

    // The caption chosen as the image goes in: the label above is the one shown first
    //: In Polish: Py&taj o podpis wstawianego obrazu
    auto* askCaption = new QCheckBox(tr("As&k for the caption of an inserted image"));
    askCaption->setObjectName(QStringLiteral("imageAskCaption"));
    //: In Polish: Wstaw > Obraz... pyta o podpis nowego obrazu (albo o jego brak), zanim obraz
    //: wejdzie do tekstu
    askCaption->setToolTip(tr("Insert > Image... asks for the caption of the new image (or none) "
                              "before it goes into the text"));
    image->addRow(askCaption);
    bind(askCaption, IMAGE_ASK_CAPTION_KEY);

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

    // How a new spread lies on its pages (Book > New Spread...); each keeps its own then
    using core::SpreadData;
    //: The group of Settings > Editor > Images with the settings of new spreads (a picture over
    //: two pages of the book). In Polish: Rozkładówki
    QFormLayout* spreads = addGroup(tr("Spreads"));
    //: In Polish: Jak nowa rozkładówka układa się na dwóch stronach. Każda rozkładówka ma
    //: potem własne ustawienia, na swojej karcie.
    addNote(spreads, tr("How a new spread lies on its two pages. Each spread keeps its own "
                        "settings, on its card."));
    QComboBox* area = addChoice(
        spreads,
        //: Where the picture of a new spread lies. In Polish: Obraz zajmuje:
        tr("The picture takes:"),
        //: In Polish: W marginesach
        {{tr("Within the margins"), QString::fromLatin1(SpreadData::MARGINS),
          //: In Polish: Górny, dolny i zewnętrzne marginesy zostają; przez grzbiet obraz biegnie
          //: bez przerwy.
          tr("The top, bottom and outer margins stay; across the spine the picture goes on "
             "without a break.")},
         //: In Polish: Do krawędzi papieru
         {tr("To the edges of the paper"), QString::fromLatin1(SpreadData::EDGE),
          //: In Polish: Całe obie strony.
          tr("Both pages, whole.")}},
        std::string());
    area->setObjectName(QStringLiteral("spreadArea"));
    bindChoice(area, SpreadData::AREA_KEY, SpreadData::MARGINS);
    QComboBox* fit = addChoice(
        spreads,
        //: How the picture of a new spread fits its area. In Polish: Dopasowanie:
        tr("Fit:"),
        //: In Polish: Cały obraz
        {{tr("The whole picture"), QString::fromLatin1(SpreadData::WHOLE),
          //: In Polish: Nic nie ginie, wolne miejsce zostaje białe.
          tr("Nothing is lost; the room left stays white.")},
         //: In Polish: Wypełnij
         {tr("Fill"), QString::fromLatin1(SpreadData::FILL),
          //: In Polish: Obraz wypełnia pole, a brzegi poza nim są przycięte.
          tr("The picture fills the area, and its edges beyond it are cut off.")}},
        std::string());
    fit->setObjectName(QStringLiteral("spreadFit"));
    bindChoice(fit, SpreadData::FIT_KEY, SpreadData::WHOLE);
    auto* gutter = new LengthSpinBox(LengthUnit::Millimeters, 0.0, SpreadData::MAX_GUTTER);
    gutter->setObjectName(QStringLiteral("spreadGutter"));
    //: The room at the spine of a new spread. In Polish: Zapas na grzbiet:
    addField(spreads, tr("Room at the spine:"), gutter, SpreadData::GUTTER_KEY);
    //: In Polish: Połówki obrazu odsuwają się od zgięcia, a pasek przy grzbiecie jest na obu
    //: stronach.
    addFieldNote(spreads, tr("The halves of the picture move away from the fold, and the strip "
                             "at the spine shows on both pages."));
    //: In Polish: Podpis pod obrazem, na prawej stronie
    QCheckBox* spreadCaption = addCheckBox(spreads, tr("A caption under the picture, on the "
                                                       "right page"),
                                           SpreadData::CAPTION_KEY);
    spreadCaption->setObjectName(QStringLiteral("spreadCaption"));
    m_spreadLabel = new QComboBox();
    m_spreadLabel->setObjectName(QStringLiteral("spreadCaptionLabel"));
    //: The label of the caption of a new spread (for screen readers). In Polish: Etykieta
    //: podpisu nowej rozkładówki
    m_spreadLabel->setAccessibleName(tr("Label of the caption of a new spread"));
    m_spreadLabel->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_spreadLabel->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    auto* spreadLabelRow = new QWidget();
    auto* spreadLabelLayout = new QHBoxLayout(spreadLabelRow);
    spreadLabelLayout->setContentsMargins(0, 0, 0, 0);
    spreadLabelLayout->addWidget(m_spreadLabel);
    spreadLabelLayout->addStretch();
    spreads->addRow(spreadLabelRow);
    alignWithText(spreadLabelRow, spreadCaption);
    fillSpreadLabels();
    bind(m_spreadLabel, SpreadData::CAPTION_LABEL_KEY);
    const auto syncSpreadLabel = [this, spreadCaption]() {
        m_spreadLabel->setEnabled(spreadCaption->isChecked());
    };
    connect(spreadCaption, &QCheckBox::toggled, this, syncSpreadLabel);
    whenLoaded(syncSpreadLabel);
    //: In Polish: Numery stron i żywa pagina
    addCheckBox(spreads, tr("Page numbers and running head"), SpreadData::PAGE_NUMBERS_KEY)
        ->setObjectName(QStringLiteral("spreadPageNumbers"));
    //: In Polish: Tytuł w spisie treści
    addCheckBox(spreads, tr("Title in the table of contents"), SpreadData::IN_CONTENTS_KEY)
        ->setObjectName(QStringLiteral("spreadInContents"));

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
    QComboBox* inManuscript = addChoice(
        manuscript,
        //: How the images are in the manuscript for a publisher. In Polish: W &maszynopisie:
        tr("In the &manuscript:"),
        //: In Polish: Obrazy w tekście, jak w edytorze
        {{tr("Images in the text, as in the editor"), QStringLiteral("inline"), {}},
         //: In Polish: Znaczniki w miejscach obrazów
         {tr("Markers in the places of images"), QStringLiteral("markers"),
          //: In Polish: W tekście „[Tu rysunek 3]”, obrazy w osobnym folderze.
          tr("\"[Figure 3 here]\" in the text, the images in a folder of their own.")}},
        std::string());
    inManuscript->setObjectName(QStringLiteral("imageManuscript"));
    inManuscript->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    inManuscript->setMinimumContentsLength(MIN_CHOICE_CHARACTERS);
    bindChoice(inManuscript, IMAGE_MANUSCRIPT_KEY, "inline");
    markNotUsedYet(inManuscript,
                   qobject_cast<QLabel*>(manuscript->labelForField(inManuscript)));
    addNote(manuscript, tr("Many publishers want the illustrations apart, in full resolution, and "
                           "in the manuscript only the places for them."));

    // The cards of the panel Images and Tables (View > Panels > Images and Tables)
    //: The group of Settings > Editor > Images with the cards of the panel Images and Tables.
    //: In Polish: Panel „Obrazy i tabele”
    QFormLayout* panel = addGroup(tr("Images and Tables Panel"));
    //: In Polish: Pokazuj obraz&y na kartach
    QCheckBox* pictures = addCheckBox(panel, tr("Show the pict&ures on the cards"),
                                      FiguresPanel::PICTURES_SETTING);
    pictures->setObjectName(QStringLiteral("figuresPanelPictures"));  // SettingsDialog::openPage()
    //: In Polish: Na kartach panelu „Obrazy i tabele” (Widok > Panele > Obrazy i tabele): mały
    //: obraz, a przy tabeli rysunek jej wierszy i kolumn.
    addOptionNote(panel, pictures,
                  tr("On the cards of the panel Images and Tables (View > Panels > Images and "
                     "Tables): a small picture of the image, and a drawing of the rows and "
                     "columns of a table."));
    auto* captionLines = new QSpinBox();
    captionLines->setObjectName(QStringLiteral("figuresPanelLines"));
    captionLines->setRange(FiguresPanel::MIN_LINES, FiguresPanel::MAX_LINES);
    //: In Polish: W&iersze podpisu na karcie:
    addField(panel, tr("Lines of a caption on its ca&rd:"), captionLines,
             FiguresPanel::LINES_SETTING);

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

void EditorImagesPage::fillSpreadLabels() {
    if (m_spreadLabel == nullptr) {
        return;
    }
    // A caption without a label (its text alone), or one of the labels of the list
    const QString current = m_spreadLabel->currentData().toString();
    const QSignalBlocker blocker(m_spreadLabel);
    m_spreadLabel->clear();
    //: The caption of a spread: its text alone. In Polish: bez etykiety i numeru
    m_spreadLabel->addItem(tr("without a label and number"), QString());
    const QStringList labels = m_labels != nullptr && m_labels->count() > 0
                                   ? listedLabels()
                                   : settingsCaptionLabels();
    for (const QString& label : labels) {
        //: The caption of a spread: a label with its number; %1: the label ("Map"). In Polish:
        //: %1 z numerem
        m_spreadLabel->addItem(tr("%1 with a number").arg(label), label);
    }
    if (const int index = m_spreadLabel->findData(current); index >= 0) {
        m_spreadLabel->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        m_spreadLabel->addItem(tr("%1 with a number").arg(current), current);  // still shown
        m_spreadLabel->setCurrentIndex(m_spreadLabel->count() - 1);
    }
}

void EditorImagesPage::fillCaptionLabels() {
    fillSpreadLabels();
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
