# Kalahari BWX Custom Controls Development

**USE ME when:** Creating custom wxWidgets controls from scratch in bwx_sdk for Kalahari project.

**Purpose:** Complete workflow for designing, implementing, testing, and integrating custom wxWidgets controls using proven patterns from bwxGamepadCtrl and wxWidgets Book examples.

**Related Documentation:** `project_docs/14_bwx_sdk_patterns.md` (architectural rationale)

---

## When to Use This Skill

✅ **Use this skill when:**
- Creating NEW custom control from scratch (not using existing wx* controls)
- Building interactive control (text editors, calendars, timelines, buttons)
- Creating visualization control (graphs, charts, mind maps, gauges)
- Need full architectural control over rendering, input, state
- Building for bwx_sdk → Kalahari integration

❌ **Do NOT use this skill when:**
- Using existing wxWidgets controls (wxRichTextCtrl, wxTreeCtrl, etc.) → use `kalahari-wxwidgets` skill
- Creating simple layouts with sizers → standard wxWidgets patterns
- Building plugins → use `kalahari-plugin-system` skill

---

## Complete Workflow: From Concept to Integration

### Phase 0: Design Questions (INTERACTIVE)

**Ask user these questions BEFORE coding:**

1. **Base Class Selection**
   - ❓ Is this control interactive (keyboard, mouse input)?
   - ❓ Should it participate in TAB order?
   - ❓ Does it need validator support?
   - **→ YES to any:** Use `wxControl`
   - **→ NO to all:** Use `wxPanel`

2. **Feature Requirements**
   - ❓ What user interactions? (click, drag, hover, keyboard)
   - ❓ Visual states? (normal, hover, active, disabled, focused)
   - ❓ Custom events needed? (notify parent of state changes)
   - ❓ Configurable styles? (flags for appearance/behavior)

3. **Rendering Requirements**
   - ❓ Need antialiasing? (smooth curves, text)
   - ❓ Complex graphics? (gradients, transformations, transparency)
   - ❓ Hit detection? (clickable regions with complex shapes)
   - ❓ Image caching? (expensive rendering operations)
   - **→ YES to antialiasing/complex:** Use `wxGraphicsContext`
   - **→ NO:** Use standard `wxDC`

4. **Performance Constraints**
   - ❓ Expected document size? (lines, elements)
   - ❓ Real-time updates? (typing, animation)
   - ❓ Memory limits?
   - **→ Design data structure accordingly (rope, gap buffer, piece table)**

5. **Integration Strategy**
   - ❓ Reusable beyond Kalahari?
   - **→ YES:** Build in bwx_sdk first, then integrate
   - **→ NO:** Build directly in Kalahari

### Phase 1: File Structure Setup

**Create these files in bwx_sdk:**

```
external/bwx_sdk/
├── include/bwx_sdk/bwx_gui/
│   └── bwx_YOUR_CONTROL.h          # Header (public API)
├── src/bwx_gui/
│   ├── bwx_YOUR_CONTROL.cpp        # Implementation
│   └── CMakeLists.txt              # Update: add source files
└── tests/
    └── test_YOUR_CONTROL.cpp       # Unit tests
```

**Naming Convention:**
- Class: `bwxYourControl` (PascalCase with `bwx` prefix)
- Files: `bwx_your_control.h/.cpp` (snake_case)
- Namespace: `bwx_sdk::gui::`

### Phase 2: Header Template

**Complete header template (`.h` file):**

