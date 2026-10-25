/// @file material_list_panel.cpp
/// @brief The list of a kind of materials of the Workshop on a tab in the middle of the window

#include "kalahari/gui/panels/material_list_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/shortcut_rules.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCollator>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <optional>

namespace kalahari {
namespace gui {

namespace {

/// What a column without a value shows
const QString NONE = QStringLiteral("–");

/// Size of a thumbnail in the table, and the height of its rows
constexpr int THUMBNAIL_WIDTH = 48;
constexpr int THUMBNAIL_HEIGHT = 36;
constexpr int THUMBNAIL_ROW = 40;

/// Size of a tile and of its picture
constexpr int TILE_WIDTH = 150;
constexpr int TILE_HEIGHT = 150;
constexpr int TILE_PICTURE_HEIGHT = 96;
constexpr int TILE_ICON = 48;  ///< The icon of a material without a picture
constexpr int TILE_MARGIN = 6;
constexpr int TILE_SPACING = 8;

/// Characters of a text from which its tooltip wraps it in lines
constexpr int LONG_TIP = 60;

/// Width of a column at first, by its id; the others fit their header
int widthOf(const QString& id) {
    if (id == QLatin1String(MaterialList::THUMBNAIL)) {
        return THUMBNAIL_WIDTH + 16;
    }
    if (id == QLatin1String(MaterialList::QUOTE)) {
        return 360;
    }
    if (id == QLatin1String(MaterialList::TITLE) || id == QLatin1String(MaterialList::LINKS) ||
        id == QLatin1String(MaterialList::DETAILS) ||
        id == QLatin1String(MaterialList::DESCRIPTION) ||
        id == QLatin1String(MaterialList::ADDRESS)) {
        return 220;
    }
    return 110;
}

/// The pixmap of picture @p path turned by @p rotation, at most @p size large; kept while the
/// file is the same
QPixmap thumbnailOf(const QString& path, int rotation, const QSize& size) {
    static QHash<QString, QPixmap> kept;
    const qreal ratio = qApp->devicePixelRatio();
    const QFileInfo info(path);
    const QString key = QStringLiteral("%1|%2|%3|%4x%5|%6")
                            .arg(path)
                            .arg(info.lastModified().toMSecsSinceEpoch())
                            .arg(rotation)
                            .arg(size.width())
                            .arg(size.height())
                            .arg(ratio);
    if (const auto found = kept.constFind(key); found != kept.constEnd()) {
        return *found;
    }
    const QImage image = readPicture(path, rotation, (QSizeF(size) * ratio).toSize());
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(ratio);
    kept.insert(key, pixmap);
    return pixmap;
}

/// A tooltip of @p text: a long one in lines
QString tipOf(const QString& text) {
    if (text.size() < LONG_TIP) {
        return text;
    }
    return QStringLiteral("<p>%1</p>").arg(text.toHtmlEscaped());
}

/// The fields of a key that a list takes for itself, and the same keys of the window's commands
/// wait while the list has the keys
enum class ListKey { None, Edit, Properties, Add, Rename, Delete, Copy };

ListKey listKeyOf(QKeyEvent* event) {
    const QKeyCombination keys = ShortcutRules::keysOf(event);
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    const bool enter = keys.key() == Qt::Key_Return || keys.key() == Qt::Key_Enter;
    if (enter && modifiers == Qt::NoModifier) {
        return ListKey::Edit;
    }
    if (enter && modifiers == Qt::AltModifier) {
        return ListKey::Properties;
    }
    if ((keys.key() == Qt::Key_Insert && modifiers == Qt::NoModifier) ||
        (enter && modifiers == (Qt::ControlModifier | Qt::ShiftModifier))) {
        return ListKey::Add;
    }
    if (keys.key() == Qt::Key_F2 && modifiers == Qt::NoModifier) {
        return ListKey::Rename;
    }
    if (keys.key() == Qt::Key_Delete && modifiers == Qt::NoModifier) {
        return ListKey::Delete;
    }
    if (keys.key() == Qt::Key_C && modifiers == Qt::ControlModifier) {
        return ListKey::Copy;
    }
    return ListKey::None;
}

/// A view that says why it has no rows
template <typename View>
class SayingView : public View {
public:
    using View::View;

    void setEmptyText(const QString& text) {
        m_emptyText = text;
        this->viewport()->update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        View::paintEvent(event);
        if (this->model() == nullptr || this->model()->rowCount() > 0 || m_emptyText.isEmpty()) {
            return;
        }
        QPainter painter(this->viewport());
        painter.setPen(this->palette().color(QPalette::PlaceholderText));
        painter.drawText(this->viewport()->rect().adjusted(16, 16, -16, -16),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_emptyText);
    }

private:
    QString m_emptyText;
};

using ListTable = SayingView<QTableView>;
using ListTiles = SayingView<QListView>;

/// Paints a tile: the picture or the icon of the material, its title and a line under it
class TileDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        const QWidget* widget = option.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &item, painter, widget);

        const QRect area = option.rect.adjusted(TILE_MARGIN, TILE_MARGIN, -TILE_MARGIN,
                                                -TILE_MARGIN);
        const QRect pictureArea(area.left(), area.top(), area.width(), TILE_PICTURE_HEIGHT);
        const QPixmap picture = index.data(MaterialListPanel::TILE_ROLE).value<QPixmap>();
        if (!picture.isNull()) {
            const QSize shown = picture.deviceIndependentSize().toSize();
            const QRect target(pictureArea.left() + (pictureArea.width() - shown.width()) / 2,
                               pictureArea.top() + (pictureArea.height() - shown.height()) / 2,
                               shown.width(), shown.height());
            painter->drawPixmap(target, picture);
        }

        const bool selected = (option.state & QStyle::State_Selected) != 0;
        const QPalette::ColorGroup group =
            (option.state & QStyle::State_Enabled) != 0 ? QPalette::Normal : QPalette::Disabled;
        const QColor text = option.palette.color(
            group, selected ? QPalette::HighlightedText : QPalette::Text);
        QFont titleFont = option.font;
        titleFont.setBold(true);
        const QFontMetrics titleMetrics(titleFont);
        const QRect titleArea(area.left(), pictureArea.bottom() + 4, area.width(),
                              titleMetrics.height());
        painter->save();
        painter->setFont(titleFont);
        painter->setPen(text);
        painter->drawText(titleArea, Qt::AlignHCenter | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, titleArea.width()));
        const QFontMetrics detailMetrics(option.font);
        const QRect detailArea(area.left(), titleArea.bottom() + 2, area.width(),
                               detailMetrics.height());
        painter->setFont(option.font);
        painter->setPen(selected ? text : option.palette.color(group, QPalette::PlaceholderText));
        painter->drawText(detailArea, Qt::AlignHCenter | Qt::AlignVCenter,
                          detailMetrics.elidedText(
                              index.data(MaterialListPanel::DETAIL_ROLE).toString(),
                              Qt::ElideRight, detailArea.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return {TILE_WIDTH, TILE_HEIGHT};
    }
};

} // namespace

// =============================================================================
// The rows of a list
// =============================================================================

