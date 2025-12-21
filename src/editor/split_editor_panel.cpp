/// @file split_editor_panel.cpp
/// @brief SplitEditorPanel implementation (OpenSpec #00042)

#include <kalahari/editor/split_editor_panel.h>
#include <kalahari/core/art_provider.h>
#include <QDataStream>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace kalahari::editor {

// =============================================================================
// Constants
// =============================================================================

/// @brief Border width for active editor indicator
constexpr int ACTIVE_BORDER_WIDTH = 2;

/// @brief State format version for save/restore
constexpr quint32 STATE_VERSION = 1;

// =============================================================================
// Construction / Destruction
// =============================================================================

SplitEditorPanel::SplitEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    setupLayout();

    // Create the primary editor
    m_primaryEditor = createEditor();
    m_layout->addWidget(m_primaryEditor);

    // Primary editor is initially active
    m_activeEditor = m_primaryEditor;
    updateActiveIndicators();
}

SplitEditorPanel::~SplitEditorPanel()
{
    // Qt handles child widget cleanup
}

// =============================================================================
// Document Management
// =============================================================================

void SplitEditorPanel::setDocument(KmlDocument* document)
{
    m_document = document;

    // Pass document to all editors
    if (m_primaryEditor != nullptr) {
        m_primaryEditor->setDocument(document);
    }
    if (m_secondaryEditor != nullptr) {
        m_secondaryEditor->setDocument(document);
    }
}

KmlDocument* SplitEditorPanel::document() const
{
    return m_document;
}

// =============================================================================
// Split Operations
// =============================================================================

SplitOrientation SplitEditorPanel::splitOrientation() const
{
    return m_orientation;
}

bool SplitEditorPanel::isSplit() const
{
    return m_orientation != SplitOrientation::None;
}

bool SplitEditorPanel::splitHorizontal()
{
    return createSplit(SplitOrientation::Horizontal);
}

bool SplitEditorPanel::splitVertical()
{
    return createSplit(SplitOrientation::Vertical);
}

bool SplitEditorPanel::closeSplit()
{
    if (!isSplit()) {
        return false;
    }

    // Determine which editor to keep (the active one, preferring primary)
    if (m_activeEditor == m_secondaryEditor) {
        // Keep primary, remove secondary
        return closeSplit(1);
    } else {
        // Default: remove secondary
        return closeSplit(1);
    }
}

bool SplitEditorPanel::closeSplit(int index)
{
    if (!isSplit()) {
        return false;
    }

    // Only secondary editor (index 1) can be closed
    if (index != 1) {
        return false;
    }

    // If secondary was active, switch to primary
    if (m_activeEditor == m_secondaryEditor) {
        m_activeEditor = m_primaryEditor;
    }

    // Remove editors from splitter
    m_splitter->widget(0)->setParent(nullptr);
    if (m_splitter->count() > 0) {
        m_splitter->widget(0)->setParent(nullptr);
    }

    // Delete splitter and secondary editor
    delete m_splitter;
    m_splitter = nullptr;

    delete m_secondaryEditor;
    m_secondaryEditor = nullptr;

    // Re-add primary editor to main layout
    m_layout->addWidget(m_primaryEditor);

    // Update state
    m_orientation = SplitOrientation::None;
    updateActiveIndicators();

    emit splitChanged(m_orientation);
    emit activeEditorChanged(m_activeEditor);

    return true;
}

// =============================================================================
// Editor Access
// =============================================================================

BookEditor* SplitEditorPanel::activeEditor() const
{
    return m_activeEditor;
}

BookEditor* SplitEditorPanel::editor(int index) const
{
    if (index == 0) {
        return m_primaryEditor;
    } else if (index == 1 && isSplit()) {
        return m_secondaryEditor;
    }
    return nullptr;
}

int SplitEditorPanel::editorCount() const
{
    return isSplit() ? 2 : 1;
}