```cpp
/////////////////////////////////////////////////////////////////////////////
// Name:        bwx_your_control.h
// Purpose:     [Brief description of control purpose]
// Author:      Kalahari Team
// Created:     [YYYY-MM-DD]
// Copyright:   (c) [YEAR] Kalahari Project
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _BWX_YOUR_CONTROL_H_
#define _BWX_YOUR_CONTROL_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/graphics.h>  // If using wxGraphicsContext

namespace bwx_sdk {
namespace gui {

// ============================================================================
// 1. STYLE FLAGS (if control supports configurable styles)
// ============================================================================

enum YourControlStyles
{
    YOURCTRL_STYLE_A       = 0x00000001,
    YOURCTRL_STYLE_B       = 0x00000002,
    YOURCTRL_DEFAULT_STYLE = (YOURCTRL_STYLE_A | wxBORDER_SIMPLE)
};

// ============================================================================
// 2. CUSTOM EVENT CLASS (if control emits custom events)
// ============================================================================

class YourControlEvent : public wxCommandEvent
{
public:
    YourControlEvent(wxEventType eventType = wxEVT_NULL, int id = wxID_ANY)
        : wxCommandEvent(eventType, id) { }

    // MANDATORY: Clone() for event propagation
    wxEvent* Clone() const override { return new YourControlEvent(*this); }

    // Custom event data getters/setters
    void SetCustomData(int data) { m_customData = data; }
    int GetCustomData() const { return m_customData; }

private:
    int m_customData = 0;

    wxDECLARE_DYNAMIC_CLASS(YourControlEvent);
};

// Event type declarations
wxDECLARE_EVENT(wxEVT_YOURCTRL_ACTION, YourControlEvent);
wxDECLARE_EVENT(wxEVT_YOURCTRL_CHANGE, YourControlEvent);

// Event handler typedef and macro
typedef void (wxEvtHandler::*YourControlEventFunction)(YourControlEvent&);
#define YourControlEventHandler(func) \
    wxEVENT_HANDLER_CAST(YourControlEventFunction, func)

// Event table macros
#define EVT_YOURCTRL_ACTION(id, func) \
    wx__DECLARE_EVT1(wxEVT_YOURCTRL_ACTION, id, YourControlEventHandler(func))
#define EVT_YOURCTRL_CHANGE(id, func) \
    wx__DECLARE_EVT1(wxEVT_YOURCTRL_CHANGE, id, YourControlEventHandler(func))

// ============================================================================
// 3. CONTROL CLASS
// ============================================================================

class bwxYourControl : public wxControl  // or wxPanel
{
public:
    // ========================================================================
    // Construction / Destruction
    // ========================================================================

    /// Default constructor (two-phase construction)
    bwxYourControl();

    /// Full constructor
    bwxYourControl(wxWindow* parent,
                   wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = YOURCTRL_DEFAULT_STYLE);

    /// Destructor (unbind events!)
    virtual ~bwxYourControl();

    /// Two-phase construction Create() method
    bool Create(wxWindow* parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = YOURCTRL_DEFAULT_STYLE);

    // ========================================================================
    // Public API
    // ========================================================================

    void SetValue(int value);
    int GetValue() const { return m_value; }

    void SetAntialiasing(bool enable);
    bool GetAntialiasing() const { return m_antialiasing; }

protected:
    // ========================================================================
    // wxWidgets overrides
    // ========================================================================

    /// Return best size for this control
    wxSize DoGetBestSize() const override;

    // ========================================================================
    // Initialization
    // ========================================================================

    /// Initialize member variables and bind events
    void Init();

    // ========================================================================
    // Event handlers
    // ========================================================================

    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouse(wxMouseEvent& event);
    void OnChar(wxKeyEvent& event);

    // ========================================================================
    // Drawing
    // ========================================================================

    /// Immediate redraw using wxClientDC
    void PaintNow();

    /// Centralized drawing logic (SINGLE place for all rendering)
    void DoDraw(wxDC& dc);

    /// Create graphics context with current settings
    wxGraphicsContext* CreateGraphicsContext(wxDC& dc);

private:
    // ========================================================================
    // Member variables
    // ========================================================================

    int m_value;
    long m_style;
    bool m_antialiasing;
    wxGraphicsRenderer* m_renderer;

    wxDECLARE_DYNAMIC_CLASS(bwxYourControl);
};

} // namespace gui
} // namespace bwx_sdk

#endif  // _BWX_YOUR_CONTROL_H_
```

### Phase 3: Implementation Template

**Complete implementation template (`.cpp` file):**

