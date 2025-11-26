#!/bin/bash
# Download ALL Material Design icons for Kalahari
# Task #00023: Complete icon set for all commands
#
# Usage: ./scripts/download_all_icons.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ICONS_DIR="$PROJECT_DIR/resources/icons"

# Create directories for all three styles
mkdir -p "$ICONS_DIR/twotone"
mkdir -p "$ICONS_DIR/outlined"
mkdir -p "$ICONS_DIR/rounded"

# Complete list of Material Design icon names needed for all commands
# Mapped from Kalahari action IDs to Material Design names
ICONS=(
    # FILE MENU
    "note_add"          # file.new
    "folder_open"       # file.open
    "close"             # file.close
    "save"              # file.save
    "save_as"           # file.saveAs
    "layers"            # file.saveAll (or content_save_all doesn't exist)
    "logout"            # file.exit / exit_to_app
    "upload_file"       # file.import.docx
    "picture_as_pdf"    # file.import.pdf, file.export.pdf
    "text_snippet"      # file.import.text
    "integration_instructions"  # file.import.scrivener
    "download"          # file.export.docx
    "code"              # file.export.markdown, format.style.code
    "auto_stories"      # file.export.epub, tools.readability
    "smartphone"        # file.export.mobi
    "functions"         # file.export.latex

    # EDIT MENU
    "undo"              # edit.undo
    "redo"              # edit.redo
    "content_cut"       # edit.cut
    "content_copy"      # edit.copy
    "content_paste"     # edit.paste
    "content_paste_go"  # edit.pasteSpecial
    "delete"            # edit.delete
    "select_all"        # edit.selectAll
    "highlight_alt"     # edit.selectWord
    "segment"           # edit.selectParagraph
    "search"            # edit.find, view.search
    "navigate_next"     # edit.findNext
    "navigate_before"   # edit.findPrevious
    "find_replace"      # edit.findReplace
    "settings"          # edit.preferences, edit.settings, assistant.settings

    # BOOK MENU
    "article"           # book.newChapter
    "theaters"          # book.newScene
    "person"            # book.newCharacter
    "person_add"        # book.newCharacter (alt)
    "place"             # book.newLocation
    "location_on"       # book.newLocation (alt)
    "add_box"           # book.newItem
    "account_tree"      # book.newMindMap
    "hub"               # book.newMindMap (alt)
    "timeline"          # book.newTimeline
    "horizontal_rule"   # book.chapterBreak
    "more_horiz"        # book.sceneBreak
    "tune"              # book.properties, view.properties

    # INSERT MENU
    "add_photo_alternate"  # insert.image
    "image"             # insert.image (alt)
    "table_chart"       # insert.table
    "grid_on"           # insert.table (alt)
    "link"              # insert.link
    "short_text"        # insert.footnote
    "notes"             # insert.endnote, format.paragraph
    "add_comment"       # insert.comment
    "comment"           # insert.comment (alt)
    "edit_note"         # insert.annotation
    "sticky_note_2"     # insert.annotation (alt)
    "emoji_symbols"     # insert.specialChar
    "schedule"          # insert.dateTime
    "event"             # insert.dateTime (alt)
    "data_object"       # insert.field
    "input"             # insert.field (alt)

    # FORMAT MENU
    "text_format"       # format.font, view.toolbars.format
    "font_download"     # format.font (alt)
    "looks_one"         # format.style.heading1
    "looks_two"         # format.style.heading2
    "looks_3"           # format.style.heading3
    "subject"           # format.style.body
    "format_quote"      # format.style.quote
    "style"             # format.style.manage
    "format_bold"       # format.bold
    "format_italic"     # format.italic
    "format_underlined" # format.underline
    "strikethrough_s"   # format.strikethrough
    "format_strikethrough"  # format.strikethrough (alt)
    "format_align_left"     # format.alignLeft
    "format_align_center"   # format.alignCenter
    "format_align_right"    # format.alignRight
    "format_align_justify"  # format.justify
    "format_indent_increase"  # format.increaseIndent
    "format_indent_decrease"  # format.decreaseIndent
    "format_list_bulleted"    # format.bullets
    "format_list_numbered"    # format.numbering
    "format_color_text"       # format.color
    "palette"           # format.color (alt)
    "format_clear"      # format.clearFormatting

    # TOOLS MENU
    "analytics"         # tools.stats.full
    "bar_chart"         # tools.stats.full (alt)
    "label"             # tools.wordCount
    "spellcheck"        # tools.spellcheck
    "grading"           # tools.grammar
    "fact_check"        # tools.grammar (alt), assistant.action.grammar
    "visibility"        # tools.focus.normal
    "center_focus_strong"   # tools.focus.focused
    "fullscreen"        # tools.focus.distractionFree, view.fullScreen
    "fullscreen_exit"   # exit fullscreen
    "backup"            # tools.backupNow
    "sync"              # tools.autoSaveSettings
    "auto_fix_normal"   # tools.autoSaveSettings (alt)
    "history"           # tools.versionHistory
    "extension"         # tools.plugins.manager
    "storefront"        # tools.plugins.marketplace
    "shopping_bag"      # tools.plugins.marketplace (alt)
    "system_update"     # tools.plugins.updates
    "refresh"           # tools.plugins.reload
    "emoji_events"      # tools.challenges
    "military_tech"     # tools.challenges (alt)
    "track_changes"     # tools.writingGoals
    "trending_up"       # tools.writingGoals (alt)
    "cloud_sync"        # tools.cloudSync
    "cloud"             # tools.cloudSync (alt)
    "groups"            # tools.collaboration
    "people"            # tools.collaboration (alt)

    # ASSISTANT MENU
    "pets"              # view.assistant, assistant (animal mascot)
    "psychology"        # assistant.ask
    "swap_horiz"        # assistant.switch
    "swap_horizontal_circle"  # assistant.switch (alt)
    "brush"             # assistant.action.style
    "auto_graph"        # assistant.action.plot
    "science"           # assistant.action.research, view.perspectives.researcher
    "flash_on"          # assistant.action.speedDraft
    "speed"             # assistant.action.speedDraft (alt)

    # VIEW MENU
    "import_contacts"   # view.navigator
    "menu_book"         # view.navigator (alt), view.toolbars.book
    "book"              # view.toolbars.book (alt)
    "description"       # view.log
    "terminal"          # view.log (alt)
    "edit"              # view.perspectives.writer
    "draw"              # view.perspectives.writer (alt)
    "rate_review"       # view.perspectives.editor
    "edit_document"     # view.perspectives.editor (alt)
    "biotech"           # view.perspectives.researcher (alt)
    "event_note"        # view.perspectives.planner
    "calendar_month"    # view.perspectives.planner (alt)
    "dashboard"         # view.perspectives.manage, view.resetLayout
    "view_module"       # view.perspectives.manage (alt)
    "view_headline"     # view.toolbars.standard
    "toolbar"           # view.toolbars.standard (doesn't exist, use view_headline)
    "format_shapes"     # view.toolbars.format (alt)
    "bolt"              # view.toolbars.quickAccess
    "star"              # view.toolbars.quickAccess (alt)
    "build"             # view.toolbars.customize
    "horizontal_split"  # view.showStatusBar
    "view_sidebar"      # view.showStatusBar (alt)
    "insert_chart"      # view.showStatsBar
    "paragraph"         # view.showFormattingMarks
    "zoom_in"           # view.zoomIn
    "zoom_out"          # view.zoomOut
    "fit_screen"        # view.resetZoom
    "zoom_out_map"      # view.resetZoom (alt)
    "view_quilt"        # view.resetLayout (alt)

    # HELP MENU
    "help"              # help.help
    "help_outline"      # help.help (alt)
    "info"              # help.about
    "school"            # help.tutorial
    "play_circle"       # help.tutorial (alt)
    "keyboard"          # help.shortcuts
    "keyboard_alt"      # help.shortcuts (alt)
    "tips_and_updates"  # help.tipsTricks
    "lightbulb"         # help.tipsTricks (alt)
    "new_releases"      # help.whatsNew
    "fiber_new"         # help.whatsNew (alt)
    "bug_report"        # help.reportBug
    "feedback"          # help.suggestFeature
    "forum"             # help.communityForum
    "update"            # help.checkUpdates

    # ADDITIONAL COMMON ICONS (for future use)
    "check"
    "check_circle"
    "cancel"
    "error"
    "warning"
    "notifications"
    "home"
    "menu"
    "more_vert"
    "expand_more"
    "expand_less"
    "chevron_right"
    "chevron_left"
    "arrow_back"
    "arrow_forward"
    "arrow_upward"
    "arrow_downward"
    "first_page"
    "last_page"
    "sort"
    "filter_list"
    "lock"
    "lock_open"
    "print"
    "share"
    "attach_file"
    "file_present"
    "folder"
    "create_new_folder"
    "drive_file_move"
    "file_copy"
)