/// @brief The materials of a list (MaterialList::rowsOf()) in the columns of its kind
class MaterialListModel : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;

    void setContents(const QList<MaterialColumn>& columns, const QList<MaterialRow>& rows,
                     bool leadIcon) {
        beginResetModel();
        m_columns = columns;
        m_rows = rows;
        m_leadColumn = -1;
        // The icon of a material is beside its kind in All Materials, else beside its title;
        // the pictures have their thumbnails
        const auto indexOf = [this](const char* id) {
            for (int i = 0; i < m_columns.size(); ++i) {
                if (m_columns.at(i).id == QLatin1String(id)) {
                    return i;
                }
            }
            return -1;
        };
        if (leadIcon) {
            m_leadColumn = indexOf(MaterialList::TYPE);
            if (m_leadColumn < 0) {
                m_leadColumn = indexOf(MaterialList::TITLE);
            }
        }
        m_titleColumn = indexOf(MaterialList::TITLE);
        endResetModel();
    }

    /// The icons again (another theme)
    void refreshIcons() {
        if (!m_rows.isEmpty() && !m_columns.isEmpty()) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                             {Qt::DecorationRole});
        }
    }

    const MaterialRow* rowAt(int row) const {
        return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
    }

    QString columnId(int column) const {
        return column >= 0 && column < m_columns.size() ? m_columns.at(column).id : QString();
    }

    int titleColumn() const { return m_titleColumn; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(m_columns.size());
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation != Qt::Horizontal || section < 0 || section >= m_columns.size()) {
            return {};
        }
        const MaterialColumn& column = m_columns.at(section);
        if (role == Qt::DisplayRole) {
            return column.title;
        }
        if (role == Qt::ToolTipRole && column.sortable) {
            //: The tooltip of the header of a column of the list of materials: %1 its name.
            //: In Polish: Sortuj według: %1
            return MaterialList::tr("Sort by: %1").arg(column.title);
        }
        return {};
    }

    QVariant data(const QModelIndex& index, int role) const override {
        const MaterialRow* row = index.isValid() ? rowAt(index.row()) : nullptr;
        if (row == nullptr || index.column() >= m_columns.size()) {
            return {};
        }
        const QString& id = m_columns.at(index.column()).id;
        const bool thumbnail = id == QLatin1String(MaterialList::THUMBNAIL);
        const QString text = row->texts.value(id);
        switch (role) {
        case Qt::DisplayRole:
            if (thumbnail) {
                return {};
            }
            return text.isEmpty() && index.column() != m_titleColumn ? NONE : text;
        case Qt::ToolTipRole:
            return text.isEmpty() ? QVariant() : QVariant(tipOf(text));
        case Qt::DecorationRole:
            if (thumbnail) {
                if (!row->picture.isEmpty()) {
                    return thumbnailOf(row->picture, row->rotation,
                                       {THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT});
                }
                return core::ArtProvider::getInstance().getIcon(row->iconId,
                                                                core::IconContext::TreeView);
            }
            if (index.column() == m_leadColumn) {
                return core::ArtProvider::getInstance().getIcon(row->iconId,
                                                                core::IconContext::TreeView);
            }
            return {};
        case Qt::AccessibleTextRole:
            return text.isEmpty() ? m_columns.at(index.column()).title : text;
        case MaterialListPanel::ID_ROLE:
            return row->id;
        case MaterialListPanel::SORT_ROLE:
            return row->order.contains(id) ? row->order.value(id) : QVariant(text);
        case MaterialListPanel::DETAIL_ROLE:
            return detailOf(*row);
        case MaterialListPanel::TILE_ROLE:
            // A material without a picture has its icon on its tile
            if (row->picture.isEmpty()) {
                const QIcon icon = core::ArtProvider::getInstance().getIcon(
                    row->iconId, core::IconContext::TreeView);
                return icon.pixmap(QSize(TILE_ICON, TILE_ICON), qApp->devicePixelRatio());
            }
            return thumbnailOf(row->picture, row->rotation,
                               {TILE_WIDTH - 2 * TILE_MARGIN, TILE_PICTURE_HEIGHT});
        default:
            return {};
        }
    }

private:
    /// The line of a tile under its title: what a material of the kind is told by at once
    static QString detailOf(const MaterialRow& row) {
        QStringList parts;
        for (const char* id : {MaterialList::DOCUMENT_KIND, MaterialList::FORMAT,
                               MaterialList::DIMENSIONS, MaterialList::SIZE}) {
            const QString text = row.texts.value(QString::fromLatin1(id));
            if (!text.isEmpty()) {
                parts << text;
            }
        }
        return parts.isEmpty() ? row.texts.value(QString::fromLatin1(MaterialList::DETAILS))
                               : parts.join(QStringLiteral(" · "));
    }

    QList<MaterialColumn> m_columns;
    QList<MaterialRow> m_rows;
    int m_leadColumn = -1;
    int m_titleColumn = -1;
};

/// @brief The rows of a list that the search and the filters let through, sorted
class MaterialListFilter : public QSortFilterProxyModel {
public:
    explicit MaterialListFilter(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {
        m_collator.setNumericMode(true);
        m_collator.setCaseSensitivity(Qt::CaseInsensitive);
        setSortRole(MaterialListPanel::SORT_ROLE);
    }

    void setFilters(const QString& search, const QString& linkedWith, bool withoutLinks,
                    const QHash<QString, QString>& columnFilter) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif
        m_words = search.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        m_linkedWith = linkedWith;
        m_withoutLinks = withoutLinks;
        m_columnFilter.clear();
        for (auto it = columnFilter.cbegin(); it != columnFilter.cend(); ++it) {
            const QString words = it.value().trimmed().toLower();
            if (!words.isEmpty()) {
                m_columnFilter.insert(it.key(), words);
            }
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateRowsFilter();
#endif
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        Q_UNUSED(sourceParent);
        const auto* model = static_cast<const MaterialListModel*>(sourceModel());
        const MaterialRow* row = model->rowAt(sourceRow);
        if (row == nullptr) {
            return false;
        }
        for (const QString& word : m_words) {
            if (!row->search.contains(word)) {
                return false;
            }
        }
        if (!m_linkedWith.isEmpty() && !row->links.contains(m_linkedWith)) {
            return false;
        }
        if (m_withoutLinks && !row->links.isEmpty()) {
            return false;
        }
        for (auto it = m_columnFilter.cbegin(); it != m_columnFilter.cend(); ++it) {
            if (!row->texts.value(it.key()).toLower().contains(it.value())) {
                return false;
            }
        }
        return true;
    }

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        const QVariant a = left.data(MaterialListPanel::SORT_ROLE);
        const QVariant b = right.data(MaterialListPanel::SORT_ROLE);
        int compared = 0;
        if (a.typeId() == QMetaType::QDateTime && b.typeId() == QMetaType::QDateTime) {
            compared = a.toDateTime() < b.toDateTime() ? -1 : (b.toDateTime() < a.toDateTime());
        } else if (a.typeId() != QMetaType::QString && b.typeId() != QMetaType::QString &&
                   a.canConvert<qlonglong>() && b.canConvert<qlonglong>()) {
            compared = a.toLongLong() < b.toLongLong() ? -1 : (b.toLongLong() < a.toLongLong());
        } else {
            // A material without a value comes after those with one
            const QString first = a.toString();
            const QString second = b.toString();
            if (first.isEmpty() != second.isEmpty()) {
                return second.isEmpty() == (sortOrder() == Qt::AscendingOrder);
            }
            compared = m_collator.compare(first, second);
        }
        if (compared != 0) {
            return compared < 0;
        }
        // The same value: by the titles
        const auto* model = static_cast<const MaterialListModel*>(sourceModel());
        const int title = model->titleColumn();
        if (title < 0 || title == left.column()) {
            return left.row() < right.row();
        }
        return m_collator.compare(left.siblingAtColumn(title).data().toString(),
                                  right.siblingAtColumn(title).data().toString()) < 0;
    }

private:
    QCollator m_collator;
    QStringList m_words;
    QString m_linkedWith;
    bool m_withoutLinks = false;
    QHash<QString, QString> m_columnFilter;
};