```cpp
/////////////////////////////////////////////////////////////////////////////
// Name:        bwx_your_control.cpp
// Purpose:     [Brief description] implementation
/////////////////////////////////////////////////////////////////////////////

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <bwx_sdk/bwx_gui/bwx_your_control.h>

namespace bwx_sdk {
namespace gui {

// ============================================================================
// Event definitions
// ============================================================================

wxDEFINE_EVENT(wxEVT_YOURCTRL_ACTION, YourControlEvent);
wxDEFINE_EVENT(wxEVT_YOURCTRL_CHANGE, YourControlEvent);

// ============================================================================
// RTTI implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(YourControlEvent, wxCommandEvent);
wxIMPLEMENT_DYNAMIC_CLASS(bwxYourControl, wxControl);

// ============================================================================
// Constructor / Destructor
// ============================================================================

bwxYourControl::bwxYourControl()
{
    Init();
}

bwxYourControl::bwxYourControl(wxWindow* parent, wxWindowID id,
                               const wxPoint& pos, const wxSize& size, long style)
{
    Create(parent, id, pos, size, style);
}

bool bwxYourControl::Create(wxWindow* parent, wxWindowID id,
                            const wxPoint& pos, const wxSize& size, long style)
{
    // Call base class Create()
    if (!wxControl::Create(parent, id, pos, size, style))
    {
        return false;
    }

    // Initialize after successful creation
    Init();

    m_style = style;

    return true;
}

void bwxYourControl::Init()
{
    // Initialize member variables
    m_value = 0;
    m_antialiasing = true;
    m_renderer = wxGraphicsRenderer::GetDefaultRenderer();

    // Bind events
    Bind(wxEVT_PAINT, &bwxYourControl::OnPaint, this);
    Bind(wxEVT_SIZE, &bwxYourControl::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &bwxYourControl::OnMouse, this);
    Bind(wxEVT_CHAR, &bwxYourControl::OnChar, this);
}

bwxYourControl::~bwxYourControl()
{
    // CRITICAL: Unbind events to prevent dangling pointers
    Unbind(wxEVT_PAINT, &bwxYourControl::OnPaint, this);
    Unbind(wxEVT_SIZE, &bwxYourControl::OnSize, this);
    Unbind(wxEVT_LEFT_DOWN, &bwxYourControl::OnMouse, this);
    Unbind(wxEVT_CHAR, &bwxYourControl::OnChar, this);
}

// ============================================================================
// wxWidgets overrides
// ============================================================================

wxSize bwxYourControl::DoGetBestSize() const
{
    // Return appropriate size based on style/content
    return wxSize(200, 100);
}

// ============================================================================
// Public API
// ============================================================================

void bwxYourControl::SetValue(int value)
{
    if (m_value != value)
    {
        m_value = value;

        // Trigger redraw if visible
        if (IsShown())
        {
            PaintNow();
        }

        // Emit change event
        YourControlEvent event(wxEVT_YOURCTRL_CHANGE, GetId());
        event.SetEventObject(this);
        event.SetCustomData(value);
        ProcessWindowEvent(event);
    }
}

void bwxYourControl::SetAntialiasing(bool enable)
{
    if (m_antialiasing != enable)
    {
        m_antialiasing = enable;
        if (IsShown())
        {
            PaintNow();
        }
    }
}

// ============================================================================
// Event handlers
// ============================================================================

void bwxYourControl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    DoDraw(dc);
}

void bwxYourControl::OnSize(wxSizeEvent& event)
{
    // Invalidate cached data if size-dependent
    PaintNow();
    event.Skip();
}

void bwxYourControl::OnMouse(wxMouseEvent& event)
{
    // Handle mouse interaction
    event.Skip();
}

void bwxYourControl::OnChar(wxKeyEvent& event)
{
    // Handle keyboard input
    event.Skip();
}

// ============================================================================
// Drawing
// ============================================================================

void bwxYourControl::PaintNow()
{
    wxClientDC dc(this);
    DoDraw(dc);
}

void bwxYourControl::DoDraw(wxDC& dc)
{
    // Get control size
    int w, h;
    dc.GetSize(&w, &h);

    // Double buffering (prevents flicker)
    wxBufferedDC bufferedDC(&dc, wxSize(w, h));
    bufferedDC.Clear();

    if (m_antialiasing)
    {
        // Use wxGraphicsContext for high-quality rendering
        wxGraphicsContext* gc = CreateGraphicsContext(bufferedDC);
        if (gc)
        {
            // Configure quality
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetInterpolationQuality(wxINTERPOLATION_BEST);

            // Draw content here
            // gc->DrawEllipse(10, 10, w-20, h-20);

            delete gc;
        }
    }
    else
    {
        // Standard DC rendering (no antialiasing)
        bufferedDC.SetPen(wxPen(GetForegroundColour()));
        bufferedDC.SetBrush(wxBrush(GetBackgroundColour()));
        bufferedDC.DrawRectangle(0, 0, w, h);
    }
}

wxGraphicsContext* bwxYourControl::CreateGraphicsContext(wxDC& dc)
{
    if (!m_renderer)
    {
        m_renderer = wxGraphicsRenderer::GetDefaultRenderer();
    }

    return m_renderer ? m_renderer->CreateContext(dc) : nullptr;
}

} // namespace gui
} // namespace bwx_sdk
```