# FOUR styles to download (user can choose in Settings > Appearance)
# - filled: Classic solid icons (default Material Design)
# - outlined: Thin outline icons
# - rounded: Soft rounded icons
# - twotone: Two-tone icons with primary/secondary colors
STYLES=("filled" "outlined" "round" "twotone")
STYLE_URLS=("materialicons" "materialiconsoutlined" "materialiconsround" "materialiconstwotone")
STYLE_DIRS=("filled" "outlined" "rounded" "twotone")

# Function to download icon with retry and version fallback
download_icon() {
    local icon_name="$1"
    local style_url="$2"
    local output_dir="$3"
    local output_file="$output_dir/$icon_name.svg"

    # Skip if already exists
    if [ -f "$output_file" ]; then
        echo "  [SKIP] $icon_name (already exists)"
        return 0
    fi

    # Try different versions (newer icons use higher versions)
    for version in 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1; do
        local url="https://fonts.gstatic.com/s/i/${style_url}/${icon_name}/v${version}/24px.svg"

        # Download with curl (silent, follow redirects, fail on error)
        if curl -sf -o "$output_file" "$url" 2>/dev/null; then
            # Verify it's a valid SVG (contains <svg)
            if grep -q "<svg" "$output_file" 2>/dev/null; then
                echo "  [OK] $icon_name (v$version)"
                return 0
            else
                rm -f "$output_file"
            fi
        fi
    done

    echo "  [FAIL] $icon_name (not found in any version)"
    return 1
}