/// @brief The header of the table, with a field under each column that filters it
class FilterHeader : public QHeaderView {
public:
    explicit FilterHeader(QWidget* parent = nullptr) : QHeaderView(Qt::Horizontal, parent) {
        setSectionsClickable(true);
        setSectionsMovable(true);
        setSortIndicatorShown(true);
        setHighlightSections(false);
        setStretchLastSection(true);
        connect(this, &QHeaderView::sectionResized, this, [this]() { placeFields(); });
        connect(this, &QHeaderView::sectionMoved, this, [this]() { placeFields(); });
    }

    /// The fields of columns @p columns; @p changed gets the column and the text typed
    void setColumns(const QList<MaterialColumn>& columns,
                    const std::function<void(const QString&, const QString&)>& changed) {
        qDeleteAll(m_fields);
        m_fields.clear();
        m_ids.clear();
        for (const MaterialColumn& column : columns) {
            auto* field = new QLineEdit(this);
            field->setObjectName(QStringLiteral("columnFilter_") + column.id);
            field->setClearButtonEnabled(true);
            //: The field under the header of a column that filters it. In Polish: filtr
            field->setPlaceholderText(MaterialList::tr("filter"));
            //: %1: the name of the column. In Polish: Filtr kolumny %1
            field->setAccessibleName(MaterialList::tr("Filter of column %1").arg(column.title));
            field->setEnabled(column.sortable);
            field->hide();
            const QString id = column.id;
            connect(field, &QLineEdit::textChanged, this,
                    [changed, id](const QString& text) { changed(id, text); });
            m_fields << field;
            m_ids << column.id;
        }
        placeFields();
    }

    QLineEdit* fieldOf(const QString& columnId) const {
        const qsizetype index = m_ids.indexOf(columnId);
        return index >= 0 ? m_fields.at(index) : nullptr;
    }

    void setFieldsShown(bool shown) {
        if (m_shown == shown) {
            return;
        }
        m_shown = shown;
        updateGeometries();
        emit geometriesChanged();
        placeFields();
    }

    bool fieldsShown() const { return m_shown; }

    QSize sizeHint() const override {
        QSize size = QHeaderView::sizeHint();
        if (m_shown) {
            size.setHeight(size.height() + fieldHeight());
        }
        return size;
    }

    /// The fields follow the columns: where they are, how wide, scrolled
    void placeFields() {
        const int height = fieldHeight();
        for (int i = 0; i < m_fields.size(); ++i) {
            QLineEdit* field = m_fields.at(i);
            if (!m_shown || isSectionHidden(i)) {
                field->hide();
                continue;
            }
            field->setGeometry(sectionViewportPosition(i), this->height() - height,
                               sectionSize(i), height);
            field->show();
        }
    }

protected:
    void updateGeometries() override {
        setViewportMargins(0, 0, 0, m_shown ? fieldHeight() : 0);
        QHeaderView::updateGeometries();
        placeFields();
    }

private:
    int fieldHeight() const {
        return m_fields.isEmpty() ? QLineEdit().sizeHint().height()
                                  : m_fields.constFirst()->sizeHint().height();
    }

    QList<QLineEdit*> m_fields;
    QStringList m_ids;
    bool m_shown = false;
};

// =============================================================================
// The list
// =============================================================================

MaterialListPanel::MaterialListPanel(const QString& list, QWidget* parent)
    : QWidget(parent)
    , m_list(list) {
    setObjectName(QStringLiteral("materialList"));
    m_model = new MaterialListModel(this);
    m_filter = new MaterialListFilter(this);
    m_filter->setSourceModel(m_model);
    createLayout();
    createCommands();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &MaterialListPanel::refreshIcons);
    load();
    restoreState();
}

MaterialListPanel::~MaterialListPanel() {
    // What the views do while they are destroyed (the current row cleared) does not reach the
    // panel
    m_table->selectionModel()->disconnect(this);
    m_tiles->selectionModel()->disconnect(this);
}