---

## Advanced Patterns from bwxGamepadCtrl Analysis

### Pattern 1: State Management (Hover/Active/Disabled)

**Use case:** Visual feedback for user interaction (buttons, interactive controls)

```cpp
class bwxYourControl : public wxControl
{
public:
    enum State
    {
        STATE_NORMAL,
        STATE_HOVER,
        STATE_ACTIVE,
        STATE_DISABLED
    };

private:
    State m_currentState;
    wxBitmap m_bitmapNormal;
    wxBitmap m_bitmapHover;
    wxBitmap m_bitmapActive;

    void UpdateState(State newState);
};

void bwxYourControl::UpdateState(State newState)
{
    if (m_currentState != newState)
    {
        m_currentState = newState;
        PaintNow();
    }
}

void bwxYourControl::OnMouseEnter(wxMouseEvent& event)
{
    UpdateState(STATE_HOVER);
    event.Skip();
}

void bwxYourControl::OnMouseLeave(wxMouseEvent& event)
{
    UpdateState(STATE_NORMAL);
    event.Skip();
}

void bwxYourControl::OnLeftDown(wxMouseEvent& event)
{
    UpdateState(STATE_ACTIVE);
    event.Skip();
}

void bwxYourControl::DoDraw(wxDC& dc)
{
    wxBitmap* bmp = nullptr;

    switch (m_currentState)
    {
        case STATE_HOVER:
            bmp = &m_bitmapHover;
            break;
        case STATE_ACTIVE:
            bmp = &m_bitmapActive;
            break;
        case STATE_DISABLED:
            // Draw grayed-out version
            break;
        case STATE_NORMAL:
        default:
            bmp = &m_bitmapNormal;
            break;
    }

    if (bmp && bmp->IsOk())
    {
        dc.DrawBitmap(*bmp, 0, 0);
    }
}
```

### Pattern 2: Hit Detection with Color-Coded Bitmap

**Use case:** Complex clickable regions (gamepad buttons, irregular shapes, mind maps)

```cpp
class bwxYourControl : public wxControl
{
private:
    wxBitmap* m_hitMapBitmap;
    wxVector<wxColour> m_hitMapColors;
    wxVector<Region> m_regions;  // Your clickable regions

    void CreateHitMap();
    int HitTest(int x, int y);
};

void bwxYourControl::CreateHitMap()
{
    // Create bitmap with unique colors for each interactive region
    m_hitMapBitmap = new wxBitmap(GetSize());

    wxMemoryDC dc;
    dc.SelectObject(*m_hitMapBitmap);

    wxGraphicsContext* gc = m_renderer->CreateContext(dc);
    gc->SetAntialiasMode(wxANTIALIAS_NONE);  // Exact pixel colors

    // Draw regions with unique colors
    for (size_t i = 0; i < m_regions.size(); ++i)
    {
        gc->SetBrush(wxBrush(m_hitMapColors[i]));
        DrawRegion(gc, m_regions[i]);
    }

    delete gc;
}

int bwxYourControl::HitTest(int x, int y)
{
    if (!m_hitMapBitmap || !m_hitMapBitmap->IsOk())
        return wxNOT_FOUND;

    wxNativePixelData data(*m_hitMapBitmap);
    if (!data) return wxNOT_FOUND;

    wxNativePixelData::Iterator p(data);
    p.MoveTo(data, x, y);

    wxColour pixelColor(p.Red(), p.Green(), p.Blue());

    // Find matching region color
    for (size_t i = 0; i < m_hitMapColors.size(); ++i)
    {
        if (m_hitMapColors[i] == pixelColor)
            return static_cast<int>(i);
    }

    return wxNOT_FOUND;
}

void bwxYourControl::OnLeftDown(wxMouseEvent& event)
{
    int regionIndex = HitTest(event.GetX(), event.GetY());

    if (regionIndex != wxNOT_FOUND)
    {
        // User clicked region regionIndex
        HandleRegionClick(regionIndex);
    }

    event.Skip();
}
```

### Pattern 3: Image Caching (Expensive Operations)

**Use case:** Resizable images, thumbnails, scaled content

