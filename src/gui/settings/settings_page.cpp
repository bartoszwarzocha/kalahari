/// @file settings_page.cpp
/// @brief Implementation of SettingsPage

#include "kalahari/gui/settings/settings_page.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/widgets/color_config_widget.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/settings_schema.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/text_issue.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace kalahari {
namespace gui {

using json = nlohmann::json;

namespace {

/// The item data of a combo box as a setting value of the same type as the stored one
json comboValue(const QVariant& data, const json& like) {
    if (like.is_number_integer()) {
        return data.toInt();
    }
    if (like.is_number()) {
        return data.toDouble();
    }
    return data.toString().toStdString();
}

/// The combo box item whose data is the setting value
int comboIndexOf(const QComboBox* combo, const json& value) {
    for (int index = 0; index < combo->count(); ++index) {
        if (comboValue(combo->itemData(index), value) == value) {
            return index;
        }
    }
    return -1;
}

/// A spin box value as a setting value: whole numbers stay whole unless stored otherwise
json spinValue(double shown, double scale, const json& like) {
    const double value = shown / scale;
    if (like.is_number_integer() && scale == 1.0) {
        return static_cast<int>(std::lround(value));
    }
    return value;
}

/// The color of a binding's value ("#rrggbb"; invalid for anything else)
QColor colorOf(const json& value) {
    return QColor(QString::fromStdString(value.is_string() ? value.get<std::string>() : std::string()));
}

/// A color as a binding's value
json colorValue(const QColor& color) {
    return json(color.name().toStdString());
}

/// The style of the notes (see SettingsPage::noteColor())
QString noteStyleSheet() {
    return QStringLiteral("color: %1;").arg(SettingsPage::noteColor().name());
}

/// The color inside a group on the window, as the style draws it (Fusion a shade darker)
QColor groupFillOn(const QColor& window) {
    QImage image(QSize(64, 64), QImage::Format_ARGB32_Premultiplied);
    image.fill(window);
    QPainter painter(&image);
    QStyleOptionFrame option;
    option.rect = image.rect();
    option.palette.setColor(QPalette::Window, window);
    QApplication::style()->drawPrimitive(QStyle::PE_FrameGroupBox, &option, &painter);
    painter.end();
    return image.pixelColor(image.width() / 2, image.height() - 16);
}

} // namespace

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    // A theme applied while the dialog is open recolors the notes
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &SettingsPage::updateNoteColors);
}

void SettingsPage::load() {
    for (Binding& binding : m_bindings) {
        reload(binding);
    }
    for (const auto& sync : m_afterLoad) {
        sync();
    }
}

void SettingsPage::whenLoaded(std::function<void()> sync) {
    m_afterLoad.push_back(std::move(sync));
}

void SettingsPage::reload(Binding& binding) {
    binding.show(binding.stored());
    // What the control shows (a spin box may round or clamp the stored value)
    binding.loaded = binding.shown();
}

bool SettingsPage::isChanged() const {
    for (const Binding& binding : m_bindings) {
        if (bindingChanged(binding)) {
            return true;
        }
    }
    return false;
}

bool SettingsPage::bindingChanged(const Binding& binding) {
    return binding.shown() != binding.loaded;
}

SettingsPage::Problem SettingsPage::problem() const {
    return {};
}

std::vector<std::string> SettingsPage::apply() {
    std::vector<std::string> written;
    for (Binding& binding : m_bindings) {
        json value = binding.shown();
        if (value == binding.loaded) {
            continue;  // Unchanged values keep following the defaults
        }
        binding.store(value);
        binding.loaded = std::move(value);
        const std::string key = binding.key();
        if (!key.empty()) {
            written.push_back(key);
        }
    }
    return written;
}

SettingsPage::Binding& SettingsPage::bindCustom(Binding binding) {
    m_bindings.push_back(std::move(binding));
    return m_bindings.back();
}