void SplitEditorPanel::setActiveEditor(int index)
{
    BookEditor* newActive = editor(index);
    if (newActive != nullptr && newActive != m_activeEditor) {
        m_activeEditor = newActive;
        m_activeEditor->setFocus();
        updateActiveIndicators();
        emit activeEditorChanged(m_activeEditor);
    }
}

// =============================================================================
// Appearance
// =============================================================================

void SplitEditorPanel::setAppearance(const EditorAppearance& appearance)
{
    m_appearance = appearance;

    if (m_primaryEditor != nullptr) {
        m_primaryEditor->setAppearance(appearance);
    }
    if (m_secondaryEditor != nullptr) {
        m_secondaryEditor->setAppearance(appearance);
    }
}

EditorAppearance SplitEditorPanel::appearance() const
{
    return m_appearance;
}

// =============================================================================
// View Mode
// =============================================================================

void SplitEditorPanel::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) {
        return;
    }

    m_viewMode = mode;

    if (m_primaryEditor != nullptr) {
        m_primaryEditor->setViewMode(mode);
    }
    if (m_secondaryEditor != nullptr) {
        m_secondaryEditor->setViewMode(mode);
    }

    emit viewModeChanged(mode);
}

ViewMode SplitEditorPanel::viewMode() const
{
    return m_viewMode;
}

// =============================================================================
// State Persistence
// =============================================================================

QByteArray SplitEditorPanel::saveState() const
{
    QByteArray stateData;
    QDataStream stream(&stateData, QIODevice::WriteOnly);

    // Write version
    stream << STATE_VERSION;

    // Write orientation
    stream << static_cast<qint32>(m_orientation);

    // Write splitter state if split
    if (isSplit() && m_splitter != nullptr) {
        stream << m_splitter->saveState();
    } else {
        stream << QByteArray();
    }

    // Write active editor index
    int activeIndex = (m_activeEditor == m_secondaryEditor) ? 1 : 0;
    stream << static_cast<qint32>(activeIndex);

    return stateData;
}

bool SplitEditorPanel::restoreState(const QByteArray& state)
{
    if (state.isEmpty()) {
        return false;
    }

    QDataStream stream(state);

    // Check version
    quint32 version;
    stream >> version;
    if (version != STATE_VERSION) {
        return false;
    }

    // Read orientation
    qint32 orientationValue;
    stream >> orientationValue;
    auto orientation = static_cast<SplitOrientation>(orientationValue);

    // Read splitter state
    QByteArray splitterState;
    stream >> splitterState;

    // Read active editor index
    qint32 activeIndex;
    stream >> activeIndex;

    // Apply state
    if (orientation != SplitOrientation::None) {
        if (!isSplit()) {
            createSplit(orientation);
        }
        if (m_splitter != nullptr && !splitterState.isEmpty()) {
            m_splitter->restoreState(splitterState);
        }
    } else if (isSplit()) {
        closeSplit();
    }

    // Restore active editor
    setActiveEditor(activeIndex);

    return true;
}

// =============================================================================
// Event Handlers
// =============================================================================

void SplitEditorPanel::keyPressEvent(QKeyEvent* event)
{
    // Handle split shortcuts
    if (event->modifiers() == Qt::ControlModifier) {
        // Ctrl+\ - Split horizontal
        if (event->key() == Qt::Key_Backslash) {
            if (!isSplit()) {
                splitHorizontal();
                event->accept();
                return;
            }
        }
        // Ctrl+W - Close split
        else if (event->key() == Qt::Key_W) {
            if (isSplit()) {
                closeSplit();
                event->accept();
                return;
            }
        }
    } else if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        // Ctrl+Shift+\ - Split vertical
        if (event->key() == Qt::Key_Backslash) {
            if (!isSplit()) {
                splitVertical();
                event->accept();
                return;
            }
        }
    }

    QWidget::keyPressEvent(event);
}