```cpp
class bwxYourControl : public wxPanel
{
private:
    wxImage m_originalImage;
    wxBitmap m_cachedBitmap;
    int m_cachedWidth, m_cachedHeight;
};

void bwxYourControl::DoDraw(wxDC& dc)
{
    int currentW, currentH;
    dc.GetSize(&currentW, &currentH);

    // Only rescale if size changed
    if (currentW != m_cachedWidth || currentH != m_cachedHeight)
    {
        m_cachedBitmap = wxBitmap(
            m_originalImage.Scale(currentW, currentH, wxIMAGE_QUALITY_HIGH)
        );
        m_cachedWidth = currentW;
        m_cachedHeight = currentH;
    }

    dc.DrawBitmap(m_cachedBitmap, 0, 0, false);
}
```

### Pattern 4: Gradient Rendering (Smooth Transitions)

**Use case:** Progress bars, gauges, smooth backgrounds

```cpp
void bwxYourControl::DrawGradient(wxDC& dc, int x, int y, int width, int height,
                                  const wxColour& col1, const wxColour& col2,
                                  double progress)
{
    // Calculate current color based on progress
    int endX = x + static_cast<int>(width * progress);

    int R_end = (col2.Red() * progress) + (col1.Red() * (1.0 - progress));
    int G_end = (col2.Green() * progress) + (col1.Green() * (1.0 - progress));
    int B_end = (col2.Blue() * progress) + (col1.Blue() * (1.0 - progress));
    wxColour colorEnd(R_end, G_end, B_end);

    // Draw pixel-by-pixel gradient
    for (int i = x; i < endX; ++i)
    {
        double ratio = static_cast<double>(i - x) / (endX - x);

        int R = (colorEnd.Red() * ratio) + (col1.Red() * (1.0 - ratio));
        int G = (colorEnd.Green() * ratio) + (col1.Green() * (1.0 - ratio));
        int B = (colorEnd.Blue() * ratio) + (col1.Blue() * (1.0 - ratio));

        dc.SetPen(wxPen(wxColour(R, G, B)));
        dc.DrawLine(i, y, i, y + height);
    }
}
```

### Pattern 5: Platform-Specific Renderer Selection

**Use case:** Optimize rendering for each platform

```cpp
class bwxYourControl : public wxControl
{
public:
    void SetDefaultRenderer();

#if __WXMSW__
    void SetRendererGDI();
    void SetRendererDirect2D();
#else
    void SetRendererCairo();
#endif

private:
    wxGraphicsRenderer* m_renderer;
};

void bwxYourControl::SetDefaultRenderer()
{
    m_renderer = wxGraphicsRenderer::GetDefaultRenderer();
}

#if __WXMSW__
void bwxYourControl::SetRendererGDI()
{
    m_renderer = wxGraphicsRenderer::GetGDIPlusRenderer();
}

void bwxYourControl::SetRendererDirect2D()
{
    m_renderer = wxGraphicsRenderer::GetDirect2DRenderer();
}
#else
void bwxYourControl::SetRendererCairo()
{
    m_renderer = wxGraphicsRenderer::GetCairoRenderer();
}
#endif
```

### Pattern 6: Transformation Matrix (Zoom, Pan, Rotate)

**Use case:** Zoomable/pannable controls (mind maps, timelines, diagrams)

```cpp
class bwxYourControl : public wxControl
{
private:
    double m_scaleFactor;
    double m_offsetX, m_offsetY;

    void SetTransformations(wxGraphicsContext* gc);
};

void bwxYourControl::SetTransformations(wxGraphicsContext* gc)
{
    wxGraphicsMatrix transform = gc->CreateMatrix();

    // Apply transformations in order: scale, then translate
    transform.Scale(m_scaleFactor, m_scaleFactor);
    transform.Translate(m_offsetX, m_offsetY);

    gc->SetTransform(transform);
}

void bwxYourControl::DoDraw(wxDC& dc)
{
    wxBufferedDC bufferedDC(&dc, GetSize());
    wxGraphicsContext* gc = CreateGraphicsContext(bufferedDC);

    if (gc)
    {
        // Apply transformations
        SetTransformations(gc);

        // Draw content (coordinates are in "world space", not screen space)
        DrawContent(gc);

        delete gc;
    }
}

void bwxYourControl::OnMouseWheel(wxMouseEvent& event)
{
    // Zoom in/out on mouse wheel
    if (event.GetWheelRotation() > 0)
    {
        m_scaleFactor *= 1.1;  // Zoom in
    }
    else
    {
        m_scaleFactor /= 1.1;  // Zoom out
    }

    PaintNow();
}
```

---