SettingsPage::Binding& SettingsPage::bind(QWidget* field, const std::string& key, double scale) {
    auto& settings = core::SettingsManager::getInstance();

    Binding binding;
    binding.key = [key]() { return key; };
    binding.stored = [&settings, key]() { return settings.get<json>(key); };
    binding.store = [&settings, key](const json& value) { settings.set<json>(key, value); };

    // The stored value's type decides the type written back (int, double, string)
    const json like = settings.get<json>(key, json());

    if (auto* box = qobject_cast<QCheckBox*>(field)) {
        binding.shown = [box]() { return json(box->isChecked()); };
        binding.show = [box](const json& value) {
            box->setChecked(value.is_boolean() && value.get<bool>());
        };
    } else if (auto* fonts = qobject_cast<QFontComboBox*>(field)) {
        // A font missing on this system is shown as its substitute: keep the stored name
        // until the user picks another font
        auto names = std::make_shared<std::pair<QString, QString>>();  // stored, shown
        binding.shown = [fonts, names]() {
            const QString family = fonts->currentFont().family();
            return json((family == names->second ? names->first : family).toStdString());
        };
        binding.show = [fonts, names](const json& value) {
            names->first = QString::fromStdString(value.is_string() ? value.get<std::string>() : "");
            fonts->setCurrentFont(QFont(names->first));
            names->second = fonts->currentFont().family();
        };
    } else if (auto* combo = qobject_cast<QComboBox*>(field)) {
        binding.shown = [combo, like]() { return comboValue(combo->currentData(), like); };
        binding.show = [combo](const json& value) {
            combo->setCurrentIndex(std::max(0, comboIndexOf(combo, value)));
        };
    } else if (auto* spin = qobject_cast<QSpinBox*>(field)) {
        binding.shown = [spin, scale, like]() { return spinValue(spin->value(), scale, like); };
        binding.show = [spin, scale](const json& value) {
            if (value.is_number()) {
                spin->setValue(static_cast<int>(std::lround(value.get<double>() * scale)));
            }
        };
    } else if (auto* length = qobject_cast<LengthSpinBox*>(field)) {
        // Compared and written in the stored unit, whatever unit it is shown in
        binding.shown = [length, like]() { return spinValue(length->storedValue(), 1.0, like); };
        binding.show = [length](const json& value) {
            if (value.is_number()) {
                length->setStoredValue(value.get<double>());
            }
        };
    } else if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(field)) {
        binding.shown = [doubleSpin, scale]() { return json(doubleSpin->value() / scale); };
        binding.show = [doubleSpin, scale](const json& value) {
            if (value.is_number()) {
                doubleSpin->setValue(value.get<double>() * scale);
            }
        };
    } else if (auto* line = qobject_cast<QLineEdit*>(field)) {
        binding.shown = [line]() { return json(line->text().toStdString()); };
        binding.show = [line](const json& value) {
            line->setText(QString::fromStdString(value.is_string() ? value.get<std::string>() : ""));
        };
    } else if (auto* color = qobject_cast<ColorConfigWidget*>(field)) {
        binding.shown = [color]() { return json(color->color().name().toStdString()); };
        binding.show = [color](const json& value) {
            if (value.is_string()) {
                color->setColor(QColor(QString::fromStdString(value.get<std::string>())));
            }
        };
    } else {
        Q_ASSERT_X(false, "SettingsPage::bind", "unsupported control");
        binding.shown = []() { return json(); };
        binding.show = [](const json&) {};
    }

    if (core::settings_schema::requiresRestart(key)) {
        const QString tip = field->toolTip();
        field->setToolTip(tip.isEmpty() ? restartText() : tip + QStringLiteral("\n") + restartText());
    }
    return bindCustom(std::move(binding));
}

QFormLayout* SettingsPage::addGroup(const QString& title) {
    auto* group = new QGroupBox(title, this);
    auto* form = new QFormLayout(group);
    // Fields fill the width on every platform, as in a grid with a stretching column
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // In a narrow window a field goes under its label, so the page does not scroll sideways
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_layout->addWidget(group);
    return form;
}

QLabel* SettingsPage::addField(QFormLayout* form, const QString& label, QWidget* field,
                               const std::string& key, double scale) {
    auto* labelWidget = new QLabel(label);
    labelWidget->setBuddy(field);
    form->addRow(labelWidget, field);
    bind(field, key, scale);
    if (core::settings_schema::requiresRestart(key)) {
        addNote(form, restartText());
    }
    return labelWidget;
}

QCheckBox* SettingsPage::addCheckBox(QFormLayout* form, const QString& text, const std::string& key) {
    auto* box = new QCheckBox(text);
    form->addRow(box);
    bind(box, key);
    if (core::settings_schema::requiresRestart(key)) {
        addNote(form, restartText());
    }
    return box;
}

QLabel* SettingsPage::addNote(QLayout* layout, const QString& text) {
    auto* note = new QLabel(text);
    note->setWordWrap(true);
    note->setStyleSheet(noteStyleSheet());
    m_notes.push_back(note);
    if (auto* form = qobject_cast<QFormLayout*>(layout)) {
        form->addRow(note);
    } else {
        layout->addWidget(note);
    }
    return note;
}

void SettingsPage::alignWithText(QLabel* note, const QAbstractButton* button) {
    const QStyle* style = button->style();
    const bool radio = qobject_cast<const QRadioButton*>(button) != nullptr;
    const int indicator =
        style->pixelMetric(radio ? QStyle::PM_ExclusiveIndicatorWidth : QStyle::PM_IndicatorWidth,
                           nullptr, button);
    const int spacing = style->pixelMetric(
        radio ? QStyle::PM_RadioButtonLabelSpacing : QStyle::PM_CheckBoxLabelSpacing, nullptr,
        button);
    note->setContentsMargins(indicator + spacing, 0, 0, 0);
}

