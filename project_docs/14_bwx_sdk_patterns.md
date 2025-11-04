# bwx_sdk Custom Controls - Design Patterns & Rationale

**Purpose:** Architectural documentation for custom wxWidgets control development in bwx_sdk for Kalahari project.

**Status:** ✅ Complete
**Version:** 1.0
**Last Updated:** 2025-11-04

---

## Strategic Decision: Custom Controls in bwx_sdk

### Why Build Custom Controls?

**Rejected Alternatives:**
- ❌ **wxRichTextCtrl** (Task #00015) - Insufficient feature control, architectural limitations
- ❌ **TipTap + wxWebView** (Task #00016) - Browser overhead, complexity, non-native feel
- ❌ **Third-party libraries** - License concerns, dependency management, learning curve

**Accepted Approach:**
- ✅ **Custom wxWidgets controls from scratch** - Complete architectural control, native performance, full flexibility

### Why bwx_sdk First (Not Directly in Kalahari)?

**"Need in Kalahari → Solution in bwx_sdk" Strategy:**

```
Kalahari needs custom text editor
    ↓
Is it wxWidgets utility? YES
    ↓
Does bwx_sdk have it? NO
    ↓
Build in bwx_sdk → Test standalone → Use in Kalahari
```

**Benefits:**
1. **Reusability** - Other projects can use the same controls
2. **Separation of Concerns** - Generic wxWidgets utilities vs Kalahari domain logic
3. **Standalone Testing** - Controls can be tested independently
4. **Maintainability** - Centralized location for wxWidgets-specific code
5. **Clean Integration** - Git submodule workflow keeps Kalahari codebase focused

**Reference:** See `bwx_sdk_kalahari_integration_strategy_MASTER` (Serena memory) for full integration strategy.

---

## Pattern Analysis Summary

### Research Sources (2025-11-03)

**Analyzed Controls:**
1. **bwxGamepadCtrl** - Complex graphics, hit detection, transformations
   - Source: Old bwx_sdk project (not in current codebase)
   - Key features: wxGraphicsContext, antialiasing, color-coded hit map, renderer selection

2. **wxWidgets Book Examples:**
   - **MyCalendar** - Grid layout, custom events, keyboard navigation
   - **MyGraphicButton** - State management (normal/hover/active), multiple bitmaps
   - **MySimpleProgressBar** - Gradient rendering, style flags
   - **MyResizableImgPanel** - Image caching, dynamic scaling

**Result:** Comprehensive template with 7 key patterns (documented in Serena memory and `.claude/skills/kalahari-bwx-custom-controls.md`)

---

## Core Patterns & Rationale

### Pattern 1: Two-Phase Construction

**Why:**
- Standard wxWidgets idiom for proper initialization
- Allows failure handling (Create() can return false)
- Separates object creation from resource allocation
- Consistent with all wx* controls

**Pattern:**
```cpp
// Default constructor
bwxYourControl() { Init(); }

// Full constructor
bwxYourControl(parent, id, ...) { Create(parent, id, ...); }

// Create() for two-phase
bool Create(parent, id, ...) {
    if (!wxControl::Create(...)) return false;
    Init();
    return true;
}
```

**Alternative Rejected:** Single-phase construction (error handling via exceptions) - Not wxWidgets style.

### Pattern 2: Centralized Drawing (OnPaint → DoDraw)

**Why:**
- Single source of truth for all rendering logic
- PaintNow() and OnPaint() both use DoDraw()
- Easier testing (can call DoDraw() directly)
- Reduces code duplication

**Pattern:**
```cpp
void OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    DoDraw(dc);
}

void PaintNow() {
    wxClientDC dc(this);
    DoDraw(dc);
}

void DoDraw(wxDC& dc) {
    // ALL rendering logic here
}
```

**Alternative Rejected:** Separate drawing logic in OnPaint() and PaintNow() - Code duplication, inconsistency.

### Pattern 3: Buffered Painting

**Why:**
- Prevents flicker (double buffering)
- Smooth visual updates
- Standard practice for custom controls
- No performance penalty on modern hardware

**Pattern:**
```cpp
void DoDraw(wxDC& dc) {
    wxBufferedDC bufferedDC(&dc, GetSize());
    // Draw to bufferedDC
}
```

**Alternative Rejected:** Direct DC rendering - Causes visible flicker on complex redraws.

### Pattern 4: wxGraphicsContext for Quality

**Why:**
- Antialiasing (smooth text, curves)
- Advanced features (gradients, transparency, transformations)
- Platform-optimized renderers (GDI+, Direct2D, Cairo)
- Better visual quality for modern UIs

**Pattern:**
```cpp
void DoDraw(wxDC& dc) {
    wxGraphicsContext* gc = m_renderer->CreateContext(dc);
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    // High-quality rendering
    delete gc;
}
```

**When to Use:**
- Complex graphics (mind maps, diagrams)
- Text rendering (smooth fonts)
- Any control where visual quality matters

**When NOT to Use:**
- Simple rectangles/lines (standard DC is faster)
- Performance-critical code (profiling shows bottleneck)

**Alternative:** Standard wxDC - Faster, but no antialiasing.

### Pattern 5: Event Unbinding in Destructor

**Why:**
- Prevents dangling pointers (control destroyed, event still bound)
- Memory leak prevention
- Crashes avoided (event fired after destruction)
- Best practice for RAII

**Pattern:**
```cpp
~bwxYourControl() {
    // CRITICAL: Unbind ALL events
    Unbind(wxEVT_PAINT, &bwxYourControl::OnPaint, this);
    Unbind(wxEVT_SIZE, &bwxYourControl::OnSize, this);
    // ... all other events
}
```

**Alternative Rejected:** Rely on automatic cleanup - Not guaranteed, causes crashes.

### Pattern 6: Custom Events for Communication

**Why:**
- Decoupling (control doesn't know about parent)
- Standard wxWidgets event system
- Allows multiple listeners
- Event propagation to ancestors

**Pattern:**
```cpp
// Event class with Clone()
class YourControlEvent : public wxCommandEvent {
    wxEvent* Clone() const override { return new YourControlEvent(*this); }
};

// Emit event
YourControlEvent event(wxEVT_YOURCTRL_CHANGE, GetId());
event.SetEventObject(this);
ProcessWindowEvent(event);
```

**When to Use:**
- Control state changes (value changed, item selected)
- User interactions (clicked, double-clicked, right-click)
- Any time parent needs notification

**Alternative Rejected:** Direct parent pointer - Tight coupling, not reusable.

### Pattern 7: State Management for Interactivity

**Why:**
- Visual feedback (hover, active, disabled states)
- User experience (shows control is responsive)
- Standard for interactive controls
- Easy to implement with enum + UpdateState()

**Pattern:**
```cpp
enum State { NORMAL, HOVER, ACTIVE, DISABLED };
State m_currentState;

void UpdateState(State newState) {
    if (m_currentState != newState) {
        m_currentState = newState;
        PaintNow();  // Redraw with new state
    }
}
```

**When to Use:**
- Buttons, clickable controls
- Any control with visual interaction feedback

**Alternative:** No state management - Poor UX, control feels "dead".

---

## Advanced Patterns (From Analysis)

### Hit Detection with Color-Coded Bitmap

**Source:** bwxGamepadCtrl

**Use Case:** Complex clickable regions (irregular shapes, overlapping areas)

**Why:**
- Accurate pixel-perfect hit detection
- Works with any shape (no geometry calculations)
- Fast lookup (single pixel read)
- Scales to many regions

**How:**
1. Create offscreen bitmap
2. Draw each region with unique color (no antialiasing!)
3. On click: Read pixel color at mouse position
4. Match color to region index

**When to Use:**
- Mind map nodes (irregular shapes)
- Gamepad buttons (circular, overlapping)
- Complex diagrams (connections, nodes)

**Alternative:** Bounding box checks - Simpler, but inaccurate for irregular shapes.

### Image Caching

**Source:** MyResizableImgPanel

**Use Case:** Expensive rendering operations (scaling, transformations)

**Why:**
- Avoid redundant calculations
- Only recompute on size change
- Significant performance improvement

**How:**
1. Store original image
2. Cache scaled bitmap + dimensions
3. On size change: Rescale and cache
4. Otherwise: Use cached bitmap

**When to Use:**
- Resizable images
- Thumbnails
- Any expensive drawing operation

**Alternative:** Render every frame - Causes lag on resize.

### Transformation Matrix (Zoom/Pan)

**Source:** bwxGamepadCtrl

**Use Case:** Zoomable/pannable content (mind maps, timelines, diagrams)

**Why:**
- Separates "world space" from "screen space"
- Easy zoom/pan implementation
- Smooth transformations
- Standard computer graphics technique

**How:**
1. Create wxGraphicsMatrix
2. Apply scale (zoom)
3. Apply translate (pan)
4. Set on graphics context
5. Draw in world coordinates

**When to Use:**
- Mind maps (zoom to node)
- Timeline (pan through events)
- Any zoomable/scrollable custom control

**Alternative:** Manual coordinate translation - Error-prone, hard to maintain.

---

## Integration Workflow

### Step 1: Design (Interactive Questions)

**Before coding, answer:**
1. wxControl or wxPanel? (interactive vs display-only)
2. What features? (click, drag, keyboard, custom events)
3. Rendering strategy? (antialiasing, hit detection, caching)
4. Performance constraints? (document size, update frequency)
5. Reusable? (bwx_sdk vs Kalahari-specific)

**Tool:** Use `.claude/skills/kalahari-bwx-custom-controls.md` (guides through questions)

### Step 2: Develop in bwx_sdk

**Location:**
- Header: `external/bwx_sdk/include/bwx_sdk/bwx_gui/bwx_your_control.h`
- Source: `external/bwx_sdk/src/bwx_gui/bwx_your_control.cpp`
- Tests: `external/bwx_sdk/tests/test_your_control.cpp` (if applicable)

**Namespace:** `bwx_sdk::gui::`

**Naming:**
- Class: `bwxYourControl` (PascalCase, `bwx` prefix)
- Files: `bwx_your_control.h/.cpp` (snake_case)

### Step 3: Test Standalone

**Cross-platform testing:**
- Linux (GCC 10+, Debug + Release)
- macOS (Clang 10+, Debug + Release)
- Windows (MSVC 2019+, Debug + Release)

**Zero warnings policy** - Fix at source, don't suppress.

### Step 4: Commit to bwx_sdk

```bash
cd external/bwx_sdk
git add include/bwx_sdk/bwx_gui/bwx_your_control.h
git add src/bwx_gui/bwx_your_control.cpp
git commit -m "feat(gui): Add bwxYourControl"
git push origin main
```

### Step 5: Update Kalahari Submodule

```bash
cd /path/to/Kalahari
cd external/bwx_sdk
git pull origin main
cd ../..
git add external/bwx_sdk
git commit -m "chore: Update bwx_sdk (add bwxYourControl)"
```

### Step 6: Use in Kalahari

```cpp
#include <bwx_sdk/bwx_gui/bwx_your_control.h>

// Use in panel
bwx_sdk::gui::bwxYourControl* ctrl =
    new bwx_sdk::gui::bwxYourControl(this, wxID_ANY);
```

**Reference:** See `bwx_sdk_kalahari_integration_strategy_MASTER` (Serena memory) for detailed workflow.

---

## Quality Standards

### Code Quality

- ✅ **Zero compiler warnings** - All platforms (Linux, macOS, Windows)
- ✅ **C++20 modern patterns** - `std::bit_cast`, smart pointers, fixed-size types
- ✅ **RTTI macros** - `wxDECLARE_DYNAMIC_CLASS`, `wxIMPLEMENT_DYNAMIC_CLASS`
- ✅ **Doxygen comments** - Public API documented (`///`, `@param`, `@return`)
- ✅ **Consistent style** - Matches bwx_sdk conventions

**Example of Fixed Warning (Task #00018):**
```cpp
// OLD (undefined behavior, platform-dependent):
long i;
i = *(long*)&y;  // Type punning

// NEW (C++20, type-safe):
int32_t i;
i = std::bit_cast<int32_t>(y);  // Always 4 bytes, all platforms
```

### Cross-Platform Compatibility

**Testing Matrix:**
| Platform | Compiler | Versions |
|----------|----------|----------|
| Linux | GCC | 10+ |
| macOS | Clang | 10+ |
| Windows | MSVC | 2019+ |

**Each platform:** Debug + Release builds

**Common Issues:**
- `sizeof(long)` differs (Windows: 4 bytes, Linux/macOS: 8 bytes) → Use `int32_t`
- Line endings (CRLF vs LF) → Git handles automatically
- File paths (backslash vs forward slash) → Use `std::filesystem::path`
- Renderer differences → Test with each platform's default renderer

### Performance Standards

**Target:** Smooth 60 FPS interaction
- Text editor: 10,000+ words, <100ms keystroke latency
- Mind map: 1,000+ nodes, smooth zoom/pan
- Timeline: 500+ events, real-time updates

**Profiling:** Use platform tools before optimizing
- Linux: gprof, perf
- macOS: Instruments
- Windows: Visual Studio Profiler

**Optimization Priority:**
1. Measure (profile with real data)
2. Optimize hotspots (not guesses)
3. Re-measure (verify improvement)

---

## Design Decisions Record

### Decision 1: bwx_sdk Location (2025-11-02)

**Context:** Where to implement custom controls?

**Options:**
- A) Directly in Kalahari (`include/kalahari/gui/controls/`)
- B) In bwx_sdk first, then integrate

**Decision:** Option B (bwx_sdk first)

**Rationale:**
- Follows "Need → Solution" integration strategy
- Enables reusability across projects
- Allows standalone testing
- Clean separation of concerns

**Reference:** Task #00017, #00018 (bwx_sdk integration)

### Decision 2: Pattern Selection (2025-11-03)

**Context:** Which patterns to use for custom controls?

**Research:** Analysis of bwxGamepadCtrl + wxWidgets Book examples

**Selected Patterns:**
1. Two-phase construction (wxWidgets standard)
2. Centralized drawing (OnPaint → DoDraw)
3. Buffered painting (no flicker)
4. wxGraphicsContext (quality over speed)
5. Event unbinding (memory safety)
6. Custom events (loose coupling)
7. State management (UX feedback)

**Rationale:**
- Proven patterns (used in production code)
- Cross-platform compatible
- Performance acceptable (profiling shows no bottlenecks)
- Maintainable (clear structure)

**Reference:** Serena memory `bwx_sdk_custom_control_template_comprehensive`

### Decision 3: Custom Text Editor Strategy (2025-11-03)

**Context:** How to implement text editor for Kalahari?

**Options:**
- A) Use wxRichTextCtrl (existing control)
- B) Use TipTap + wxWebView (web-based)
- C) Custom control from scratch