void MaterialListPanel::createLayout() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The kind and the commands of the material chosen
    auto* tools = new QWidget(this);
    tools->setObjectName(QStringLiteral("materialListTools"));
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(6, 4, 6, 4);
    toolsLayout->setSpacing(2);
    auto* icon = new QLabel(tools);
    icon->setObjectName(QStringLiteral("materialListIcon"));
    m_heading = new QLabel(tools);
    m_heading->setObjectName(QStringLiteral("materialListHeading"));
    m_heading->setTextFormat(Qt::RichText);
    toolsLayout->addWidget(icon);
    toolsLayout->addWidget(m_heading);
    const auto separator = [](QWidget* parent) {
        auto* line = new QFrame(parent);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        return line;
    };
    const auto button = [tools](const char* name, bool text) {
        auto* tool = new QToolButton(tools);
        tool->setObjectName(QString::fromLatin1(name));
        tool->setAutoRaise(true);
        tool->setToolButtonStyle(text ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
        return tool;
    };
    toolsLayout->addSpacing(6);
    toolsLayout->addWidget(separator(tools));
    m_addButton = button("materialListAdd", true);
    m_editButton = button("materialListEdit", true);
    m_linkButton = button("materialListLink", false);
    m_copyButton = button("materialListCopy", false);
    m_deleteButton = button("materialListDelete", false);
    m_tableButton = button("materialListTable", false);
    m_tilesButton = button("materialListTiles", false);
    m_columnsButton = button("materialListColumns", false);
    m_tableButton->setCheckable(true);
    m_tilesButton->setCheckable(true);
    for (QToolButton* tool : {m_addButton, m_editButton, m_linkButton, m_copyButton,
                              m_deleteButton}) {
        toolsLayout->addWidget(tool);
    }
    auto* viewSeparator = separator(tools);
    viewSeparator->setObjectName(QStringLiteral("materialListViewSeparator"));
    toolsLayout->addWidget(viewSeparator);
    toolsLayout->addWidget(m_tableButton);
    toolsLayout->addWidget(m_tilesButton);
    toolsLayout->addWidget(separator(tools));
    toolsLayout->addWidget(m_columnsButton);
    toolsLayout->addStretch();
    layout->addWidget(tools);

    // The search and the filters
    auto* filters = new QWidget(this);
    filters->setObjectName(QStringLiteral("materialListFilters"));
    auto* filtersLayout = new QHBoxLayout(filters);
    filtersLayout->setContentsMargins(6, 2, 6, 4);
    filtersLayout->setSpacing(8);
    m_search = new QLineEdit(filters);
    m_search->setObjectName(QStringLiteral("materialListSearch"));
    m_search->setClearButtonEnabled(true);
    //: The search of a list of materials; %1: its keys. In Polish: Szukaj (%1)
    m_search->setPlaceholderText(
        MaterialList::tr("Search (%1)")
            .arg(ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_F))));
    //: In Polish: Szukaj w materiałach
    m_search->setAccessibleName(MaterialList::tr("Search in the materials"));
    m_search->setMinimumWidth(180);
    m_search->installEventFilter(this);
    //: The filter of a list of materials by what they are linked with. In Polish: Powiązane z:
    auto* linkedLabel = new QLabel(MaterialList::tr("Linked with:"), filters);
    m_linkedWith = new QComboBox(filters);
    m_linkedWith->setObjectName(QStringLiteral("materialListLinkedWith"));
    m_linkedWith->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    linkedLabel->setBuddy(m_linkedWith);
    //: The filter of a list of materials: those not linked with anything. In Polish: Bez
    //: powiązań
    m_withoutLinks = new QCheckBox(MaterialList::tr("Without links"), filters);
    m_withoutLinks->setObjectName(QStringLiteral("materialListWithoutLinks"));
    m_columnFiltersButton = new QToolButton(filters);
    m_columnFiltersButton->setObjectName(QStringLiteral("materialListColumnFilters"));
    m_columnFiltersButton->setAutoRaise(true);
    m_columnFiltersButton->setCheckable(true);
    m_columnFiltersButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    //: Shows a field under the header of each column that filters it. In Polish: Filtry kolumn
    m_columnFiltersButton->setText(MaterialList::tr("Column Filters"));
    m_clearButton = new QToolButton(filters);
    m_clearButton->setObjectName(QStringLiteral("materialListClearFilters"));
    m_clearButton->setAutoRaise(true);
    //: In Polish: Wyczyść filtry
    m_clearButton->setText(MaterialList::tr("Clear Filters"));
    filtersLayout->addWidget(m_search);
    filtersLayout->addWidget(linkedLabel);
    filtersLayout->addWidget(m_linkedWith);
    filtersLayout->addWidget(m_withoutLinks);
    filtersLayout->addWidget(m_columnFiltersButton);
    filtersLayout->addWidget(m_clearButton);
    filtersLayout->addStretch();
    layout->addWidget(filters);

    // The table and the tiles
    m_stack = new QStackedWidget(this);
    auto* table = new ListTable(m_stack);
    m_table = table;
    m_table->setObjectName(QStringLiteral("materialListTable"));
    m_header = new FilterHeader(m_table);
    m_table->setHorizontalHeader(m_header);
    m_table->setModel(m_filter);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    //: In Polish: Lista materiałów
    m_table->setAccessibleName(MaterialList::tr("List of materials"));
    auto* tiles = new ListTiles(m_stack);
    m_tiles = tiles;
    m_tiles->setObjectName(QStringLiteral("materialListTiles"));
    m_tiles->setModel(m_filter);
    m_tiles->setViewMode(QListView::IconMode);
    m_tiles->setResizeMode(QListView::Adjust);
    m_tiles->setMovement(QListView::Static);
    m_tiles->setWrapping(true);
    m_tiles->setUniformItemSizes(true);
    m_tiles->setSpacing(TILE_SPACING / 2);
    m_tiles->setGridSize({TILE_WIDTH + TILE_SPACING, TILE_HEIGHT + TILE_SPACING});
    m_tiles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tiles->setItemDelegate(new TileDelegate(m_tiles));
    m_tiles->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tiles->setAccessibleName(MaterialList::tr("List of materials"));
    m_stack->addWidget(m_table);
    m_stack->addWidget(m_tiles);
    layout->addWidget(m_stack, 1);

    // How many are shown, and how they are sorted
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("materialListFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(8, 2, 8, 2);
    m_footer = new QLabel(footer);
    m_footer->setObjectName(QStringLiteral("materialListCount"));
    m_sorting = new QLabel(footer);
    m_sorting->setObjectName(QStringLiteral("materialListSorting"));
    footerLayout->addWidget(m_footer);
    footerLayout->addStretch();
    footerLayout->addWidget(m_sorting);
    layout->addWidget(footer);

    for (QAbstractItemView* view : {static_cast<QAbstractItemView*>(m_table),
                                    static_cast<QAbstractItemView*>(m_tiles)}) {
        view->installEventFilter(this);
        connect(view, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
            if (const QString id = idAt(index); !id.isEmpty()) {
                emit requestOpen(id);
            }
        });
        connect(view, &QWidget::customContextMenuRequested, this,
                [this, view](const QPoint& pos) { showMenuAt(view, pos); });
        connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this, view](const QModelIndex& current) { onCurrentChanged(view, current); });
    }
    connect(m_header, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showColumnsMenu(m_header->mapToGlobal(pos)); });
    connect(m_header, &QHeaderView::sectionMoved, this, [this]() {
        if (!m_loading) {
            keepColumns();
        }
    });
    connect(m_header, &QHeaderView::sortIndicatorChanged, this, [this]() { updateFooter(); });
    connect(m_table->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [this]() { m_header->placeFields(); });
    connect(m_filter, &QAbstractItemModel::rowsInserted, this, [this]() { updateFooter(); });
    connect(m_filter, &QAbstractItemModel::rowsRemoved, this, [this]() { updateFooter(); });
    connect(m_filter, &QAbstractItemModel::modelReset, this, [this]() { updateFooter(); });
    connect(m_filter, &QAbstractItemModel::layoutChanged, this, [this]() { updateFooter(); });

    connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
    connect(m_linkedWith, &QComboBox::currentIndexChanged, this, [this]() { applyFilters(); });
    connect(m_withoutLinks, &QCheckBox::toggled, this, [this]() { applyFilters(); });
    connect(m_columnFiltersButton, &QToolButton::toggled, this,
            [this](bool shown) { showColumnFilters(shown); });
    connect(m_clearButton, &QToolButton::clicked, this, [this]() {
        const QSignalBlocker blockSearch(m_search);
        const QSignalBlocker blockLinked(m_linkedWith);
        const QSignalBlocker blockWithout(m_withoutLinks);
        m_search->clear();
        m_linkedWith->setCurrentIndex(0);
        m_withoutLinks->setChecked(false);
        for (const MaterialColumn& column : m_columns) {
            if (QLineEdit* field = m_header->fieldOf(column.id)) {
                const QSignalBlocker blockField(field);
                field->clear();
            }
        }
        m_states[m_list].columnFilter.clear();
        applyFilters();
        focusList();
    });
    setFocusProxy(m_table);
}

