// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file settings_search.cpp
/// @brief The search of the Settings dialog

#include "kalahari/gui/settings/settings_search.h"

#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QTextDocumentFragment>

#include <algorithm>

namespace kalahari::gui {

namespace {

constexpr QChar SMALL_L_STROKE(0x0142);    ///< "ł", which has no accent to drop
constexpr QChar CAPITAL_L_STROKE(0x0141);  ///< "Ł"
constexpr int MARK_MARGIN_X = 4;           ///< Pixels of a mark beside its option
constexpr int MARK_MARGIN_Y = 2;           ///< Pixels of a mark above and below its option
constexpr qreal MARK_RADIUS = 4.0;
constexpr int MARK_FILL_ALPHA = 70;  ///< The fill: light, so the option's text stays readable
constexpr qreal MARK_BORDER = 1.5;

/// @brief Whether a widget is a part of another control of the page (the field of a list or a
///        number, the rows of a list of words, a scroll bar), not an option of its own
bool partOfControl(const QWidget* widget, const QWidget* page) {
    for (const QWidget* parent = widget->parentWidget(); parent != nullptr && parent != page;
         parent = parent->parentWidget()) {
        if (qobject_cast<const QAbstractItemView*>(parent) != nullptr ||
            qobject_cast<const QComboBox*>(parent) != nullptr ||
            qobject_cast<const QAbstractSpinBox*>(parent) != nullptr ||
            qobject_cast<const QScrollBar*>(parent) != nullptr) {
            return true;
        }
    }
    return false;
}

/// @brief Whether a page shows a widget, or does once its advanced settings are open
bool shownOnPage(const QWidget* widget, const QWidget* page) {
    const auto* settingsPage = qobject_cast<const SettingsPage*>(page);
    const QWidget* advanced = settingsPage != nullptr ? settingsPage->advancedSettings() : nullptr;
    for (const QWidget* each = widget; each != nullptr && each != page;
         each = each->parentWidget()) {
        if (each->isHidden() && each != advanced) {
            return false;
        }
    }
    return true;
}

}  // namespace

// =============================================================================
// SettingsSearch
// =============================================================================

SettingsSearch::SettingsSearch(const QString& text)
    : m_words(folded(text).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {}

bool SettingsSearch::matches(const QString& text) const {
    if (m_words.isEmpty()) {
        return false;
    }
    const QString searched = folded(text);
    return std::all_of(m_words.cbegin(), m_words.cend(),
                       [&searched](const QString& word) { return searched.contains(word); });
}

QString SettingsSearch::folded(const QString& text) {
    const QString plain =
        Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText() : text;
    // The marks of the access keys: "&&" is an ampersand, a single "&" marks the next letter
    QString withoutKeys;
    withoutKeys.reserve(plain.size());
    for (qsizetype i = 0; i < plain.size(); ++i) {
        if (plain.at(i) != QLatin1Char('&')) {
            withoutKeys += plain.at(i);
        } else if (i + 1 < plain.size() && plain.at(i + 1) == QLatin1Char('&')) {
            withoutKeys += QLatin1Char('&');
            ++i;
        }
    }
    // The letters without their accents ("ż" as "z"; the compatibility form makes "…" "...")
    QString result;
    const QString decomposed = withoutKeys.normalized(QString::NormalizationForm_KD);
    result.reserve(decomposed.size());
    for (const QChar letter : decomposed) {
        if (letter.category() == QChar::Mark_NonSpacing) {
            continue;
        }
        result += letter == SMALL_L_STROKE || letter == CAPITAL_L_STROKE ? QLatin1Char('l') : letter;
    }
    return result.toCaseFolded().simplified();
}

// =============================================================================
// The options of a page
// =============================================================================

QStringList optionTexts(const QWidget* widget) {
    QStringList texts;
    if (const auto* label = qobject_cast<const QLabel*>(widget)) {
        texts << label->text();
    } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
        texts << button->text();
    } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
        texts << group->title();
    } else if (const auto* list = qobject_cast<const QComboBox*>(widget)) {
        for (int index = 0; index < list->count(); ++index) {
            texts << list->itemText(index);
        }
    } else if (const auto* field = qobject_cast<const QLineEdit*>(widget)) {
        texts << field->placeholderText();
    }
    texts.removeAll(QString());
    return texts;
}

