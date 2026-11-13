/// @file dialog_checks.h
/// @brief What the tests check of the program's windows: their access keys, their size on a
///        small screen, their texts in Polish, the gear

#pragma once

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QWidget>

#include <functional>
#include <map>
#include <utility>

namespace kalahari::test {

/// The access key of a text: the letter after a single "&"
inline QChar accessKey(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts with access keys of a window: of the labels with a field, the buttons, the groups
inline QStringList keyTexts(const QWidget& root) {
    QStringList texts;
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            texts << label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            texts << button->text();
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            texts << group->title();
        }
    }
    return texts;
}

/// Each access key of a window once, a Latin letter (Alt does not reach "ś")
inline void checkKeys(const QWidget& root) {
    std::map<QChar, QStringList> byKey;
    for (const QString& text : keyTexts(root)) {
        const QChar key = accessKey(text);
        if (!key.isNull()) {
            CHECK((key.unicode() < 0x80 && key.isLetterOrNumber()));
            byKey[key] << text;
        }
    }
    for (const auto& [key, texts] : byKey) {
        INFO(texts.join(QStringLiteral(" / ")).toStdString());
        CHECK(texts.size() == 1);
    }
}

/// The window laid out as it shows
inline void layOut(QWidget& window) {
    window.show();
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
}

/// 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger controls need a few more)
inline void checkSmallScreen(const QWidget& window) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(window.sizeHint().width() <= 911);
    CHECK(window.sizeHint().height() <= MAX_HEIGHT);
}

/// The window in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// The program's way of opening the Settings dialog, until the test ends
class SettingsOpener {
public:
    explicit SettingsOpener(std::function<void(QWidget*, const QString&)> opener) {
        gui::dialogs::KalahariDialog::setSettingsOpener(std::move(opener));
    }
    ~SettingsOpener() { gui::dialogs::KalahariDialog::setSettingsOpener(nullptr); }
    SettingsOpener(const SettingsOpener&) = delete;
    SettingsOpener& operator=(const SettingsOpener&) = delete;
};


}  // namespace kalahari::test