void MaterialListPanel::createCommands() {
    //: Edits the material chosen: its card in the middle of the window, or the editor of a
    //: working note. In Polish: Edytuj
    m_editButton->setText(MaterialList::tr("Edit"));
    //: %1: the keys. In Polish: Edytuj materiał na jego karcie, a notatkę roboczą w edytorze
    //: (%1)
    m_editButton->setToolTip(
        MaterialList::tr("Edit the material on its card; a working note in the editor (%1)")
            .arg(ShortcutRules::keysText(QKeyCombination(Qt::NoModifier, Qt::Key_Return))));
    //: In Polish: Powiąż...
    m_linkButton->setToolTip(MaterialList::tr("Link..."));
    //: %1: the keys. In Polish: Kopiuj (%1)
    m_copyButton->setToolTip(MaterialList::tr("Copy (%1)").arg(
        ShortcutRules::keysText(QKeyCombination(Qt::ControlModifier, Qt::Key_C))));
    //: %1: the keys. In Polish: Usuń (%1)
    m_deleteButton->setToolTip(MaterialList::tr("Delete (%1)").arg(
        ShortcutRules::keysText(QKeyCombination(Qt::NoModifier, Qt::Key_Delete))));
    //: Shows the materials in a table. In Polish: Tabela
    m_tableButton->setToolTip(MaterialList::tr("Table"));
    //: Shows the materials as tiles with their pictures. In Polish: Kafelki
    m_tilesButton->setToolTip(MaterialList::tr("Tiles"));
    //: Chooses the columns of the table. In Polish: Kolumny...
    m_columnsButton->setToolTip(MaterialList::tr("Columns..."));
    m_searchIcon = m_search->addAction(QIcon(), QLineEdit::LeadingPosition);

    connect(m_addButton, &QToolButton::clicked, this, [this]() { emit requestAdd(m_list); });
    connect(m_editButton, &QToolButton::clicked, this, [this]() {
        if (const MaterialRow* row = m_model->rowAt(
                m_filter->mapToSource(view()->currentIndex()).row())) {
            emit requestEdit(row->id, row->texts.contains(QString::fromLatin1(
                                          MaterialList::QUOTE)));
        }
    });
    connect(m_linkButton, &QToolButton::clicked, this, [this]() {
        if (const QString id = currentMaterial(); !id.isEmpty()) {
            emit requestLink(id);
        }
    });
    connect(m_copyButton, &QToolButton::clicked, this, &MaterialListPanel::copyCurrent);
    connect(m_deleteButton, &QToolButton::clicked, this, [this]() {
        if (const QString id = currentMaterial(); !id.isEmpty()) {
            emit requestDelete(id);
        }
    });
    connect(m_tableButton, &QToolButton::clicked, this, [this]() { showTiles(false); });
    connect(m_tilesButton, &QToolButton::clicked, this, [this]() { showTiles(true); });
    connect(m_columnsButton, &QToolButton::clicked, this, [this]() {
        showColumnsMenu(m_columnsButton->mapToGlobal(QPoint(0, m_columnsButton->height())));
    });
}

void MaterialListPanel::setList(const QString& list) {
    if (list == m_list) {
        return;
    }
    keepState();
    m_list = list;
    load();
    restoreState();
}

void MaterialListPanel::refresh() {
    keepState();
    load();
    restoreState();
}

void MaterialListPanel::refreshIcons() {
    auto& art = core::ArtProvider::getInstance();
    if (auto* icon = findChild<QLabel*>(QStringLiteral("materialListIcon"))) {
        const int size = art.getIconSize(core::IconContext::Panel);
        icon->setPixmap(art.getIcon(MaterialList::iconIdOf(m_list), core::IconContext::Panel)
                            .pixmap(size, size));
    }
    const auto iconOf = [&art](const char* id) {
        return art.getIcon(QString::fromLatin1(id), core::IconContext::Menu);
    };
    m_addButton->setIcon(iconOf("book.addElement"));
    m_editButton->setIcon(iconOf("edit.rename"));
    m_linkButton->setIcon(iconOf("common.link"));
    m_copyButton->setIcon(iconOf("edit.copy"));
    m_deleteButton->setIcon(iconOf("edit.delete"));
    m_tableButton->setIcon(iconOf("materials.table"));
    m_tilesButton->setIcon(iconOf("materials.tiles"));
    m_columnsButton->setIcon(iconOf("materials.columns"));
    m_columnFiltersButton->setIcon(iconOf("common.filterList"));
    m_searchIcon->setIcon(iconOf("edit.find"));
    m_model->refreshIcons();
}

QString MaterialListPanel::currentMaterial() const {
    return idAt(view()->currentIndex());
}