std::vector<QWidget*> optionsFound(QWidget* page, const SettingsSearch& search) {
    std::vector<QWidget*> found;
    if (page == nullptr || search.isEmpty()) {
        return found;
    }
    for (QWidget* widget : page->findChildren<QWidget*>()) {
        if (qobject_cast<SearchMarks*>(widget) != nullptr || !shownOnPage(widget, page) ||
            partOfControl(widget, page)) {
            continue;
        }
        const QStringList texts = optionTexts(widget);
        if (std::any_of(texts.cbegin(), texts.cend(),
                        [&search](const QString& text) { return search.matches(text); })) {
            found.push_back(widget);
        }
    }
    // In the order the page shows them: from the top, then from the left
    std::stable_sort(found.begin(), found.end(), [page](const QWidget* first, const QWidget* second) {
        const QPoint a = first->mapTo(page, QPoint(0, 0));
        const QPoint b = second->mapTo(page, QPoint(0, 0));
        return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });
    return found;
}

// =============================================================================
// SearchMarks
// =============================================================================

SearchMarks::SearchMarks(QWidget* page)
    : QWidget(page) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(page->rect());
    page->installEventFilter(this);
    // The marks follow the color of the search's hits as Settings > Editor > Colors changes it
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            qOverload<>(&QWidget::update));
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            qOverload<>(&QWidget::update));
    hide();
}

void SearchMarks::setOptions(const std::vector<QWidget*>& options) {
    for (const QPointer<QWidget>& option : m_options) {
        if (option != nullptr) {
            option->removeEventFilter(this);
        }
    }
    m_options.clear();
    for (QWidget* option : options) {
        m_options.emplace_back(option);
        // An option that moves or changes its size takes its mark with it
        option->installEventFilter(this);
    }
    setVisible(!m_options.empty());
    // Over the widgets of the page, also those it made since
    raise();
    update();
}

std::vector<QWidget*> SearchMarks::options() const {
    std::vector<QWidget*> options;
    for (const QPointer<QWidget>& option : m_options) {
        if (option != nullptr) {
            options.push_back(option.data());
        }
    }
    return options;
}

QColor SearchMarks::markColor() {
    return core::ThemeManager::getInstance().editorColor(editor::SEARCH_MATCH_COLOR_KEY,
                                                         editor::EditorColors().searchMatch);
}

QRect SearchMarks::markOf(const QWidget* option) const {
    const QWidget* page = parentWidget();
    QRect area = option->rect();
    // A group: its title, not the whole group with its options
    if (const auto* group = qobject_cast<const QGroupBox*>(option)) {
        QStyleOptionGroupBox title;
        title.initFrom(group);
        title.text = group->title();
        title.lineWidth = 1;
        title.midLineWidth = 0;
        title.textAlignment = group->alignment();
        title.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxLabel;
        if (group->isFlat()) {
            title.features |= QStyleOptionFrame::Flat;
        }
        if (group->isCheckable()) {
            title.subControls |= QStyle::SC_GroupBoxCheckBox;
        }
        area = group->style()->subControlRect(QStyle::CC_GroupBox, &title,
                                              QStyle::SC_GroupBoxLabel, group);
    }
    return QRect(option->mapTo(page, area.topLeft()), area.size())
        .adjusted(-MARK_MARGIN_X, -MARK_MARGIN_Y, MARK_MARGIN_X, MARK_MARGIN_Y);
}

void SearchMarks::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = markColor();
    border.setAlpha(255);
    QColor fill = border;
    fill.setAlpha(MARK_FILL_ALPHA);
    painter.setPen(QPen(border, MARK_BORDER));
    painter.setBrush(fill);
    const QWidget* page = parentWidget();
    for (const QPointer<QWidget>& option : m_options) {
        if (option != nullptr && option->isVisibleTo(page)) {
            painter.drawRoundedRect(QRectF(markOf(option)).adjusted(0.5, 0.5, -0.5, -0.5),
                                    MARK_RADIUS, MARK_RADIUS);
        }
    }
}

bool SearchMarks::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::Resize:
        if (watched == parentWidget()) {
            setGeometry(parentWidget()->rect());
        }
        update();
        break;
    case QEvent::Move:
    case QEvent::Show:
    case QEvent::Hide:
        update();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
