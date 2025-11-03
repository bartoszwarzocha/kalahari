# Task #00016: TipTap Rich Text Editor with Page View

**Status:** ❌ REJECTED
**Assignee:** N/A
**Estimated effort:** 10 days (2 weeks)
**Priority:** N/A
**Phase:** Phase 1 - Core Editor
**Dependencies:** Task #00014 (EditorPanel foundation)
**Rejection Date:** 2025-11-03
**Rejection Reason:** Strategic decision to implement custom wxWidgets-based text editor control (Task #00014_02) instead of web-based solution. Reasons: (1) Better native performance, (2) No wxWebView/WebKit dependencies, (3) Consistent with C++ architecture, (4) Better integration with wxWidgets ecosystem, (5) Avoids browser engine overhead and complexity.

---

## 📋 Overview (REJECTED)

~~Replace wxRichTextCtrl with wxWebView + TipTap editor to provide:
- Professional page-like view (A4 pages with margins)~~

**Decision:** Custom wxWidgets control (Task #00014_02) chosen for better native integration and performance.
- Multiple view modes (Author, Publisher, Plain Text)
- Full rich text editing capabilities (bold, italic, tables, lists, etc.)
- Separation of content (HTML) and presentation (CSS)
- Custom features for book writers (footnotes, comments, fragment marking)

**Decision rationale:** See research in session 2025-11-01 - TipTap provides ProseMirror's full power with easier API, battle-tested by NYT/Atlassian, actively developed.

---

## 🎯 Goals

### Primary Goals
1. **Page-like editor view** - white pages on gray background, A4 format, comfortable margins
2. **All editing features** - bold, italic, underline, color, indent, tables, lists
3. **Multiple view modes** - instant CSS switching (Author/Publisher/Plain)
4. **Stable foundation** - production-ready, cross-platform (Windows/macOS/Linux)

### Secondary Goals
1. **Custom book features** - footnotes, comments, fragment marking
2. **Clean HTML storage** - separation of content and presentation
3. **Extensibility** - easy to add new features via TipTap extensions

---

## 📦 Deliverables

### Code Changes

#### 1. EditorPanel refactor (C++)
- **File:** `src/gui/panels/editor_panel.cpp`
- **Changes:**
  - Replace wxRichTextCtrl with wxWebView
  - Implement C++ ↔ JavaScript bridge (RunScript, AddScriptMessageHandler)
  - Load TipTap HTML/CSS/JS resources
  - Implement save/load (HTML content)
  - Handle events (text changed, word count)

#### 2. TipTap integration (JavaScript/HTML/CSS)
- **Directory:** `resources/editor/` (new)
- **Files:**
  - `index.html` - TipTap container
  - `tiptap-editor.js` - TipTap configuration + extensions
  - `styles-author.css` - Author view (page-like)
  - `styles-publisher.css` - Publisher view (plain)
  - `styles-common.css` - Shared styles

#### 3. Build system updates
- **File:** `CMakeLists.txt`
- **Changes:**
  - Install HTML/JS/CSS resources to bundle
  - Configure resource paths (Windows/macOS/Linux)

#### 4. MainWindow integration
- **File:** `src/gui/main_window.cpp`
- **Changes:**
  - Add "View Mode" menu (Author/Publisher/Plain)
  - Update Format menu handlers (delegate to TipTap)
  - Handle view switching

---

## 🛠️ Technical Approach

### Architecture

```
┌─────────────────────────────────────────┐
│         EditorPanel (C++)               │
│  ┌───────────────────────────────────┐  │
│  │      wxWebView                    │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │  TipTap Editor (JavaScript) │  │  │
│  │  │                             │  │  │
│  │  │  - Extensions (50+)         │  │  │
│  │  │  - Custom Page Node         │  │  │
│  │  │  - Custom Footnote Node     │  │  │
│  │  │  - Custom Comment Mark      │  │  │
│  │  └─────────────────────────────┘  │  │
│  │           ↕ RunScript()            │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │  C++ Event Handlers         │  │  │
│  │  │  - onContentChanged()       │  │  │
│  │  │  - onWordCountUpdate()      │  │  │
│  │  │  - onSaveRequest()          │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
          ↕
┌─────────────────────────────────────────┐
│    BookElement (HTML storage)           │
│    - content.html (clean HTML)          │
│    - metadata.json (wordCount, etc.)    │
└─────────────────────────────────────────┘
```

### TipTap Configuration

**Core Extensions:**
- `StarterKit` - bold, italic, lists, headings, etc.
- `Table` - table support with row/col operations
- `TextStyle` - color, font customization
- `Underline` - underline formatting
- `TextAlign` - left/center/right/justify
- `Indent` - paragraph indentation

**Custom Extensions:**
- `PageNode` - A4 page container with page breaks
- `FootnoteNode` - inline footnote markers
- `CommentMark` - robocze comments (highlighted spans)
- `FragmentMark` - oznaczanie fragmentów różnymi stylami

### View Modes (CSS Switching)

**Author Mode (default):**
```css
body { background: #f0f0f0; padding: 40px; }
.page {
  width: 21cm; max-width: 21cm;
  min-height: 29.7cm; /* A4 */
  background: white;
  margin: 20px auto;
  padding: 2.5cm 2.5cm;
  box-shadow: 0 0 10px rgba(0,0,0,0.1);
  font-family: 'Georgia', serif;
  font-size: 12pt;
  line-height: 1.6;
}
.page + .page {
  border-top: 2px dashed #ccc;
  margin-top: 20px;
  page-break-before: always;
}
```

**Publisher Mode:**
```css
body { background: white; margin: 20px; }
.page {
  font-family: 'Courier New', monospace;
  font-size: 12pt;
  line-height: 2.0; /* double spacing */
  max-width: 100%;
  padding: 0;
  box-shadow: none;
  border: none;
}
```

**Plain Text Mode:**
```css
body { background: white; margin: 20px; }
* { font-family: monospace; font-size: 12pt; }
/* Strip all formatting */
```

### C++ ↔ JavaScript Bridge

**C++ → JavaScript (commands):**
```cpp
// Apply formatting
void EditorPanel::applyBold() {
    m_webView->RunScript("editor.chain().focus().toggleBold().run()");
}

// Switch view mode
void EditorPanel::setViewMode(ViewMode mode) {
    wxString css = getViewModeCSS(mode);
    wxString script = wxString::Format(
        "document.getElementById('theme').textContent = `%s`;",
        css
    );
    m_webView->RunScript(script);
}
```

**JavaScript → C++ (events):**
```javascript
// Content changed
editor.on('update', () => {
  window.webkit.messageHandlers.contentChanged.postMessage({
    html: editor.getHTML(),
    wordCount: countWords(editor.getText())
  });
});

// C++ handler
void EditorPanel::onScriptMessage(wxWebViewEvent& evt) {
    wxString message = evt.GetString();
    // Parse JSON, update UI
}
```

### File Storage Format

**Before (RTF - mixed content/format):**
```
{\rtf1\ansi
{\fonttbl {\f0 Times;}}
{\b Chapter 1}\par
Some text...
}
```

**After (HTML - clean separation):**
```html
<!-- content.html -->
<div class="page">
  <h1>Chapter 1</h1>
  <p>Some text...</p>
</div>
```

```json
// metadata.json
{
  "wordCount": 42,
  "modified": "2025-11-01T12:34:56Z",
  "viewMode": "author"
}
```

---

## ✅ Implementation Checklist

### Week 1: Foundation + Page View

**Day 1: wxWebView Setup**
- [ ] Remove wxRichTextCtrl from EditorPanel
- [ ] Add wxWebView to EditorPanel
- [ ] Create resources/editor/ directory structure
- [ ] Add TipTap CDN links to index.html (or bundle locally)
- [ ] Test basic HTML loading in wxWebView
- [ ] Verify RunScript() works (simple JavaScript test)

**Day 2: TipTap Basic Editor**
- [ ] Implement tiptap-editor.js with StarterKit
- [ ] Configure basic extensions (bold, italic, lists)
- [ ] Test editor initialization
- [ ] Implement C++ → JS bridge (applyBold, applyItalic, etc.)
- [ ] Test formatting commands from C++

**Day 3: Page Node Implementation**
- [ ] Create PageNode extension (custom TipTap node)
- [ ] Implement NodeView for A4 page rendering
- [ ] Add page-break-after CSS
- [ ] Test automatic page creation
- [ ] Add page separator lines (visual feedback)

**Day 4: Author View Styling**
- [ ] Implement styles-author.css (page-like view)
- [ ] Test margins, padding, shadows
- [ ] Implement gray background ("desk" effect)
- [ ] Test page overflow and pagination
- [ ] Polish visual appearance

**Day 5: View Mode Switching**
- [ ] Implement styles-publisher.css (plain text view)
- [ ] Implement styles-common.css (shared)
- [ ] Add CSS switching via JavaScript
- [ ] Add "View Mode" menu in MainWindow
- [ ] Test switching between Author/Publisher modes
- [ ] Verify content preservation during switch

### Week 2: Features + Integration

**Day 6: Table Support**
- [ ] Add Table extension to TipTap config
- [ ] Test table insertion
- [ ] Add toolbar buttons (insert table, add row/col)
- [ ] Test table editing in both view modes
- [ ] Verify table RTL/LTR support

**Day 7: Custom Extensions (Footnotes)**
- [ ] Implement FootnoteNode extension
- [ ] Add footnote insertion command
- [ ] Add footnote numbering logic
- [ ] Style footnotes in Author view
- [ ] Test footnote editing workflow

**Day 8: Custom Extensions (Comments + Marks)**
- [ ] Implement CommentMark extension (highlighted comments)
- [ ] Implement FragmentMark extension (colored marking)
- [ ] Add UI controls (toolbar or right-click menu)
- [ ] Test comment insertion/editing
- [ ] Style comments distinctively

**Day 9: Save/Load Implementation**
- [ ] Implement getHTML() → C++ bridge
- [ ] Implement setHTML() ← C++ bridge
- [ ] Update loadChapter() - load HTML content
- [ ] Update saveChapter() - save HTML to .klh file
- [ ] Migrate RTF → HTML converter (optional, for backward compat)
- [ ] Test save/load cycle

**Day 10: Polish + Testing**
- [ ] Word count implementation (JavaScript → C++)
- [ ] Status bar integration (update word count)
- [ ] Undo/Redo verification (TipTap built-in)
- [ ] Cross-platform testing (Windows/Linux/macOS)
- [ ] Performance testing (large documents >10k words)
- [ ] Memory leak testing (valgrind/sanitizers)
- [ ] User acceptance testing

---

## 🧪 Testing Plan

### Unit Tests
- [ ] C++ ↔ JS bridge communication
- [ ] HTML save/load cycle
- [ ] View mode switching
- [ ] Custom extension functionality

### Integration Tests
- [ ] Full editing workflow (create, edit, save, load)
- [ ] Format menu commands
- [ ] View mode persistence
- [ ] Multi-chapter editing

### Manual Testing
- [ ] Visual appearance (page view looks professional)
- [ ] Editing experience (smooth, responsive)
- [ ] Cross-platform consistency
- [ ] Large document performance (50+ pages)

### Edge Cases
- [ ] Empty document
- [ ] Very long paragraphs (>1000 words)
- [ ] Many pages (100+ pages)
- [ ] Special characters (Unicode, emojis)
- [ ] Mixed content (text + tables + lists)

---

## 📚 Resources

### TipTap Documentation
- Main docs: https://tiptap.dev/docs/editor/getting-started/overview
- Extensions: https://tiptap.dev/docs/editor/extensions/functionality
- Custom nodes: https://tiptap.dev/docs/editor/extensions/custom-extensions
- NodeView API: https://tiptap.dev/docs/editor/core-concepts/node-views

### ProseMirror (underlying)
- Documentation: https://prosemirror.net/docs/
- Schema: https://prosemirror.net/docs/guide/#schema
- Transforms: https://prosemirror.net/docs/guide/#transform

### wxWidgets
- wxWebView: https://docs.wxwidgets.org/stable/classwx_web_view.html
- RunScript: https://docs.wxwidgets.org/stable/classwx_web_view.html#a9c0e9f9f9f9f9f9f9

---

## ⚠️ Risks & Mitigations

### Risk 1: Browser Engine Compatibility
**Risk:** Edge/WebKit differences in contentEditable behavior
**Mitigation:** TipTap abstracts browser quirks, test on all platforms early

### Risk 2: Performance (Large Documents)
**Risk:** JavaScript performance on 100+ page documents
**Mitigation:** Lazy loading, virtual scrolling if needed, benchmark early

### Risk 3: C++ ↔ JS Bridge Complexity
**Risk:** RunScript() async nature, message passing overhead
**Mitigation:** Design simple message protocol, comprehensive error handling

### Risk 4: Learning Curve
**Risk:** TipTap/ProseMirror concepts need time to understand
**Mitigation:** Start with examples, read docs thoroughly, ask community

### Risk 5: Migration from RTF
**Risk:** Existing .klh files have RTF content
**Mitigation:** Implement RTF → HTML converter (optional Phase 2), or fresh start

---

## 🎯 Success Criteria

### Must Have
- ✅ Professional page-like view (indistinguishable from Word)
- ✅ All standard formatting (bold, italic, lists, tables)
- ✅ View mode switching (Author ↔ Publisher)
- ✅ Stable save/load (no data loss)
- ✅ Cross-platform (Windows, macOS, Linux)

### Nice to Have
- ✅ Footnotes working
- ✅ Comments working
- ✅ Fragment marking working
- ✅ Smooth performance (>50 pages)
- ✅ RTF backward compatibility

### Stretch Goals
- Spell checking integration
- Grammar checking
- AI assistant integration (future task)
- Real-time collaboration (Yjs - future task)

---

## 📝 Notes

- **TipTap vs Quill vs raw ProseMirror:** Chose TipTap for balance of power + ease of use
- **HTML vs RTF:** HTML provides better content/presentation separation
- **CDN vs Local Bundle:** Start with CDN for development, bundle for production
- **Version pinning:** Pin TipTap version (e.g., 2.8.0) for stability
- **Browser engine:** wxWebView uses Edge (Windows), WebKit (macOS), WebKit2 (Linux)

---

## 🔗 Related Tasks

- Task #00014 - EditorPanel (wxRichTextCtrl foundation) - **SUPERSEDED**
- Task #00015 - Project Navigator Panel - **DEPENDENCY** (need to load chapters)
- Future: AI Assistant integration (Phase 2)
- Future: Spell/Grammar checking (Phase 2)
- Future: Export to DOCX/PDF (Phase 2)

---

**Created:** 2025-11-01
**Updated:** 2025-11-01
**Author:** Claude (with user approval)