**Decision:** Option C (custom control)

**Rationale:**
- Full architectural control (formatting, rendering, input)
- Native performance (no browser engine)
- Extensibility (add features as needed)
- Proven feasible (analysis shows similar controls exist)

**Reference:** Tasks #00015 (rejected), #00016 (rejected), #00019 (accepted)

---

## Future Custom Controls (Planned)

### Phase 1: Core Editor

1. **bwxTextEditor** (Task #00019) - Rich text editor for chapters
   - Base: wxControl
   - Features: Multiline, formatting (bold/italic/underline), word wrap, scrolling
   - Patterns: Centralized drawing, buffered painting, custom events
   - Status: 📋 Next task

### Phase 2+: Advanced Visualizations

2. **bwxTimelineControl** - Story timeline visualization
   - Base: wxControl
   - Features: Zoomable, pannable, event markers, connections
   - Patterns: Transformation matrix, hit detection, state management

3. **bwxMindMapControl** - Character/plot relationship graph
   - Base: wxControl
   - Features: Nodes, connections, drag & drop, zoom/pan
   - Patterns: Hit detection (color-coded bitmap), transformation matrix, state management

4. **bwxStatisticsGauge** - Writing progress visualization
   - Base: wxControl
   - Features: Circular progress, color gradients, animations
   - Patterns: Gradient rendering, antialiasing

5. **bwxCharacterCard** - Interactive character profile display
   - Base: wxPanel
   - Features: Image, text fields, collapsible sections
   - Patterns: Image caching, state management (collapsed/expanded)

**All controls:** Follow patterns documented here, implemented via `.claude/skills/kalahari-bwx-custom-controls.md` workflow.

---

## Related Documentation

### Internal Documentation

- **[.claude/skills/kalahari-bwx-custom-controls.md](../.claude/skills/kalahari-bwx-custom-controls.md)** - Executable workflow (HOW)
- **[03_architecture.md](03_architecture.md)** - Overall Kalahari architecture
- **[12_dev_protocols.md](12_dev_protocols.md)** - Development protocols, task workflow

### Serena Memories

- **bwx_sdk_custom_control_template_comprehensive** - Full analysis (8000+ words)
- **bwx_sdk_kalahari_integration_strategy_MASTER** - Integration strategy
- **bwx_sdk_integration_decisions_complete** - Technical implementation details

### External Resources

- **wxControl:** https://docs.wxwidgets.org/3.3/classwx_control.html
- **wxGraphicsContext:** https://docs.wxwidgets.org/3.3/classwx_graphics_context.html
- **Custom Controls Overview:** https://docs.wxwidgets.org/3.3/overview_windowsizing.html

---

## Summary

**Strategic Approach:** Custom controls in bwx_sdk first, then integrate into Kalahari

**Key Patterns:** 7 core patterns proven in bwxGamepadCtrl and wxWidgets Book examples

**Quality Standards:** Zero warnings, cross-platform tested, C++20 modern patterns

**Workflow:** Design → Develop in bwx_sdk → Test standalone → Commit → Update submodule → Integrate

**Tool:** Use `.claude/skills/kalahari-bwx-custom-controls.md` for step-by-step implementation guidance

---

**Document Status:** ✅ Complete
**Last Review:** 2025-11-04
**Next Review:** After implementing first custom control (bwxTextEditor, Task #00019)
**Maintainer:** Kalahari Core Team