## Integration Workflow: bwx_sdk → Kalahari

### Step 1: Develop in bwx_sdk

**Why bwx_sdk first?**
- Reusability across projects
- Standalone testing
- Clean separation of concerns
- "Need → Solution" strategy

```bash
# Work in bwx_sdk submodule
cd external/bwx_sdk

# Create files
# include/bwx_sdk/bwx_gui/bwx_your_control.h
# src/bwx_gui/bwx_your_control.cpp

# Update CMakeLists.txt
# src/bwx_gui/CMakeLists.txt - add source file

# Test standalone (if tests exist)
# tests/test_your_control.cpp
```

### Step 2: Commit to bwx_sdk

```bash
cd external/bwx_sdk

# Stage changes
git add include/bwx_sdk/bwx_gui/bwx_your_control.h
git add src/bwx_gui/bwx_your_control.cpp
git add src/bwx_gui/CMakeLists.txt

# Commit
git commit -m "feat(gui): Add bwxYourControl custom control

- Two-phase construction pattern
- Antialiasing support
- Custom events (wxEVT_YOURCTRL_ACTION, wxEVT_YOURCTRL_CHANGE)
- Cross-platform tested (Linux, macOS, Windows)
"

# Push to bwx_sdk repo
git push origin main
```

### Step 3: Update Submodule in Kalahari

```bash
cd /path/to/Kalahari

# Pull latest bwx_sdk
cd external/bwx_sdk
git pull origin main
cd ../..

# Update submodule pointer
git add external/bwx_sdk
git commit -m "chore: Update bwx_sdk (add bwxYourControl)"
git push origin main
```

### Step 4: Use in Kalahari

```cpp
// include/kalahari/gui/panels/your_panel.h
#include <bwx_sdk/bwx_gui/bwx_your_control.h>

class YourPanel : public wxPanel
{
private:
    bwx_sdk::gui::bwxYourControl* m_customControl;
};

// src/gui/panels/your_panel.cpp
void YourPanel::setupLayout()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_customControl = new bwx_sdk::gui::bwxYourControl(this, wxID_ANY);
    m_customControl->SetAntialiasing(true);

    sizer->Add(m_customControl, 1, wxALL | wxEXPAND, 10);
    SetSizer(sizer);
}
```

---

## Implementation Checklist

### Pre-Implementation

- [ ] Answer all design questions (base class, features, rendering, performance)
- [ ] Review similar controls (bwxGamepadCtrl, MyCalendar, MyGraphicButton)
- [ ] Choose appropriate patterns (state management, hit detection, caching)
- [ ] Design data structures (if complex state/document model)

### Implementation Phase

- [ ] Create header file with complete template
- [ ] Create implementation file with complete template
- [ ] Implement Init() - initialize members, bind events
- [ ] Implement destructor - unbind ALL events
- [ ] Implement Create() - two-phase construction
- [ ] Implement DoGetBestSize() - return appropriate size
- [ ] Implement OnPaint() → DoDraw() pattern
- [ ] Implement PaintNow() for immediate redraws
- [ ] Implement DoDraw() - centralized drawing logic
- [ ] Add buffering (wxBufferedDC) to prevent flicker
- [ ] Add antialiasing (wxGraphicsContext) if needed
- [ ] Implement custom events (if needed)
- [ ] Implement state management (if interactive)
- [ ] Implement hit detection (if complex regions)
- [ ] Add image caching (if expensive operations)
- [ ] Update CMakeLists.txt - add source files

### Testing Phase

- [ ] Test on Linux (GCC, Debug + Release)
- [ ] Test on macOS (Clang, Debug + Release)
- [ ] Test on Windows (MSVC, Debug + Release)
- [ ] Test resizing (does layout work correctly?)
- [ ] Test keyboard navigation (TAB order, if wxControl)
- [ ] Test mouse interaction (click, drag, hover)
- [ ] Test custom events (do they propagate correctly?)
- [ ] Profile performance (is it smooth for large data?)
- [ ] Check memory leaks (valgrind, ASAN)
- [ ] Verify zero compiler warnings

### Integration Phase

- [ ] Commit to bwx_sdk with descriptive message
- [ ] Push to bwx_sdk remote repository
- [ ] Update bwx_sdk submodule in Kalahari
- [ ] Include header in Kalahari code
- [ ] Link bwx_gui library (already done)
- [ ] Test integration in Kalahari context
- [ ] Update CHANGELOG.md (Kalahari)
- [ ] Update ROADMAP.md if milestone

