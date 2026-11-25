// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file series_page.cpp
/// @brief The series in the Properties panel: its name and its volumes

#include "kalahari/gui/panels/series_page.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace kalahari::gui {

namespace {

/// The margins and the spacing of the page, as those of the other pages of the panel
constexpr int PAGE_MARGIN = 11;
/// The space between the buttons
constexpr int BUTTON_SPACING = 6;

/// The columns of the table of the volumes
enum Column { NUMBER_COLUMN = 0, TITLE_COLUMN = 1, WORDS_COLUMN = 2, TEXTS_COLUMN = 3 };

/// The words of a hint under a field or a table: wrapped, in the color of a placeholder
QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    return hint;
}

/// The number of the main texts of @p kind in @p statistics; every text without a main kind
int mainTextsOf(const core::TextStatistics& statistics, const core::KindRef& kind) {
    if (!kind) {
        return statistics.elements;
    }
    const auto it = statistics.kinds.find(kind.reference());
    return it == statistics.kinds.end() ? 0 : it->second;
}

/// What the keys @p keys do in the table of the volumes
enum class VolumeKey { None, Earlier, Later, Open };

VolumeKey volumeKeyOf(QKeyCombination keys) {
    if (keys == QKeyCombination(Qt::AltModifier, Qt::Key_Up)) {
        return VolumeKey::Earlier;
    }
    if (keys == QKeyCombination(Qt::AltModifier, Qt::Key_Down)) {
        return VolumeKey::Later;
    }
    const bool enter = keys == QKeyCombination(Qt::Key_Return) ||
                       keys == QKeyCombination(Qt::KeypadModifier, Qt::Key_Enter);
    return enter ? VolumeKey::Open : VolumeKey::None;
}

}  // namespace