QColor SettingsPage::noteColor() {
    // The placeholder color alone has 2.3:1 with the window in the light theme. A note lies
    // on the window or in a group on it: readable on both
    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    const QColor window = theme.palette.window;
    const QColor onWindow =
        readableColor(theme.palette.placeholderText, window, MIN_TEXT_CONTRAST);
    return readableColor(onWindow, groupFillOn(window), MIN_TEXT_CONTRAST);
}

QColor SettingsPage::noteColorOn(const QColor& background) {
    return readableColor(noteColor(), background, MIN_TEXT_CONTRAST);
}

void SettingsPage::updateNoteColors() {
    const QString styleSheet = noteStyleSheet();
    for (QLabel* note : m_notes) {
        note->setStyleSheet(styleSheet);
    }
}

void SettingsPage::markNotUsedYet(QWidget* field, QLabel* label) {
    field->setEnabled(false);
    field->setToolTip(tr("Coming in future version"));
    if (label) {
        label->setEnabled(false);
    }
}

QString SettingsPage::restartText() {
    return tr("Takes effect after restarting Kalahari.");
}

ColorConfigWidget* SettingsPage::addThemeColor(QFormLayout* form, const QString& label,
                                               const std::string& themeKey) {
    auto* widget = new ColorConfigWidget(label);
    form->addRow(widget);

    // The theme in use when the page is filled and its own color: a changed color is kept for
    // that theme, also when Apply changes the theme first
    struct ShownTheme {
        std::string name;
        QColor color;
    };
    auto shown = std::make_shared<ShownTheme>();
    Binding binding;
    binding.key = [shown, themeKey]() {
        return core::ThemeManager::editorColorSetting(shown->name, themeKey);
    };
    binding.stored = [shown, themeKey]() {
        const auto& themes = core::ThemeManager::getInstance();
        shown->name = themes.getCurrentTheme().name;
        shown->color = themes.defaultEditorColor(themeKey, QColor());
        const QColor stored(QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
            core::ThemeManager::editorColorSetting(shown->name, themeKey), std::string())));
        return colorValue(stored.isValid() ? stored : shown->color);
    };
    binding.shown = [widget]() { return colorValue(widget->color()); };
    binding.show = [widget](const json& value) { widget->setColor(colorOf(value)); };
    binding.store = [shown, themeKey](const json& value) {
        // The theme's own color is not kept: the theme decides it again
        auto& settings = core::SettingsManager::getInstance();
        const std::string key = core::ThemeManager::editorColorSetting(shown->name, themeKey);
        const QColor color = colorOf(value);
        if (color == shown->color) {
            settings.removeKey(key);
        } else {
            settings.set<std::string>(key, color.name().toStdString());
        }
    };
    Binding& bound = bindCustom(std::move(binding));
    m_restoreColors.emplace_back([widget, shown]() { widget->setColor(shown->color); });

    // Another theme in use: its colors, once Apply has stored the changed ones
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            [this, &bound, shown]() {
                QTimer::singleShot(0, this, [this, &bound, shown]() {
                    if (core::ThemeManager::getInstance().getCurrentTheme().name != shown->name) {
                        reload(bound);
                    }
                });
            });
    return widget;
}

void SettingsPage::addIssueColor(QFormLayout* form, editor::TextIssueKind kind) {
    // The frames of the kind's cards in the panel have the same color (issueColor())
    addThemeColor(form, tr("Underline"), editor::issueColorKey(kind));
}

QCheckBox* SettingsPage::addOptimizeColorsCheckBox(QFormLayout* form) {
    QCheckBox* box = addCheckBox(form, tr("Optimize colors in themes"),
                                 core::ThemeManager::OPTIMIZE_COLORS_SETTING);
    box->setObjectName(QStringLiteral("optimizeThemeColors"));
    box->setProperty(SHARED_CHECK_BOX, true);
    box->setToolTip(tr("On: a color too light for a light background is shown darker, and one "
                       "too dark for a dark background lighter (on the paper and in the panels). "
                       "Off: the colors exactly as set. One setting for the spelling, the grammar, "
                       "the annotations and the comparison of the versions."));
    return box;
}

QPushButton* SettingsPage::addRestoreColorsButton(QFormLayout* form) {
    auto* button = new QPushButton(tr("Restore Default Colors"));
    auto* row = new QHBoxLayout();
    row->addWidget(button);
    row->addStretch();
    form->addRow(row);

    const std::size_t first = m_restoreButtonColors;
    const std::size_t last = m_restoreColors.size();
    m_restoreButtonColors = last;
    connect(button, &QPushButton::clicked, this, [this, first, last]() {
        for (std::size_t i = first; i < last; ++i) {
            m_restoreColors.at(i)();
        }
    });
    return button;
}

} // namespace gui
} // namespace kalahari