---

## Examples from Analysis

### Example 1: bwxGamepadCtrl (Complex Graphics)

**Source:** Old bwx_sdk project (not in current codebase)

**Key Features:**
- wxGraphicsContext with antialiasing
- Platform-specific renderers (GDI+, Direct2D, Cairo)
- Color-coded hit detection bitmap
- Transformation matrix (scale, translate)
- Image rendering with transparency

**When to Use Similar Pattern:**
- Mind map control (nodes, connections)
- Diagram editor (shapes, links)
- Game controller visualization
- Any control with complex clickable regions

### Example 2: MyCalendar (Grid, Events)

**Source:** wxWidgets Book examples

**Key Features:**
- wxControl base (interactive, keyboard support)
- Grid-based layout (days, weeks)
- Custom events (date selected)
- Mouse hover feedback
- Keyboard navigation (arrow keys)

**When to Use Similar Pattern:**
- Timeline control (grid of events)
- Schedule view (hours, appointments)
- Table with interactive cells

### Example 3: MyGraphicButton (States)

**Source:** wxWidgets Book examples

**Key Features:**
- State management (normal, hover, active, disabled)
- Multiple bitmaps (one per state)
- Smooth transitions
- Mouse enter/leave/down events

**When to Use Similar Pattern:**
- Custom buttons with complex graphics
- Toggle buttons with visual feedback
- Interactive icons

### Example 4: MySimpleProgressBar (Gradients)

**Source:** wxWidgets Book examples

**Key Features:**
- Gradient rendering (smooth color transitions)
- Progress percentage calculation
- Style flags (horizontal/vertical, colors)
- Pixel-by-pixel color interpolation

**When to Use Similar Pattern:**
- Writing progress gauge
- Word count visualization
- Any progress indicator

### Example 5: MyResizableImgPanel (Caching)

**Source:** wxWidgets Book examples

**Key Features:**
- Image caching (rescale only on size change)
- High-quality scaling (wxIMAGE_QUALITY_HIGH)
- OnSize event handling
- Cached width/height tracking

**When to Use Similar Pattern:**
- Image viewer with zoom
- Thumbnail display
- Any expensive rendering operation

---

## Common Pitfalls & Solutions

### Pitfall 1: Forgetting Event Unbinding

**Problem:** Memory leaks, crashes on control destruction

**Solution:**
```cpp
~bwxYourControl()
{
    // ALWAYS unbind in destructor
    Unbind(wxEVT_PAINT, &bwxYourControl::OnPaint, this);
    Unbind(wxEVT_SIZE, &bwxYourControl::OnSize, this);
    // ... all other events
}
```

### Pitfall 2: Flicker During Redraw

**Problem:** Screen flashing on every paint

**Solution:** Use wxBufferedDC
```cpp
void bwxYourControl::DoDraw(wxDC& dc)
{
    wxBufferedDC bufferedDC(&dc, GetSize());
    // Draw to bufferedDC, not dc
}
```

### Pitfall 3: Not Calling event.Skip()

**Problem:** Event chain breaks, parent doesn't receive event

**Solution:**
```cpp
void bwxYourControl::OnMouse(wxMouseEvent& event)
{
    // Handle event
    // ...

    // ALWAYS call Skip() unless you want to stop propagation
    event.Skip();
}
```

### Pitfall 4: Forgetting Clone() in Custom Events

**Problem:** Custom events don't propagate correctly

**Solution:**
```cpp
class YourControlEvent : public wxCommandEvent
{
public:
    // MANDATORY for event propagation
    wxEvent* Clone() const override {
        return new YourControlEvent(*this);
    }
};
```

### Pitfall 5: Not Deleting wxGraphicsContext

**Problem:** Memory leak

**Solution:**
```cpp
void bwxYourControl::DoDraw(wxDC& dc)
{
    wxGraphicsContext* gc = CreateGraphicsContext(dc);

    if (gc)
    {
        // Use gc

        delete gc;  // ALWAYS delete!
    }
}
```

### Pitfall 6: Platform-Dependent sizeof()

**Problem:** `sizeof(long)` differs (Windows: 4, Linux/macOS: 8)

**Solution:** Use fixed-size types
```cpp
#include <cstdint>

// BAD:
long value;

// GOOD:
int32_t value;  // Always 4 bytes
int64_t value;  // Always 8 bytes
```

---

## Performance Optimization Tips

