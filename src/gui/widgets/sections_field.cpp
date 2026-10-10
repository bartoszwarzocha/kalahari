/// @file sections_field.cpp
/// @brief Fields of the sections of a book

#include "kalahari/gui/widgets/sections_field.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStyleOptionComboBox>
#include <QStylePainter>

#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// SectionsComboBox
// ============================================================================

SectionsComboBox::SectionsComboBox(QWidget* parent)
    : QComboBox(parent)
    , m_language(QStringLiteral("en"))
{
    setToolTip(tr("Names of the three sections the Navigator divides the book into: the front, "
                  "the main and the back one. Without sections the Navigator shows the elements "
                  "of the book in one list."));
    populate();
}

void SectionsComboBox::setLanguage(const QString& language) {
    if (language == m_language) {
        return;
    }
    m_language = language;
    const QSignalBlocker blocker(this);
    const QString chosen = choice();
    populate();
    choose(chosen);
}

QString SectionsComboBox::choice() const {
    return currentData().toString();
}

void SectionsComboBox::choose(const QString& item) {
    setCurrentIndex(std::max(findData(item), 0));
}

QString SectionsComboBox::itemOf(const core::BookSections& sections) {
    if (!sections.shown) {
        return QString::fromLatin1(NO_SECTIONS);
    }
    if (sections.set == QLatin1String(core::ProjectBook::CUSTOM_SECTIONS) ||
        isNameSet(sections.set)) {
        return sections.set;
    }
    return core::ProjectBook::sectionNameSets().first().id;  // a set this version does not know
}

bool SectionsComboBox::isNameSet(const QString& item) {
    const QList<core::SectionNameSet>& sets = core::ProjectBook::sectionNameSets();
    return std::any_of(sets.cbegin(), sets.cend(),
                       [&item](const core::SectionNameSet& set) { return set.id == item; });
}

void SectionsComboBox::paintEvent(QPaintEvent* /*event*/) {
    // As QComboBox paints itself, with the text cut to the width of the field
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));
    QStyleOptionComboBox option;
    initStyleOption(&option);
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    const QRect field =
        style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    option.currentText =
        fontMetrics().elidedText(option.currentText, Qt::ElideRight, field.width() - 2);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

void SectionsComboBox::populate() {
    clear();
    for (const core::SectionNameSet& set : core::ProjectBook::sectionNameSets()) {
        QStringList names;
        for (core::BookPlace place : SECTION_PLACES) {
            names << set.name(place, m_language);
        }
        addItem(names.join(QStringLiteral(" · ")), set.id);
    }
    addItem(tr("Custom Names"), QString::fromLatin1(core::ProjectBook::CUSTOM_SECTIONS));
    addItem(tr("No Sections"), QString::fromLatin1(NO_SECTIONS));

    // The list shows the whole names, also when the field is narrower than them
    view()->setMinimumWidth(view()->sizeHintForColumn(0) + 2 * view()->frameWidth());
}

// ============================================================================
// SectionNamesEdit
// ============================================================================

SectionNamesEdit::SectionNamesEdit(Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QBoxLayout(orientation == Qt::Horizontal ? QBoxLayout::LeftToRight
                                                                : QBoxLayout::TopToBottom,
                                  this);
    layout->setContentsMargins(0, 0, 0, 0);

    const std::array<QString, SECTION_PLACES.size()> tips = {
        tr("Name of the first section"), tr("Name of the second section"),
        tr("Name of the third section")};
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        auto* field = new QLineEdit(this);
        field->setToolTip(tips.at(i));
        field->setAccessibleName(tips.at(i));
        layout->addWidget(field, 1);
        connect(field, &QLineEdit::textEdited, this, [this](const QString&) { emit edited(); });
        connect(field, &QLineEdit::editingFinished, this, &SectionNamesEdit::editingFinished);
        m_fields.at(i) = field;
    }
    setLanguage(QStringLiteral("en"));
}

void SectionNamesEdit::setLanguage(const QString& language) {
    const core::SectionNameSet& first = core::ProjectBook::sectionNameSets().first();
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        m_fields.at(i)->setPlaceholderText(first.name(SECTION_PLACES.at(i), language));
    }
}

QStringList SectionNamesEdit::names() const {
    QStringList names;
    for (const QLineEdit* field : m_fields) {
        names << field->text().trimmed();
    }
    return names;
}

void SectionNamesEdit::setNames(const QStringList& names) {
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        m_fields.at(i)->setText(names.value(static_cast<qsizetype>(i)));
    }
}

QLineEdit* SectionNamesEdit::firstField() const {
    return m_fields.front();
}

}  // namespace gui
}  // namespace kalahari