bool MaterialListPanel::selectMaterial(const QString& elementId) {
    for (int row = 0; row < m_filter->rowCount(); ++row) {
        const QModelIndex index = m_filter->index(row, 0);
        if (index.data(ID_ROLE).toString() != elementId) {
            continue;
        }
        QAbstractItemView* shown = view();
        const QModelIndex target =
            shown == m_tiles ? index.siblingAtColumn(m_tiles->modelColumn()) : index;
        shown->selectionModel()->setCurrentIndex(
            target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        shown->scrollTo(target);
        return true;
    }
    return false;
}

void MaterialListPanel::focusSearch() {
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void MaterialListPanel::focusList() {
    view()->setFocus(Qt::OtherFocusReason);
}

QAbstractItemView* MaterialListPanel::view() const {
    return m_stack->currentWidget() == m_tiles ? static_cast<QAbstractItemView*>(m_tiles)
                                               : static_cast<QAbstractItemView*>(m_table);
}

QSortFilterProxyModel* MaterialListPanel::rows() const {
    return m_filter;
}

void MaterialListPanel::showTiles(bool tiles) {
    tiles = tiles && MaterialList::hasTiles(m_list);
    const QString current = currentMaterial();
    const bool focused = view()->hasFocus();
    m_stack->setCurrentWidget(tiles ? static_cast<QWidget*>(m_tiles)
                                    : static_cast<QWidget*>(m_table));
    setFocusProxy(view());
    m_tableButton->setChecked(!tiles);
    m_tilesButton->setChecked(tiles);
    m_columnsButton->setEnabled(!tiles);
    m_columnFiltersButton->setEnabled(!tiles);
    if (MaterialList::hasTiles(m_list) && MaterialList::tilesShown(m_list) != tiles) {
        MaterialList::setTilesShown(m_list, tiles);
    }
    if (!current.isEmpty()) {
        const bool loading = m_loading;
        m_loading = true;
        selectMaterial(current);
        m_loading = loading;
    }
    if (focused) {
        focusList();
    }
    updateFooter();
}

void MaterialListPanel::showColumnFilters(bool shown) {
    if (m_columnFiltersButton->isChecked() != shown) {
        const QSignalBlocker blocker(m_columnFiltersButton);
        m_columnFiltersButton->setChecked(shown);
    }
    m_states[m_list].columnFilters = shown;
    m_header->setFieldsShown(shown);
    if (shown) {
        // The first field of the columns shown gets the keys
        for (int visual = 0; visual < m_header->count(); ++visual) {
            const int logical = m_header->logicalIndex(visual);
            QLineEdit* field = m_header->fieldOf(m_model->columnId(logical));
            if (!m_header->isSectionHidden(logical) && field != nullptr && field->isEnabled()) {
                if (m_columnFiltersButton->hasFocus()) {
                    field->setFocus(Qt::OtherFocusReason);
                }
                break;
            }
        }
    }
    applyFilters();
}

void MaterialListPanel::setColumnFilter(const QString& columnId, const QString& text) {
    showColumnFilters(true);
    if (QLineEdit* field = m_header->fieldOf(columnId)) {
        field->setText(text);
    }
}

void MaterialListPanel::showColumn(const QString& columnId, bool shown) {
    const int column = columnOf(columnId);
    if (column < 0 || m_header->isSectionHidden(column) == !shown) {
        return;
    }
    // One column stays
    if (!shown && shownColumns().size() <= 1) {
        return;
    }
    m_header->setSectionHidden(column, !shown);
    if (shown && m_header->sectionSize(column) < 20) {
        m_header->resizeSection(column, widthOf(columnId));
    }
    keepColumns();
    m_header->placeFields();
    applyFilters();
}

QStringList MaterialListPanel::shownColumns() const {
    QStringList shown;
    for (int visual = 0; visual < m_header->count(); ++visual) {
        const int logical = m_header->logicalIndex(visual);
        if (!m_header->isSectionHidden(logical)) {
            shown << m_model->columnId(logical);
        }
    }
    return shown;
}

void MaterialListPanel::fillMenu(QMenu& menu) {
    auto& art = core::ArtProvider::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const QModelIndex current = view()->currentIndex();
    const MaterialRow* row = m_model->rowAt(m_filter->mapToSource(current).row());
    const bool changing = !pm.isReadOnly();
    const auto withKeys = [](const QString& text, const QKeyCombination& keys) {
        return text + QLatin1Char('\t') + ShortcutRules::keysText(keys);
    };
    if (row != nullptr) {
        const QString id = row->id;
        const bool quote = row->texts.contains(QString::fromLatin1(MaterialList::QUOTE));
        //: Edits a material: its card in the middle of the window, or the editor of a working
        //: note. In Polish: &Edytuj
        QAction* edit = menu.addAction(art.getIcon(QStringLiteral("edit.rename"),
                                                   core::IconContext::Menu),
                                       withKeys(MaterialList::tr("&Edit"),
                                                QKeyCombination(Qt::NoModifier, Qt::Key_Return)));
        QFont bold = edit->font();
        bold.setBold(true);
        edit->setFont(bold);
        connect(edit, &QAction::triggered, this, [this, id, quote]() { emit requestEdit(id, quote); });
        if (!row->text) {
            //: Opens a material as the settings say (a double-click): its preview, the system
            //: program, the browser. In Polish: &Otwórz
            QAction* open = menu.addAction(
                art.getIcon(QStringLiteral("common.openExternal"), core::IconContext::Menu),
                MaterialList::tr("&Open"));
            connect(open, &QAction::triggered, this, [this, id]() { emit requestOpen(id); });
            if (!row->texts.value(QString::fromLatin1(MaterialList::FILE)).isEmpty()) {
                //: In Polish: Pokaż w &folderze
                QAction* folder = menu.addAction(
                    art.getIcon(QStringLiteral("common.folder"), core::IconContext::Menu),
                    MaterialList::tr("Show in &Folder"));
                connect(folder, &QAction::triggered, this,
                        [this, id]() { emit requestShowInFolder(id); });
            }
        }
        menu.addSeparator();
        //: In Polish: &Kopiuj
        QAction* copy = menu.addAction(
            art.getIcon(QStringLiteral("edit.copy"), core::IconContext::Menu),
            withKeys(MaterialList::tr("&Copy"), QKeyCombination(Qt::ControlModifier, Qt::Key_C)));
        connect(copy, &QAction::triggered, this, &MaterialListPanel::copyCurrent);
        if (!row->text) {
            //: In Polish: &Powiąż...
            QAction* link = menu.addAction(
                art.getIcon(QStringLiteral("common.link"), core::IconContext::Menu),
                MaterialList::tr("&Link..."));
            link->setEnabled(changing);
            connect(link, &QAction::triggered, this, [this, id]() { emit requestLink(id); });
        }
        //: In Polish: Zmień &nazwę...
        QAction* rename = menu.addAction(withKeys(MaterialList::tr("Rena&me..."),
                                                  QKeyCombination(Qt::NoModifier, Qt::Key_F2)));
        rename->setEnabled(changing);
        connect(rename, &QAction::triggered, this, [this, id]() { emit requestRename(id); });
        menu.addSeparator();
        //: In Polish: &Usuń...
        QAction* remove = menu.addAction(
            art.getIcon(QStringLiteral("edit.delete"), core::IconContext::Menu),
            withKeys(MaterialList::tr("&Delete..."),
                     QKeyCombination(Qt::NoModifier, Qt::Key_Delete)));
        remove->setEnabled(changing);
        connect(remove, &QAction::triggered, this, [this, id]() { emit requestDelete(id); });
        //: In Polish: Właś&ciwości
        QAction* properties = menu.addAction(
            withKeys(MaterialList::tr("P&roperties"),
                     QKeyCombination(Qt::AltModifier, Qt::Key_Return)));
        connect(properties, &QAction::triggered, this,
                [this, id]() { emit requestProperties(id); });
        menu.addSeparator();
    }
    QAction* add = menu.addAction(m_addButton->icon(),
                                  withKeys(m_addButton->text(),
                                           QKeyCombination(Qt::NoModifier, Qt::Key_Insert)));
    add->setEnabled(m_addButton->isEnabled());
    connect(add, &QAction::triggered, this, [this]() { emit requestAdd(m_list); });
}

void MaterialListPanel::copyCurrent() {
    const MaterialRow* row = m_model->rowAt(m_filter->mapToSource(view()->currentIndex()).row());
    if (row == nullptr || row->copy.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(row->copy);
    //: The status bar after Copy in a list of materials; %1: the title of the material. In
    //: Polish: Skopiowano: %1
    emit message(MaterialList::tr("Copied: %1")
                     .arg(row->texts.value(QString::fromLatin1(MaterialList::TITLE))));
}

bool MaterialListPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        // Esc clears the search, then gives the keys to the list; Down goes to the list
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier) {
            if (!m_search->text().isEmpty()) {
                m_search->clear();
            } else {
                focusList();
            }
            return true;
        }
        if (key->key() == Qt::Key_Down && key->modifiers() == Qt::NoModifier) {
            focusList();
            return true;
        }
    }
    if ((watched == m_table || watched == m_tiles) &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (listKeyOf(key) == ListKey::None) {
            return QWidget::eventFilter(watched, event);
        }
        if (event->type() == QEvent::ShortcutOverride) {
            key->accept();
            return true;
        }
        return handleKey(static_cast<QAbstractItemView*>(watched), key);
    }
    return QWidget::eventFilter(watched, event);
}

