/// @file central_tabs.cpp
/// @brief The tabs in the middle of the window: Qt's one row, or as many rows as the width needs

#include "kalahari/gui/widgets/central_tabs.h"

#include "kalahari/core/settings_manager.h"

#include <QAbstractButton>
#include <QEvent>
#include <QTabBar>

#include <string>

namespace kalahari {
namespace gui {

/// @brief Qt's tab bar of CentralTabs, with the tabs in more rows over it
///
/// In more rows it paints nothing and asks for the height of the rows at the width of the tab
/// widget, which it spans (document mode). The rows show its tabs as they are: they are set
/// again whenever its tabs are laid out (a tab added, closed, moved or renamed), it paints (an
/// icon changed) or a tooltip is about to show.
class CentralTabBar : public QTabBar {
public:
    explicit CentralTabBar(QTabWidget* tabs)
        : QTabBar(tabs), m_tabs(tabs)
    {
        connect(this, &QTabBar::currentChanged, this, [this](int /*index*/) { sync(); });
        // After the tab widget has moved its page (it heard of the move first)
        connect(this, &QTabBar::tabMoved, this, [this](int /*from*/, int /*to*/) { sync(); });
    }

    void setRows(PanelTabBar::Rows rows) {
        const bool wrap = rows == PanelTabBar::Rows::Wrap;
        if (wrap == (m_rows != nullptr)) {
            return;
        }
        if (wrap) {
            m_rows = new PanelTabBar(this);
            m_rows->setObjectName(QStringLiteral("centralTabRows"));
            m_rows->setAccessibleName(CentralTabs::tr("Tabs"));
            m_rows->setDrawing(PanelTabBar::Drawing::Style);
            m_rows->setLook(PanelTabBar::Look::IconsAndNames);
            m_rows->setRows(PanelTabBar::Rows::Wrap);
            m_rows->setClosable(tabsClosable());
            m_rows->installEventFilter(this);
            connect(m_rows, &PanelTabBar::tabChosen, this, &QTabBar::setCurrentIndex);
            connect(m_rows, &PanelTabBar::tabClicked, this, &QTabBar::tabBarClicked);
            connect(m_rows, &PanelTabBar::tabMoveRequested, this, &QTabBar::moveTab);
            connect(m_rows, &PanelTabBar::tabCloseRequested, this, &QTabBar::tabCloseRequested);

            // Qt's arrows and close buttons would show through the rows
            m_scrollButtons = usesScrollButtons();
            setUsesScrollButtons(false);
            showCloseButtons(false);

            // One place in the order of the Tab key, the rows'
            const bool focused = hasFocus();
            m_focusPolicy = focusPolicy();
            setFocusPolicy(Qt::NoFocus);
            setFocusProxy(m_rows);
            m_rows->setGeometry(rect());
            m_rows->show();
            m_rows->raise();
            sync();
            if (focused) {
                m_rows->setFocus(Qt::OtherFocusReason);
            }
        } else {
            const bool focused = m_rows->hasFocus();
            setFocusProxy(nullptr);
            setFocusPolicy(m_focusPolicy);
            m_rows->hide();
            m_rows->deleteLater();
            m_rows = nullptr;
            setUsesScrollButtons(m_scrollButtons);
            showCloseButtons(true);
            if (focused) {
                setFocus(Qt::OtherFocusReason);
            }
        }
        updateGeometry();
        update();
    }

    [[nodiscard]] PanelTabBar* tabRows() const { return m_rows; }

    [[nodiscard]] QSize sizeHint() const override {
        if (m_rows == nullptr) {
            return QTabBar::sizeHint();
        }
        // The bar spans the tab widget: the rows its width needs
        const int width = m_tabs->width();
        return {width, m_rows->heightForWidth(width)};
    }