# Main download loop
echo "============================================"
echo "Downloading Material Design Icons"
echo "============================================"
echo ""

total_icons=${#ICONS[@]}
total_styles=${#STYLES[@]}
total_to_download=$((total_icons * total_styles))
downloaded=0
failed=0

for i in "${!STYLES[@]}"; do
    style="${STYLES[$i]}"
    style_url="${STYLE_URLS[$i]}"
    style_dir="${STYLE_DIRS[$i]}"
    output_dir="$ICONS_DIR/$style_dir"

    echo "=========================================="
    echo "Style: $style"
    echo "Output: $output_dir"
    echo "=========================================="

    for icon in "${ICONS[@]}"; do
        if download_icon "$icon" "$style_url" "$output_dir"; then
            ((downloaded++))
        else
            ((failed++))
        fi
    done

    echo ""
done

echo "============================================"
echo "Download Complete!"
echo "============================================"
echo "Total icons: $total_to_download"
echo "Downloaded: $downloaded"
echo "Failed: $failed"
echo "Skipped (existing): $((total_to_download - downloaded - failed))"
echo ""

# Count files in each directory
echo "Files per style:"
for style_dir in "${STYLE_DIRS[@]}"; do
    count=$(ls -1 "$ICONS_DIR/$style_dir"/*.svg 2>/dev/null | wc -l)
    echo "  $style_dir: $count SVG files"
done