SeriesPage::SeriesPage(core::ProjectManager& book, QWidget* parent)
    : QWidget(parent), m_book(book) {
    setObjectName(QStringLiteral("seriesPage"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* area = new QScrollArea(this);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(area);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    layout->setSpacing(PAGE_MARGIN);
    layout->addWidget(createSeriesGroup(content));
    layout->addWidget(createVolumesGroup(content));
    layout->addStretch();
    area->setWidget(content);
    outer->addWidget(area);
}

QWidget* SeriesPage::createSeriesGroup(QWidget* parent) {
    //: The group of the name of a series in the Properties panel. In Polish: Seria
    auto* group = new QGroupBox(tr("Series"), parent);
    auto* form = new QFormLayout(group);
    m_nameEdit = new QLineEdit(group);
    m_nameEdit->setObjectName(QStringLiteral("seriesNameEdit"));
    //: In Polish: Nazwa serii:
    form->addRow(tr("Name of the series:"), m_nameEdit);
    //: Under the name of a series. In Polish: Nazwa serii jest nazwą korzenia w Nawigatorze i
    //: podtytułem okna.
    form->addRow(hintLabel(tr("The name of the series is the name of the root of the Navigator "
                              "and the subtitle of the window."),
                           group));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &SeriesPage::saveName);
    return group;
}

QWidget* SeriesPage::createVolumesGroup(QWidget* parent) {
    //: The group of the volumes of a series in the Properties panel. In Polish: Tomy
    auto* group = new QGroupBox(tr("Volumes"), parent);
    auto* layout = new QVBoxLayout(group);
    m_volumes = new QTreeWidget(group);
    m_volumes->setObjectName(QStringLiteral("seriesVolumeList"));
    m_volumes->setRootIsDecorated(false);
    m_volumes->setUniformRowHeights(true);
    m_volumes->setSelectionMode(QAbstractItemView::SingleSelection);
    m_volumes->setColumnCount(TEXTS_COLUMN + 1);
    m_volumes->header()->setStretchLastSection(false);
    m_volumes->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_volumes->header()->setSectionResizeMode(TITLE_COLUMN, QHeaderView::Stretch);
    //: The name of the table of the volumes of a series, for screen readers. In Polish: Tomy
    //: serii
    m_volumes->setAccessibleName(tr("Volumes of the series"));
    m_volumes->installEventFilter(this);
    layout->addWidget(m_volumes);

    auto* buttons = new QWidget(group);
    auto* flow = new FlowLayout(buttons, BUTTON_SPACING, BUTTON_SPACING);
    flow->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Nowy tom...
    m_newButton = new QPushButton(tr("New Volume..."), buttons);
    //: Moves the volume chosen one place up. In Polish: Wcześniej
    m_earlierButton = new QPushButton(tr("Earlier"), buttons);
    //: The tooltip of Earlier under the volumes of a series. In Polish: Tom wcześniej (%1)
    m_earlierButton->setToolTip(
        tr("Volume Earlier (%1)").arg(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Up)));
    //: Moves the volume chosen one place down. In Polish: Później
    m_laterButton = new QPushButton(tr("Later"), buttons);
    //: The tooltip of Later under the volumes of a series. In Polish: Tom później (%1)
    m_laterButton->setToolTip(
        tr("Volume Later (%1)").arg(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Down)));
    //: Opens the first chapter of the volume chosen. In Polish: Otwórz
    m_openButton = new QPushButton(tr("Open"), buttons);
    //: The tooltip of Open under the volumes of a series. In Polish: Otwórz pierwszy rozdział
    //: tomu (%1)
    m_openButton->setToolTip(tr("Open the first chapter of the volume (%1)")
                                 .arg(ShortcutRules::keysText(Qt::Key_Enter)));
    for (QPushButton* button : {m_newButton, m_earlierButton, m_laterButton, m_openButton}) {
        flow->addWidget(button);
    }
    layout->addWidget(buttons);
    m_summary = hintLabel(QString(), group);
    m_summary->setObjectName(QStringLiteral("seriesSummary"));
    layout->addWidget(m_summary);

    connect(m_newButton, &QPushButton::clicked, this, &SeriesPage::requestNewVolume);
    connect(m_earlierButton, &QPushButton::clicked, this, [this]() { moveChosen(-1); });
    connect(m_laterButton, &QPushButton::clicked, this, [this]() { moveChosen(1); });
    connect(m_openButton, &QPushButton::clicked, this, &SeriesPage::openChosen);
    connect(m_volumes, &QTreeWidget::itemDoubleClicked, this, &SeriesPage::openChosen);
    connect(m_volumes, &QTreeWidget::currentItemChanged, this, &SeriesPage::updateButtons);
    return group;
}

void SeriesPage::refresh() {
    const core::BookProject* project = m_book.project();
    if (project == nullptr) {
        return;
    }
    // The name the writer is typing stays as it is
    if (!m_nameEdit->hasFocus()) {
        m_nameEdit->setText(project->series);
    }
    m_nameEdit->setPlaceholderText(project->books.isEmpty() ? QString()
                                                            : project->books.constFirst().title);
    m_nameEdit->setReadOnly(m_book.isReadOnly());

    // The main texts are named after their kind: "Chapters", "Stories"
    const core::KindRef mainKind = m_book.mainTextKind();
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("main"), wordsOf(mainKind)}};
    //: The columns of the table of the volumes of a series. In Polish: Tom, Tytuł, Słowa,
    //: {Main:plural} (Rozdziały), Elementy
    m_volumes->setHeaderLabels({tr("Volume"), tr("Title"), tr("Words"),
                                mainKind ? core::fillWords(tr("{Main:plural}"), nouns)
                                         : tr("Elements")});

    const QString chosen = chosenVolume().isEmpty() && m_book.book() != nullptr
                               ? m_book.book()->id
                               : chosenVolume();
    const QLocale locale;
    qint64 words = 0;
    m_volumes->clear();
    for (qsizetype i = 0; i < project->books.size(); ++i) {
        const core::ProjectBook& volume = project->books.at(i);
        const core::TextStatistics body = m_book.statisticsOf(volume.mainElements);
        auto* row = new QTreeWidgetItem(m_volumes);
        row->setText(NUMBER_COLUMN, locale.toString(i + 1));
        row->setText(TITLE_COLUMN, volume.title);
        row->setText(WORDS_COLUMN, locale.toString(body.words));
        row->setText(TEXTS_COLUMN, locale.toString(mainTextsOf(body, mainKind)));
        for (const int column : {NUMBER_COLUMN, WORDS_COLUMN, TEXTS_COLUMN}) {
            row->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        }
        row->setData(NUMBER_COLUMN, Qt::UserRole, volume.id);
        if (volume.id == chosen) {
            m_volumes->setCurrentItem(row);
        }
        words += body.words;
    }

    const auto count = static_cast<int>(project->books.size());
    //: Under the table of the volumes; %1: the words of the whole series, %n: the volumes. In
    //: Polish: Razem: %1 słów w %n tomach. Warsztat – postacie, miejsca, przedmioty, słownik i
    //: materiały – jest wspólny dla wszystkich tomów.
    m_summary->setText(tr("In all: %1 words in %n volumes. The Workshop – the characters, the "
                          "locations, the items, the dictionary and the materials – is shared "
                          "by all the volumes.",
                          nullptr, count)
                           .arg(locale.toString(words)));
    updateButtons();
}