    [[nodiscard]] QSize minimumSizeHint() const override {
        if (m_rows == nullptr) {
            return QTabBar::minimumSizeHint();
        }
        return {m_rows->minimumSizeHint().width(), sizeHint().height()};
    }

protected:
    void tabInserted(int index) override {
        QTabBar::tabInserted(index);
        if (m_rows != nullptr) {
            showCloseButtons(false);
        }
        sync();
    }

    void tabRemoved(int index) override {
        QTabBar::tabRemoved(index);
        sync();
    }

    void tabLayoutChange() override {
        QTabBar::tabLayoutChange();
        sync();
    }

    void paintEvent(QPaintEvent* event) override {
        if (m_rows == nullptr) {
            QTabBar::paintEvent(event);
            return;
        }
        // Painting asked for by a tab of its own: an icon or a color changed
        sync();
    }

    void resizeEvent(QResizeEvent* event) override {
        QTabBar::resizeEvent(event);
        if (m_rows != nullptr) {
            m_rows->setGeometry(rect());
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_rows && event->type() == QEvent::ToolTip) {
            sync();  // a tooltip set since
        }
        return QTabBar::eventFilter(watched, event);
    }

private:
    /// @brief Show or hide the close buttons of Qt's tabs
    void showCloseButtons(bool shown) {
        for (int i = 0; i < count(); ++i) {
            for (const ButtonPosition side : {LeftSide, RightSide}) {
                if (QWidget* button = tabButton(i, side);
                    qobject_cast<QAbstractButton*>(button) != nullptr) {
                    button->setVisible(shown);
                }
            }
        }
    }

    /// @brief Give the rows the tabs as they are now
    void sync() {
        if (m_rows == nullptr) {
            return;
        }
        QList<PanelTabBar::Tab> tabs;
        tabs.reserve(count());
        for (int i = 0; i < count(); ++i) {
            tabs.append({m_tabs->widget(i), tabIcon(i), tabText(i), tabToolTip(i)});
        }
        m_rows->setIconSize(iconSize().width());
        m_rows->setClosable(tabsClosable());

        // Tabs set again forget the one under the mouse: only when they changed
        bool same = tabs.size() == m_rows->count();
        for (int i = 0; same && i < tabs.size(); ++i) {
            const PanelTabBar::Tab& shown = m_rows->tab(i);
            same = shown.key == tabs.at(i).key && shown.name == tabs.at(i).name &&
                   shown.toolTip == tabs.at(i).toolTip &&
                   shown.icon.cacheKey() == tabs.at(i).icon.cacheKey();
        }
        if (same) {
            m_rows->setCurrentIndex(currentIndex());
            return;
        }
        m_rows->setTabs(tabs, currentIndex());
        updateGeometry();
    }

    QTabWidget* m_tabs;
    PanelTabBar* m_rows = nullptr;  ///< The tabs in more rows, nullptr in one row
    bool m_scrollButtons = true;
    Qt::FocusPolicy m_focusPolicy = Qt::TabFocus;
};

CentralTabs::CentralTabs(QWidget* parent)
    : QTabWidget(parent), m_bar(new CentralTabBar(this))
{
    setObjectName(QStringLiteral("centralTabs"));
    setTabBar(m_bar);
}

void CentralTabs::setRows(PanelTabBar::Rows rows) {
    m_bar->setRows(rows);
}

PanelTabBar::Rows CentralTabs::rows() const {
    return m_bar->tabRows() != nullptr ? PanelTabBar::Rows::Wrap : PanelTabBar::Rows::Single;
}

PanelTabBar* CentralTabs::tabRows() const {
    return m_bar->tabRows();
}

void CentralTabs::applySettings() {
    setRows(rowsSetting());
}

PanelTabBar::Rows CentralTabs::rowsSetting() {
    return core::SettingsManager::getInstance().get<std::string>(ROWS_KEY, ROWS_SINGLE) ==
                   ROWS_WRAP
               ? PanelTabBar::Rows::Wrap
               : PanelTabBar::Rows::Single;
}

} // namespace gui
} // namespace kalahari
