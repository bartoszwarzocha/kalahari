/// @file tag_detector.h
/// @brief Tag detection service for TODO/FIX/CHECK markers (OpenSpec #00042 Task 7.10)
///
/// TagDetector scans KML documents for special tags like TODO, FIX, CHECK, NOTE, WARNING.
/// It provides:
/// - Real-time tag detection as document changes
/// - Color coding for different tag types
/// - Position tracking for navigation
///
/// The service uses the document observer pattern to stay synchronized with content changes.

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QColor>
#include <QRegularExpression>

// Forward declarations
namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::editor {

/// @brief Types of tags that can be detected
enum class TagType {
    Todo,       ///< TODO: something to do
    Fix,        ///< FIX: bug to fix
    Check,      ///< CHECK: verify this
    Note,       ///< NOTE: informational
    Warning     ///< WARNING: important notice
};

/// @brief Information about a detected tag in the document
///
/// Contains all the information needed to display and navigate to a tag.
struct DetectedTag {
    TagType type;           ///< Type of the tag (TODO, FIX, etc.)
    int paragraphIndex;     ///< Paragraph containing the tag
    int startPos;           ///< Position in paragraph text where tag starts
    int length;             ///< Length of the tag keyword (e.g., "TODO" = 4)
    QString keyword;        ///< The tag keyword ("TODO", "FIX", etc.)
    QString content;        ///< Text after the tag (the description)
    int lineNumber;         ///< Line number for display purposes (1-based)

    /// @brief Default constructor
    DetectedTag() = default;

    /// @brief Construct with all fields
    DetectedTag(TagType t, int paraIdx, int start, int len,
                const QString& kw, const QString& cont, int line)
        : type(t)
        , paragraphIndex(paraIdx)
        , startPos(start)
        , length(len)
        , keyword(kw)
        , content(cont)
        , lineNumber(line)
    {}

    /// @brief Equality comparison
    bool operator==(const DetectedTag& other) const {
        return type == other.type &&
               paragraphIndex == other.paragraphIndex &&
               startPos == other.startPos &&
               length == other.length;
    }
};

/// @brief Tag detection service for scanning KML documents
///
/// Scans documents for special marker tags (TODO, FIX, CHECK, NOTE, WARNING)
/// and provides signals when tags are found or changed.
///
/// Usage:
/// @code
/// auto detector = new TagDetector(this);
/// detector->setDocument(document);
/// detector->scan();
///
/// // Get all TODO tags
/// QList<DetectedTag> todos = detector->tagsOfType(TagType::Todo);
///
/// // Navigate to a tag
/// connect(detector, &TagDetector::tagsChanged,
///         this, &MyEditor::onTagsUpdated);
/// @endcode
class TagDetector : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a tag detector
    /// @param parent Parent QObject for ownership
    explicit TagDetector(QObject* parent = nullptr);

    /// @brief Destructor - cleans up document observer
    ~TagDetector() override;

    // Non-copyable
    TagDetector(const TagDetector&) = delete;
    TagDetector& operator=(const TagDetector&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the BookEditor to scan
    /// @param editor BookEditor to monitor (nullptr to disconnect)
    /// @note Previous editor is automatically disconnected
    void setBookEditor(BookEditor* editor);

    /// @brief Get the currently set editor
    /// @return Current editor, or nullptr if none set
    BookEditor* bookEditor() const { return m_editor; }

    // =========================================================================
    // Scanning
    // =========================================================================

    /// @brief Scan entire document for tags
    ///
    /// Clears previous results and scans all paragraphs.
    /// Emits tagsChanged() when complete.
    void scan();

    /// @brief Scan a single paragraph for tags
    /// @param index Paragraph index to scan
    ///
    /// Updates tags for the specified paragraph only.
    /// Emits tagsChanged() if tags were added or removed.
    void scanParagraph(int index);

    /// @brief Clear all detected tags
    void clear();

    // =========================================================================
    // Tag Queries
    // =========================================================================

    /// @brief Get all detected tags
    /// @return List of all detected tags (sorted by paragraph, then position)
    QList<DetectedTag> allTags() const;

    /// @brief Get tags of a specific type
    /// @param type The tag type to filter by
    /// @return List of tags matching the type
    QList<DetectedTag> tagsOfType(TagType type) const;

    /// @brief Get tags in a specific paragraph
    /// @param index The paragraph index
    /// @return List of tags in that paragraph
    QList<DetectedTag> tagsInParagraph(int index) const;

    /// @brief Get count of all tags
    /// @return Total number of detected tags
    int tagCount() const { return m_tags.size(); }

    /// @brief Get count of tags of a specific type
    /// @param type The tag type to count
    /// @return Number of tags of that type
    int tagCount(TagType type) const;

    // =========================================================================
    // Appearance
    // =========================================================================

    /// @brief Get the display color for a tag type
    /// @param type The tag type
    /// @return Color to use for highlighting/display
    static QColor colorForType(TagType type);

    /// @brief Get the display name for a tag type
    /// @param type The tag type
    /// @return Human-readable name (e.g., "TODO", "FIX")
    static QString nameForType(TagType type);

    /// @brief Get tag type from keyword string
    /// @param keyword The keyword (e.g., "TODO", "FIX")
    /// @return The corresponding TagType, or TagType::Note if unknown
    static TagType typeFromKeyword(const QString& keyword);

signals:
    /// @brief Emitted when tags list has changed
    ///
    /// Connect to this signal to update UI when tags are added/removed.
    void tagsChanged();

    /// @brief Emitted when a new tag is found during scanning
    /// @param tag The detected tag
    void tagFound(const DetectedTag& tag);

private slots:
    /// @brief Handle paragraph modified signal from BookEditor
    /// @param paragraphIndex Index of the modified paragraph
    void onParagraphModified(int paragraphIndex);

    /// @brief Handle paragraph inserted signal from BookEditor
    /// @param paragraphIndex Index of the newly inserted paragraph
    void onParagraphInserted(int paragraphIndex);

    /// @brief Handle paragraph removed signal from BookEditor
    /// @param paragraphIndex Index of the removed paragraph
    void onParagraphRemoved(int paragraphIndex);

private:

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// @brief Detect tags in a text string
    /// @param text The text to scan
    /// @param paragraphIndex The paragraph index for position tracking
    void detectTagsInText(const QString& text, int paragraphIndex);

    /// @brief Remove tags for a specific paragraph
    /// @param paragraphIndex The paragraph to remove tags from
    /// @return Number of tags removed
    int removeTagsForParagraph(int paragraphIndex);

    /// @brief Calculate line number for a paragraph
    /// @param paragraphIndex The paragraph index
    /// @return 1-based line number
    int calculateLineNumber(int paragraphIndex) const;

    // =========================================================================
    // Members
    // =========================================================================

    BookEditor* m_editor{nullptr};
    QList<DetectedTag> m_tags;

    /// @brief Regex pattern for matching tags
    ///
    /// Matches: TODO:, FIX:, CHECK:, NOTE:, WARNING: (case insensitive)
    /// Captures: (keyword) and (content after)
    static const QRegularExpression s_tagPattern;
};

}  // namespace kalahari::editor
