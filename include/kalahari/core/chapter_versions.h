/// @file chapter_versions.h
/// @brief Versions of the text of a chapter file: copies of its text that the writer keeps, or
/// that the program keeps by itself, to read, compare and bring back later
///
/// The versions of a file of the book are in the book's folder, under versions/ and the path of
/// the file without ".kchapter": those of book/chapter_002.kchapter are in
/// versions/book/chapter_002/. The folder has index.json, which lists them, and one .kml file
/// with the text of each version, named after the time it was kept (the local time of the
/// computer), and in index.json its words (WordTally: "dashes" are the dashes standing alone):
/// @code{.json}
/// {
///   "format": 1,
///   "file": "book/chapter_002.kchapter",
///   "versions": [
///     { "file": "2026-10-21_145210.kml", "created": "2026-10-21T12:52:10.204Z",
///       "origin": "manual", "name": "Before the dialogue was cut", "words": 2486,
///       "dashes": 31 },
///     { "file": "2026-10-21_090512.kml", "created": "2026-10-21T07:05:12.918Z",
///       "origin": "replaceAll", "find": "Taun", "replace": "Tau", "replaced": 12,
///       "words": 2455, "dashes": 30 }
///   ]
/// }
/// @endcode
/// The versions belong to the file, not to its element: a file moved in the book, or taken out
/// of the book and added again, keeps them. The files do not change their names when their
/// elements are moved or renamed (BookProject).

#pragma once