bool MaterialListPanel::handleKey(QAbstractItemView* view, QKeyEvent* key) {
    const MaterialRow* row = m_model->rowAt(m_filter->mapToSource(view->currentIndex()).row());
    const ListKey what = listKeyOf(key);
    if (what == ListKey::Add) {
        if (m_addButton->isEnabled()) {
            emit requestAdd(m_list);
        }
        return true;
    }
    if (row == nullptr) {
        return true;
    }
    const bool changing = !core::ProjectManager::getInstance().isReadOnly();
    switch (what) {
    case ListKey::Edit:
        emit requestEdit(row->id, row->texts.contains(QString::fromLatin1(MaterialList::QUOTE)));
        break;
    case ListKey::Properties:
        emit requestProperties(row->id);
        break;
    case ListKey::Rename:
        if (changing) {
            emit requestRename(row->id);
        }
        break;
    case ListKey::Delete:
        if (changing) {
            emit requestDelete(row->id);
        }
        break;
    case ListKey::Copy:
        copyCurrent();
        break;
    default:
        break;
    }
    return true;
}

void MaterialListPanel::load() {
    const bool loading = m_loading;
    m_loading = true;
    m_columns = MaterialList::columnsOf(m_list);
    const bool pictures = std::any_of(m_columns.cbegin(), m_columns.cend(),
                                      [](const MaterialColumn& column) {
                                          return column.id ==
                                                 QLatin1String(MaterialList::THUMBNAIL);
                                      });
    m_model->setContents(m_columns, MaterialList::rowsOf(m_list), !pictures);
    m_table->verticalHeader()->setDefaultSectionSize(
        pictures ? THUMBNAIL_ROW : m_table->fontMetrics().height() + 8);
    m_table->setIconSize(pictures ? QSize(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT) : QSize(16, 16));
    m_tiles->setModelColumn(std::max(0, m_model->titleColumn()));
    m_header->setColumns(m_columns, [this](const QString& id, const QString& text) {
        m_states[m_list].columnFilter.insert(id, text);
        applyFilters();
    });
    for (int column = 0; column < m_columns.size(); ++column) {
        m_header->resizeSection(column, widthOf(m_columns.at(column).id));
    }
    applyColumns();
    fillLinkedWith();

    // The empty list says why
    QString empty;
    if (m_model->rowCount() == 0) {
        //: A list of materials without any; %1: its name, "Quotes"; %2: the keys of Add. In
        //: Polish: %1: na razie pusto. „Dodaj” (%2) dodaje nowy materiał.
        empty = MaterialList::tr("%1: nothing yet. Add (%2) adds one.")
                    .arg(MaterialList::nameOf(m_list),
                         ShortcutRules::keysText(QKeyCombination(Qt::NoModifier, Qt::Key_Insert)));
    } else {
        //: In Polish: Żaden materiał nie pasuje do filtrów.
        empty = MaterialList::tr("No material matches the filters.");
    }
    static_cast<ListTable*>(m_table)->setEmptyText(empty);
    static_cast<ListTiles*>(m_tiles)->setEmptyText(empty);

    const bool tiles = MaterialList::hasTiles(m_list);
    m_tableButton->setVisible(tiles);
    m_tilesButton->setVisible(tiles);
    if (auto* line = findChild<QFrame*>(QStringLiteral("materialListViewSeparator"))) {
        line->setVisible(tiles);
    }
    showTiles(MaterialList::tilesShown(m_list));
    refreshIcons();
    updateHeading();
    updateCommands();
    m_loading = loading;
}

void MaterialListPanel::keepState() {
    State& state = m_states[m_list];
    state.kept = true;
    state.search = m_search->text();
    state.linkedWith = m_linkedWith->currentData().toString();
    state.withoutLinks = m_withoutLinks->isChecked();
    state.columnFilters = m_header->fieldsShown();
    state.current = currentMaterial();
    state.sortColumn = m_model->columnId(m_header->sortIndicatorSection());
    state.sortOrder = m_header->sortIndicatorOrder();
}

void MaterialListPanel::restoreState() {
    const bool loading = m_loading;
    m_loading = true;
    State& state = m_states[m_list];
    {
        const QSignalBlocker blockSearch(m_search);
        const QSignalBlocker blockLinked(m_linkedWith);
        const QSignalBlocker blockWithout(m_withoutLinks);
        m_search->setText(state.search);
        const int linked = m_linkedWith->findData(state.linkedWith);
        m_linkedWith->setCurrentIndex(std::max(0, linked));
        m_withoutLinks->setChecked(state.withoutLinks);
        for (auto it = state.columnFilter.cbegin(); it != state.columnFilter.cend(); ++it) {
            if (QLineEdit* field = m_header->fieldOf(it.key())) {
                const QSignalBlocker blockField(field);
                field->setText(it.value());
            }
        }
    }
    {
        const QSignalBlocker blocker(m_columnFiltersButton);
        m_columnFiltersButton->setChecked(state.columnFilters);
    }
    m_header->setFieldsShown(state.columnFilters);
    const QString sortColumn =
        state.sortColumn.isEmpty() ? MaterialList::firstSortOf(m_list) : state.sortColumn;
    const int column = columnOf(sortColumn);
    m_table->sortByColumn(column >= 0 ? column : std::max(0, m_model->titleColumn()),
                          state.kept ? state.sortOrder : Qt::AscendingOrder);
    applyFilters();
    if (!state.current.isEmpty()) {
        selectMaterial(state.current);
    }
    m_loading = loading;
}

void MaterialListPanel::applyColumns() {
    const QStringList shown = MaterialList::shownColumns(m_list);
    for (int column = 0; column < m_columns.size(); ++column) {
        m_header->setSectionHidden(column, !shown.contains(m_columns.at(column).id));
    }
    for (int place = 0; place < shown.size(); ++place) {
        const int logical = columnOf(shown.at(place));
        const int visual = m_header->visualIndex(logical);
        if (logical >= 0 && visual != place) {
            m_header->moveSection(visual, place);
        }
    }
    m_header->placeFields();
}

void MaterialListPanel::applyFilters() {
    if (m_model == nullptr || m_filter == nullptr) {
        return;
    }
    State& state = m_states[m_list];
    // The filter of a hidden column does not hide rows
    QHash<QString, QString> columnFilter;
    if (m_header->fieldsShown()) {
        for (auto it = state.columnFilter.cbegin(); it != state.columnFilter.cend(); ++it) {
            const int column = columnOf(it.key());
            if (column >= 0 && !m_header->isSectionHidden(column)) {
                columnFilter.insert(it.key(), it.value());
            }
        }
    }
    const QString current = currentMaterial();
    const bool loading = m_loading;
    m_loading = true;
    m_filter->setFilters(m_search->text(), m_linkedWith->currentData().toString(),
                         m_withoutLinks->isChecked(), columnFilter);
    if (!current.isEmpty()) {
        selectMaterial(current);
    }
    m_loading = loading;
    const bool filtered = !m_search->text().isEmpty() || m_linkedWith->currentIndex() > 0 ||
                          m_withoutLinks->isChecked() || !columnFilter.isEmpty();
    m_clearButton->setVisible(filtered);
    updateFooter();
    updateCommands();
}