bool SplitEditorPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Track focus changes to update active editor
    if (event->type() == QEvent::FocusIn) {
        if (watched == m_primaryEditor && m_activeEditor != m_primaryEditor) {
            m_activeEditor = m_primaryEditor;
            updateActiveIndicators();
            emit activeEditorChanged(m_activeEditor);
        } else if (watched == m_secondaryEditor && m_activeEditor != m_secondaryEditor) {
            m_activeEditor = m_secondaryEditor;
            updateActiveIndicators();
            emit activeEditorChanged(m_activeEditor);
        }
    }

    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// Private Methods
// =============================================================================

BookEditor* SplitEditorPanel::createEditor()
{
    auto* editor = new BookEditor(this);

    // Apply current document
    if (m_document != nullptr) {
        editor->setDocument(m_document);
    }

    // Apply appearance and view mode
    editor->setAppearance(m_appearance);
    editor->setViewMode(m_viewMode);

    // Setup connections
    setupEditorConnections(editor);

    // Install event filter for focus tracking
    editor->installEventFilter(this);

    return editor;
}

void SplitEditorPanel::setupEditorConnections(BookEditor* editor)
{
    // Forward cursor position changes from active editor
    connect(editor, &BookEditor::cursorPositionChanged, this,
            [this, editor](const CursorPosition& position) {
                if (editor == m_activeEditor) {
                    emit cursorPositionChanged(position);
                }
            });

    // Forward selection changes from active editor
    connect(editor, &BookEditor::selectionChanged, this,
            [this, editor]() {
                if (editor == m_activeEditor) {
                    emit selectionChanged();
                }
            });
}

void SplitEditorPanel::updateActiveIndicators()
{
    // Get primary color from ArtProvider
    QColor activeColor = kalahari::core::ArtProvider::getInstance().getPrimaryColor();

    // Build stylesheet for active border
    QString activeBorder = QString("border: %1px solid %2;")
                               .arg(ACTIVE_BORDER_WIDTH)
                               .arg(activeColor.name());
    QString inactiveBorder = "border: none;";

    // Apply to editors
    if (m_primaryEditor != nullptr) {
        if (m_primaryEditor == m_activeEditor) {
            m_primaryEditor->setStyleSheet(activeBorder);
            // Enable cursor blinking only for active editor
            m_primaryEditor->setCursorBlinkingEnabled(true);
        } else {
            m_primaryEditor->setStyleSheet(inactiveBorder);
            m_primaryEditor->setCursorBlinkingEnabled(false);
        }
    }

    if (m_secondaryEditor != nullptr) {
        if (m_secondaryEditor == m_activeEditor) {
            m_secondaryEditor->setStyleSheet(activeBorder);
            m_secondaryEditor->setCursorBlinkingEnabled(true);
        } else {
            m_secondaryEditor->setStyleSheet(inactiveBorder);
            m_secondaryEditor->setCursorBlinkingEnabled(false);
        }
    }
}

bool SplitEditorPanel::createSplit(SplitOrientation orientation)
{
    if (isSplit()) {
        return false;
    }

    // Create splitter
    m_splitter = new QSplitter(this);
    m_splitter->setOrientation(
        orientation == SplitOrientation::Horizontal
            ? Qt::Horizontal
            : Qt::Vertical);

    // Remove primary editor from layout
    m_layout->removeWidget(m_primaryEditor);

    // Create secondary editor
    m_secondaryEditor = createEditor();

    // Add both editors to splitter
    m_splitter->addWidget(m_primaryEditor);
    m_splitter->addWidget(m_secondaryEditor);

    // Set equal sizes
    QList<int> sizes;
    if (orientation == SplitOrientation::Horizontal) {
        int halfWidth = width() / 2;
        sizes << halfWidth << halfWidth;
    } else {
        int halfHeight = height() / 2;
        sizes << halfHeight << halfHeight;
    }
    m_splitter->setSizes(sizes);

    // Add splitter to layout
    m_layout->addWidget(m_splitter);

    // Update state
    m_orientation = orientation;
    updateActiveIndicators();

    emit splitChanged(m_orientation);

    return true;
}

void SplitEditorPanel::setupLayout()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
}

}  // namespace kalahari::editor