1. **Cache Expensive Calculations**
   - Scale images once, reuse until size changes
   - Pre-calculate layout, invalidate on change
   - Store rendered lines/regions

2. **Use Dirty Regions**
   - Only redraw changed areas
   - Track which regions need update
   - Minimize full redraws

3. **Profile Before Optimizing**
   - Measure with real data (10K lines, 100 nodes)
   - Use platform profilers (gprof, Instruments, Visual Studio Profiler)
   - Optimize hotspots, not guesses

4. **Consider Data Structures**
   - Rope: Good for large documents (efficient insert/delete)
   - Gap Buffer: Simple, cache-friendly (good for typical editing)
   - Piece Table: Optimal for undo/redo (VS Code uses this)

5. **Lazy Rendering**
   - Only render visible area
   - Virtual scrolling for long lists
   - On-demand detail loading

---

## Quality Standards

### Code Quality

- ✅ Zero compiler warnings (all platforms)
- ✅ C++20 modern patterns (std::bit_cast, smart pointers)
- ✅ RTTI macros (wxDECLARE_DYNAMIC_CLASS, wxIMPLEMENT_DYNAMIC_CLASS)
- ✅ Doxygen comments (/// brief, @param, @return)
- ✅ Consistent naming (bwx prefix, snake_case files, PascalCase classes)

### Cross-Platform

- ✅ Test on Linux (GCC 10+)
- ✅ Test on macOS (Clang 10+)
- ✅ Test on Windows (MSVC 2019+)
- ✅ Handle platform differences (renderer, file paths, line endings)

### Testing

- ✅ Unit tests (if testable in isolation)
- ✅ Integration tests (in Kalahari context)
- ✅ Manual testing (visual verification)
- ✅ Performance testing (large documents, real-world data)

### Documentation

- ✅ Header comments (purpose, author, created date)
- ✅ Public API documentation (all public methods)
- ✅ Usage examples (in comments or separate file)
- ✅ Update project_docs/ if architectural decision

---

## Related Resources

### Documentation

- **project_docs/14_bwx_sdk_patterns.md** - Architectural rationale (WHY these patterns)
- **project_docs/03_architecture.md** - Overall project architecture
- **project_docs/12_dev_protocols.md** - Development protocols, MCP tools

### Serena Memories

- **bwx_sdk_custom_control_template_comprehensive** - Full analysis (8000+ words)
- **bwx_sdk_kalahari_integration_strategy_MASTER** - Integration strategy

### wxWidgets Documentation

- **wxControl:** https://docs.wxwidgets.org/3.3/classwx_control.html
- **wxPanel:** https://docs.wxwidgets.org/3.3/classwx_panel.html
- **wxDC:** https://docs.wxwidgets.org/3.3/classwx_d_c.html
- **wxGraphicsContext:** https://docs.wxwidgets.org/3.3/classwx_graphics_context.html
- **Custom Controls:** https://docs.wxwidgets.org/3.3/overview_windowsizing.html

### External References

- **Text Editor Data Structures:** https://en.wikipedia.org/wiki/Rope_(data_structure)
- **Gap Buffer:** https://en.wikipedia.org/wiki/Gap_buffer
- **Piece Table:** https://en.wikipedia.org/wiki/Piece_table
- **VS Code Text Buffer:** https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation

---

## Quick Reference

### Minimum Viable Custom Control

1. Choose base class (wxControl or wxPanel)
2. Two-phase construction (default constructor + Create())
3. Init() - initialize, bind events
4. Destructor - unbind events
5. OnPaint() → DoDraw() pattern
6. DoGetBestSize() override
7. wxBufferedDC to prevent flicker

### Adding Antialiasing

1. Member: `wxGraphicsRenderer* m_renderer`
2. Init: `m_renderer = wxGraphicsRenderer::GetDefaultRenderer()`
3. DoDraw: Create wxGraphicsContext from renderer
4. Configure: `gc->SetAntialiasMode(wxANTIALIAS_DEFAULT)`
5. Delete: `delete gc` after use

### Adding Custom Events

1. Define event class (inherit wxCommandEvent)
2. Override Clone() - MANDATORY
3. Declare event types (wxDECLARE_EVENT)
4. Define event types in .cpp (wxDEFINE_EVENT)
5. Emit: `ProcessWindowEvent(event)`

---

**END OF SKILL**

**Remember:** Start with Phase 0 (Design Questions) - understand requirements BEFORE coding!