void SeriesPage::focusName() {
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->selectAll();
}

QString SeriesPage::chosenVolume() const {
    const QTreeWidgetItem* row = m_volumes->currentItem();
    return row != nullptr ? row->data(NUMBER_COLUMN, Qt::UserRole).toString() : QString();
}

void SeriesPage::saveName() {
    const core::BookProject* project = m_book.project();
    const QString name = m_nameEdit->text().trimmed();
    if (project == nullptr || m_book.isReadOnly() || name == project->series) {
        return;
    }
    if (!m_book.setSeriesName(name)) {
        //: In Polish: Nie udało się zapisać nazwy serii: nie można zapisać projektu.
        dialogs::MessageDialog::warning(this, tr("Series"),
                                        tr("Could not save the name of the series: the project "
                                           "could not be saved."));
        return;
    }
    emit seriesChanged();
}

void SeriesPage::moveChosen(int step) {
    const core::BookProject* project = m_book.project();
    if (project == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QString bookId = chosenVolume();
    const qsizetype index = project->bookIndex(bookId);
    const qsizetype to = index + step;
    if (index < 0 || to < 0 || to >= project->books.size()) {
        return;
    }
    if (!m_book.moveBook(bookId, to)) {
        //: In Polish: Nie udało się przenieść tomu: nie można zapisać projektu.
        dialogs::MessageDialog::warning(this, tr("Move Volume"),
                                        tr("Could not move the volume: the project could not "
                                           "be saved."));
        return;
    }
    refresh();
    emit seriesChanged();
}

void SeriesPage::openChosen() {
    if (const QString bookId = chosenVolume(); !bookId.isEmpty()) {
        emit requestOpenVolume(bookId);
    }
}

void SeriesPage::updateButtons() {
    const core::BookProject* project = m_book.project();
    const qsizetype index = project != nullptr ? project->bookIndex(chosenVolume()) : -1;
    const qsizetype count = project != nullptr ? project->books.size() : 0;
    const bool editable = project != nullptr && !m_book.isReadOnly();
    m_newButton->setEnabled(editable);
    m_earlierButton->setEnabled(editable && index > 0);
    m_laterButton->setEnabled(editable && index >= 0 && index < count - 1);
    m_openButton->setEnabled(index >= 0);
}

bool SeriesPage::runVolumeKey(QKeyCombination keys, bool run) {
    const VolumeKey action = volumeKeyOf(keys);
    if (action == VolumeKey::None || !run) {
        return action != VolumeKey::None;
    }
    if (action == VolumeKey::Open) {
        openChosen();
    } else {
        moveChosen(action == VolumeKey::Earlier ? -1 : 1);
    }
    return true;
}

bool SeriesPage::eventFilter(QObject* watched, QEvent* event) {
    const bool keys =
        event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress;
    if (watched == m_volumes && keys) {
        auto* key = static_cast<QKeyEvent*>(event);
        // The keys are the table's, not the window's shortcuts
        if (runVolumeKey(ShortcutRules::keysOf(key), event->type() == QEvent::KeyPress)) {
            key->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