#include <kalahari/core/text_statistics.h>

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace kalahari {
namespace core {

/// @brief Why a version was kept
enum class VersionOrigin {
    Manual,      ///< The writer kept it (Save Version)
    Save,        ///< The program kept it when the chapter was saved
    ReplaceAll,  ///< The program kept it before Replace All changed the text
    Restore      ///< The program kept it before another version was brought back
};

/// @brief The name of @p origin in index.json: manual, save, replaceAll, restore
QString versionOriginName(VersionOrigin origin);

/// @brief The origin named @p name in index.json; nullopt for a name the program does not know
std::optional<VersionOrigin> versionOriginNamed(const QString& name);

/// @brief One version of the text of a file, as index.json lists it
struct ChapterVersion {
    QString file = {};       ///< Its file in the folder of the versions: 2026-10-21_145210.kml
    QDateTime created = {};  ///< When it was kept (UTC)
    VersionOrigin origin = VersionOrigin::Manual;  ///< Why it was kept
    QString name = {};         ///< What the writer wrote about it; empty: nothing
    QString found = {};        ///< ReplaceAll: the text that was replaced
    QString replacement = {};  ///< ReplaceAll: the text put in its place
    int replaced = 0;          ///< ReplaceAll: how many times
    QDateTime restored = {};   ///< Restore: when the version brought back was kept (UTC)
    WordTally words = {};      ///< Words of its text, under any rules of counting (tallyWords())

    /// @brief Whether the program kept it by itself, so that it goes when there are too many
    bool isAutomatic() const { return origin != VersionOrigin::Manual; }

    bool operator==(const ChapterVersion& other) const = default;
};

/// @brief The versions of one file of a book
///
/// Each change reads index.json, changes it and writes it again in one step (QSaveFile), so a
/// failed write (a full disk, a crash) leaves the versions as they were. A .kml file that
/// index.json does not list is not a version; a listed one whose file is gone is left out.
class ChapterVersions {
public:
    /// @brief Folder of the versions of all files, in the folder of the book
    static constexpr const char* FOLDER = "versions";
    /// @brief The list of the versions of a file, in the folder of its versions
    static constexpr const char* INDEX_FILE = "index.json";
    /// @brief Format of index.json
    static constexpr int FORMAT = 1;

    /// @brief Setting: whether Save Version asks for a description
    static constexpr const char* ASK_NAME_SETTING = "editor.versions.askName";
    /// @brief Setting: whether saving a chapter keeps its text as a version
    static constexpr const char* ON_SAVE_SETTING = "editor.versions.onSave";
    /// @brief Setting: minutes between the versions kept on saving
    static constexpr const char* ON_SAVE_MINUTES_SETTING = "editor.versions.onSaveMinutes";
    /// @brief Setting: whether Replace All in a chapter keeps its text as a version first
    static constexpr const char* BEFORE_REPLACE_ALL_SETTING = "editor.versions.beforeReplaceAll";
    /// @brief Setting: how many versions that the program kept by itself each file keeps
    static constexpr const char* KEEP_AUTOMATIC_SETTING = "editor.versions.keepAutomatic";

    /// @brief Folder of the versions of file @p file of the book in folder @p projectDir
    /// @param file The file, relative to the folder of the book, with "/"
    ///        (ProjectElement::file)
    /// @return Its absolute path, e.g. <projectDir>/versions/book/chapter_002; empty when
    ///         @p file is not a file inside the folder of the book (an absolute path, a drive,
    ///         "..")
    static QString folderOf(const QString& projectDir, const QString& file);

    /// @brief The versions of file @p file of the book in folder @p projectDir
    /// @param file The file, relative to the folder of the book (ProjectElement::file)
    ChapterVersions(const QString& projectDir, const QString& file);

    /// @brief Whether the file can have versions: it is inside the folder of the book
    bool isValid() const { return !m_folder.isEmpty(); }

    /// @brief Absolute path of the folder of the versions; empty when !isValid()
    QString folder() const { return m_folder; }

    /// @brief The file, relative to the folder of the book, as given
    QString file() const { return m_file; }

    /// @brief The versions, newest first
    QList<ChapterVersion> list() const;

    /// @brief Number of the versions
    qsizetype count() const { return list().size(); }

    /// @brief The newest version; nullopt when there is none
    std::optional<ChapterVersion> newest() const;

    /// @brief The version kept in .kml file @p versionFile; nullopt when there is none
    std::optional<ChapterVersion> find(const QString& versionFile) const;

    /// @brief Text (KML) of the version kept in .kml file @p versionFile; nullopt when there is
    /// no such version or its file cannot be read
    std::optional<QString> text(const QString& versionFile) const;

    /// @brief Whether the newest version has text @p kml
    bool newestHas(const QString& kml) const;

    /// @brief When the newest version of @p origin was kept; nullopt when there is none
    std::optional<QDateTime> lastCreated(VersionOrigin origin) const;

    /// @brief Keep @p kml as a new version
    /// @param kml The text
    /// @param about Why it is kept and what is said about it: its origin, name, found,
    ///        replacement, replaced and restored; the others are left out
    /// @param now When it is kept; an invalid time: now
    /// @return The new version; nullopt when its file or index.json cannot be written (then
    ///         nothing changes)
    std::optional<ChapterVersion> add(const QString& kml, const ChapterVersion& about,
                                      const QDateTime& now = QDateTime());

    /// @brief Give the version kept in .kml file @p versionFile the writer's description
    /// @p name (empty: none)
    /// @return false when there is no such version or index.json cannot be written
    bool rename(const QString& versionFile, const QString& name);

    /// @brief Delete the version kept in .kml file @p versionFile
    /// @return false when there is no such version or index.json cannot be written; a .kml
    ///         file that cannot be deleted stays, but the version is gone from the list
    bool remove(const QString& versionFile);

    /// @brief Delete the oldest versions that the program kept by itself, so that at most
    /// @p keep of them stay; the writer's own versions stay
    /// @return The number of the versions deleted; -1 when index.json cannot be written
    int removeOldAutomatic(int keep);

private:
    /// @brief index.json as the disk has it
    struct Index {
        int format = FORMAT;             ///< Its format: a newer one is not written over
        QList<ChapterVersion> versions;  ///< Newest first, without those whose file is gone
    };

    /// @brief Read index.json; no file: no versions
    Index read() const;

    /// @brief Write index.json with the versions of @p index in one step; no versions: remove
    /// index.json
    /// @return false when it cannot be written, or when it is of a newer format (a newer
    ///         version of the program wrote it, and this one would lose what it added)
    bool write(const Index& index) const;

    /// @brief Remove the folder of the versions when it is empty, and the empty folders above
    /// it, versions/ too
    void removeEmptyFolders() const;

    /// @brief Name of a new .kml file for a version kept at @p created: its local time, with
    /// _2, _3… when the folder or @p listed has that name already
    QString newVersionFile(const QDateTime& created, const QList<ChapterVersion>& listed) const;

    QString m_file;    ///< The file, relative to the folder of the book
    QString m_folder;  ///< Absolute path of the folder of its versions
    QString m_root;    ///< Absolute path of versions/ in the folder of the book
};

}  // namespace core
}  // namespace kalahari