void MaterialListPanel::fillLinkedWith() {
    const QString chosen = m_linkedWith->currentData().toString();
    const QSignalBlocker blocker(m_linkedWith);
    m_linkedWith->clear();
    //: The filter "Linked with:" that lets every material through. In Polish: dowolnym
    //: elementem
    m_linkedWith->addItem(MaterialList::tr("any element"), QString());
    // What the materials of the list are linked with, by title
    QStringList targets;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        for (const QString& link : m_model->rowAt(row)->links) {
            if (!targets.contains(link) && !linkTitle(link).isEmpty()) {
                targets << link;
            }
        }
    }
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(targets.begin(), targets.end(), [&collator](const QString& a, const QString& b) {
        return collator.compare(linkTitle(a), linkTitle(b)) < 0;
    });
    for (const QString& target : targets) {
        m_linkedWith->addItem(linkIcon(target, core::IconContext::ComboBox), linkTitle(target),
                              target);
    }
    m_linkedWith->setCurrentIndex(std::max(0, m_linkedWith->findData(chosen)));
}

void MaterialListPanel::updateCommands() {
    const auto& pm = core::ProjectManager::getInstance();
    const MaterialRow* row = m_model->rowAt(m_filter->mapToSource(view()->currentIndex()).row());
    const bool changing = pm.isProjectOpen() && !pm.isReadOnly();
    bool canAdd = false;
    if (m_list.isEmpty()) {
        for (const QString& kind : MaterialList::kinds()) {
            const std::optional<core::KindReference> reference = core::KindReference::parse(kind);
            canAdd = canAdd ||
                     (reference && core::ProjectManager::canAdd(pm.bookTypes().findKind(
                                       reference->packageId, reference->kindId)));
        }
    } else if (const std::optional<core::KindReference> reference =
                   core::KindReference::parse(m_list)) {
        canAdd = core::ProjectManager::canAdd(
            pm.bookTypes().findKind(reference->packageId, reference->kindId));
    }
    m_addButton->setEnabled(changing && canAdd);
    const QKeyCombination insert(Qt::NoModifier, Qt::Key_Insert);
    if (m_list.isEmpty()) {
        //: Adds a material in the Add Element window. In Polish: Dodaj...
        m_addButton->setText(MaterialList::tr("Add..."));
    } else {
        const std::optional<core::KindReference> reference = core::KindReference::parse(m_list);
        const core::KindRef kind =
            reference ? pm.bookTypes().findKind(reference->packageId, reference->kindId)
                      : core::KindRef{};
        //: Adds a material of the kind of the list: "Add Quote...". In Polish: Dodaj
        //: {kind:accusative}...
        m_addButton->setText(kind ? core::fillWords(MaterialList::tr("Add {Kind}..."),
                                                    {{QStringLiteral("kind"), wordsOf(kind)}})
                                  : MaterialList::tr("Add..."));
    }
    m_addButton->setToolTip(QStringLiteral("%1 (%2)").arg(m_addButton->text(),
                                                          ShortcutRules::keysText(insert)));
    m_editButton->setEnabled(row != nullptr);
    m_copyButton->setEnabled(row != nullptr && !row->copy.isEmpty());
    m_linkButton->setEnabled(row != nullptr && !row->text && changing);
    m_deleteButton->setEnabled(row != nullptr && changing);
}

void MaterialListPanel::updateHeading() {
    //: The name of a list of materials and their number. In Polish: <b>%1</b>&nbsp;&nbsp;%2
    m_heading->setText(MaterialList::tr("<b>%1</b>&nbsp;&nbsp;%2")
                           .arg(MaterialList::nameOf(m_list).toHtmlEscaped(),
                                QLocale().toString(m_model->rowCount())));
}

void MaterialListPanel::updateFooter() {
    //: How many materials of a list are shown: %1 shown, %2 all. In Polish: Na liście: %1 z %2
    m_footer->setText(MaterialList::tr("Shown: %1 of %2")
                          .arg(QLocale().toString(m_filter->rowCount()),
                               QLocale().toString(m_model->rowCount())));
    if (view() == m_tiles) {
        m_sorting->setText(MaterialList::tr("Tiles"));
        return;
    }
    const int section = m_header->sortIndicatorSection();
    const QString name = m_model->headerData(section, Qt::Horizontal, Qt::DisplayRole).toString();
    if (name.isEmpty()) {
        m_sorting->clear();
        return;
    }
    if (m_header->sortIndicatorOrder() == Qt::AscendingOrder) {
        //: How a list of materials is sorted; %1: the column. In Polish: Sortowanie: %1,
        //: rosnąco
        m_sorting->setText(MaterialList::tr("Sorted by %1, ascending").arg(name));
    } else {
        //: How a list of materials is sorted; %1: the column. In Polish: Sortowanie: %1,
        //: malejąco
        m_sorting->setText(MaterialList::tr("Sorted by %1, descending").arg(name));
    }
}

void MaterialListPanel::showMenuAt(QAbstractItemView* view, const QPoint& pos) {
    QMenu menu(this);
    const QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) {
        view->selectionModel()->clearCurrentIndex();
        view->clearSelection();
    }
    fillMenu(menu);
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void MaterialListPanel::showColumnsMenu(const QPoint& globalPos) {
    QMenu menu(this);
    const QStringList shown = shownColumns();
    for (const MaterialColumn& column : m_columns) {
        if (!column.sortable) {
            continue;
        }
        QAction* action = menu.addAction(column.title);
        action->setCheckable(true);
        action->setChecked(shown.contains(column.id));
        // One column stays
        action->setEnabled(!(action->isChecked() && shown.size() <= 1));
        const QString id = column.id;
        connect(action, &QAction::toggled, this, [this, id](bool on) { showColumn(id, on); });
    }
    menu.addSeparator();
    //: A note at the end of the menu of the columns. In Polish: Aby zmienić kolejność,
    //: przeciągnij nagłówek kolumny.
    QAction* note = menu.addAction(MaterialList::tr("To change the order, drag the header of a "
                                                    "column."));
    note->setEnabled(false);
    menu.exec(globalPos);
}

void MaterialListPanel::keepColumns() {
    MaterialList::setShownColumns(m_list, shownColumns());
}

void MaterialListPanel::onCurrentChanged(QAbstractItemView* view, const QModelIndex& current) {
    // The other view has the same material
    QAbstractItemView* other = view == m_table ? static_cast<QAbstractItemView*>(m_tiles)
                                               : static_cast<QAbstractItemView*>(m_table);
    if (current.isValid() && other->currentIndex().row() != current.row()) {
        const QModelIndex target =
            current.siblingAtColumn(other == m_tiles ? m_tiles->modelColumn() : current.column());
        const QSignalBlocker blocker(other->selectionModel());
        other->selectionModel()->setCurrentIndex(
            target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        other->viewport()->update();
    }
    updateCommands();
    if (!m_loading && current.isValid()) {
        m_states[m_list].current = idAt(current);
        emit materialChosen(idAt(current));
    }
}

QString MaterialListPanel::idAt(const QModelIndex& index) const {
    return index.isValid() ? index.data(ID_ROLE).toString() : QString();
}

int MaterialListPanel::columnOf(const QString& columnId) const {
    for (int column = 0; column < m_columns.size(); ++column) {
        if (m_columns.at(column).id == columnId) {
            return column;
        }
    }
    return -1;
}

} // namespace gui
} // namespace kalahari
