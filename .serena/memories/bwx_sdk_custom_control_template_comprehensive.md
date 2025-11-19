---
**⚠️ ARCHIVED - HISTORICAL REFERENCE ONLY**

**Status:** ARCHIVED (2025-11-19)  
**Reason:** Kalahari migrated from wxWidgets 3.3.0+ to Qt6 6.5.0+  
**Relevance:** Historical reference for bwx_sdk (wxWidgets-specific)  
**Active Alternative:** Qt6 custom controls (QWidget subclasses)  
**Migration Decision:** See `qt_migration_decision_2025-11-19` memory

This memory documents wxWidgets custom control patterns used in the bwx_sdk submodule. 
While bwx_sdk is preserved in the wxwidgets-archive branch, Kalahari's GUI layer is now Qt6-based.

**Do NOT use these patterns for new Kalahari GUI code.**

---

# BWX_SDK Custom Control Development Template

**Date:** 2025-11-03  
**Purpose:** Comprehensive pattern guide for creating custom wxWidgets controls in bwx_sdk  
**Sources:** bwxGamepadCtrl + wxWidgets Book Examples (MyCalendar, MyGraphicButton, MySimpleProgressBar, MyResizableImgPanel)

---

## 1. BASE CLASS HIERARCHY

### wxControl vs wxPanel

**Use wxControl when:**
- Creating a control that users interact with (buttons, progress bars, calendars)
- Need proper keyboard navigation (TAB order)
- Control should participate in validator system
- Need wxControlBase functionality

**Use wxPanel when:**
- Creating a container for images/graphics
- No user interaction expected (pure display)
- Don't need control-specific features

**Examples:**
- `MyCalendar`, `MyGraphicButton`, `MySimpleProgressBar`: **wxControl**
- `MyResizableImgPanel`, `bwxGamepadCtrl`: **wxPanel**

---

## 2. COMPLETE HEADER TEMPLATE

```cpp
/////////////////////////////////////////////////////////////////////////////
// Name:        bwx_my_control.h
// Purpose:     Custom control description
// Author:      Your Name
// Created:     YYYY-MM-DD
// Copyright:   (c) Copyright info
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

// ============================================================================
// 1. STYLE FLAGS (if control supports styles)
// ============================================================================

enum MyControlStyles
{
    MYCTRL_STYLE_A       = 0x00000001,
    MYCTRL_STYLE_B       = 0x00000002,
    MYCTRL_DEFAULT_STYLE = (MYCTRL_STYLE_A | wxBORDER_SIMPLE)
};

// ============================================================================
// 2. CUSTOM EVENT CLASS (if control emits custom events)
// ============================================================================

class MyControlEvent : public wxCommandEvent
{
public:
    MyControlEvent(wxEventType eventType = wxEVT_NULL, int id = wxID_ANY)
        : wxCommandEvent(eventType, id) { }

    // MANDATORY: Clone() for event propagation
    wxEvent* Clone() const override { return new MyControlEvent(*this); }

    // Custom event data getters/setters
    void SetCustomData(int data) { m_customData = data; }
    int GetCustomData() const { return m_customData; }

private:
    int m_customData = 0;

    wxDECLARE_DYNAMIC_CLASS(MyControlEvent);
};

// Event type declarations
wxDECLARE_EVENT(wxEVT_MYCTRL_ACTION, MyControlEvent);
wxDECLARE_EVENT(wxEVT_MYCTRL_CHANGE, MyControlEvent);

// Event handler typedef and macro
typedef void (wxEvtHandler::*MyControlEventFunction)(MyControlEvent&);
#define MyControlEventHandler(func) \
    wxEVENT_HANDLER_CAST(MyControlEventFunction, func)

// Event table macros
#define EVT_MYCTRL_ACTION(id, func) \
    wx__DECLARE_EVT1(wxEVT_MYCTRL_ACTION, id, MyControlEventHandler(func))
#define EVT_MYCTRL_CHANGE(id, func) \
    wx__DECLARE_EVT1(wxEVT_MYCTRL_CHANGE, id, MyControlEventHandler(func))

// ============================================================================
// 3. CONTROL CLASS
// ============================================================================

class MyControl : public wxControl  // or wxPanel
{
public:
    // Default constructor
    MyControl() { Init(); }

    // Full constructor
    MyControl(wxWindow* parent, 
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = MYCTRL_DEFAULT_STYLE);

    virtual ~MyControl();

    // Two-phase construction
    bool Create(wxWindow* parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = MYCTRL_DEFAULT_STYLE);

    // ========================================================================
    // Public API
    // ========================================================================

    void SetValue(int value);
    int GetValue() const { return m_value; }

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

    // ========================================================================
    // Drawing
    // ========================================================================

    /// Immediate redraw using wxClientDC
    void PaintNow();

    /// Centralized drawing logic
    void DoDraw(wxDC& dc);

private:
    // Member variables
    int m_value;
    long m_style;

    wxDECLARE_DYNAMIC_CLASS(MyControl);
};
```

---

## 3. COMPLETE IMPLEMENTATION TEMPLATE

```cpp
/////////////////////////////////////////////////////////////////////////////
// Name:        bwx_my_control.cpp
// Purpose:     Custom control implementation
/////////////////////////////////////////////////////////////////////////////

#include "bwx_my_control.h"

// ============================================================================
// Event definitions
// ============================================================================

wxDEFINE_EVENT(wxEVT_MYCTRL_ACTION, MyControlEvent);
wxDEFINE_EVENT(wxEVT_MYCTRL_CHANGE, MyControlEvent);

// ============================================================================
// RTTI implementation
// ============================================================================

wxIMPLEMENT_DYNAMIC_CLASS(MyControlEvent, wxCommandEvent);
wxIMPLEMENT_DYNAMIC_CLASS(MyControl, wxControl);

// ============================================================================
// Constructor / Destructor
// ============================================================================

MyControl::MyControl(wxWindow* parent, wxWindowID id,
                     const wxPoint& pos, const wxSize& size, long style)
{
    Create(parent, id, pos, size, style);
}

bool MyControl::Create(wxWindow* parent, wxWindowID id,
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

void MyControl::Init()
{
    // Initialize member variables
    m_value = 0;

    // Bind events
    Bind(wxEVT_PAINT, &MyControl::OnPaint, this);
    Bind(wxEVT_SIZE, &MyControl::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &MyControl::OnMouse, this);
}

MyControl::~MyControl()
{
    // Unbind events (important for proper cleanup)
    Unbind(wxEVT_PAINT, &MyControl::OnPaint, this);
    Unbind(wxEVT_SIZE, &MyControl::OnSize, this);
    Unbind(wxEVT_LEFT_DOWN, &MyControl::OnMouse, this);
}

// ============================================================================
// wxWidgets overrides
// ============================================================================

wxSize MyControl::DoGetBestSize() const
{
    // Return appropriate size based on style/content
    return wxSize(150, 30);
}

// ============================================================================
// Public API
// ============================================================================

void MyControl::SetValue(int value)
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
        MyControlEvent event(wxEVT_MYCTRL_CHANGE, GetId());
        event.SetEventObject(this);
        event.SetCustomData(value);
        ProcessWindowEvent(event);
    }
}

// ============================================================================
// Event handlers
// ============================================================================

void MyControl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    DoDraw(dc);
}

void MyControl::OnSize(wxSizeEvent& event)
{
    // Invalidate cached data if size-dependent
    PaintNow();
    event.Skip();
}

void MyControl::OnMouse(wxMouseEvent& event)
{
    // Handle mouse interaction
    event.Skip();
}

// ============================================================================
// Drawing
// ============================================================================

void MyControl::PaintNow()
{
    wxClientDC dc(this);
    DoDraw(dc);
}

void MyControl::DoDraw(wxDC& dc)
{
    // Get control size
    int w, h;
    dc.GetSize(&w, &h);

    // Clear background
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    // Draw control content
    dc.SetPen(wxPen(GetForegroundColour()));
    dc.SetBrush(wxBrush(GetForegroundColour()));
    dc.DrawRectangle(0, 0, w, h);
}
```

---

## 4. ADVANCED PATTERNS FROM bwxGamepadCtrl

### 4.1 wxGraphicsContext with Antialiasing

```cpp
#include <wx/graphics.h>

void MyControl::DoDraw(wxDC& dc)
{
    // Double buffering
    wxBufferedDC bufferedDC(&dc, GetSize());
    bufferedDC.Clear();

    // Create graphics context with specific renderer
    wxGraphicsContext* gc = m_renderer->CreateContext(bufferedDC);
    
    if (!gc) return;

    // Configure rendering quality
    gc->SetAntialiasMode(m_antialiasing ? wxANTIALIAS_DEFAULT : wxANTIALIAS_NONE);
    gc->SetInterpolationQuality(wxINTERPOLATION_BEST);
    gc->SetCompositionMode(wxCOMPOSITION_OVER);

    // Apply transformations
    SetTransformations(gc);

    // Draw with graphics context
    DrawContent(gc);

    delete gc;
}

void MyControl::SetTransformations(wxGraphicsContext* gc)
{
    wxGraphicsMatrix transform = gc->CreateMatrix();
    transform.Scale(m_scaleFactor, m_scaleFactor);
    transform.Translate(m_offsetX, m_offsetY);
    gc->SetTransform(transform);
}
```

### 4.2 Renderer Selection (Platform-Specific)

```cpp
class MyControl : public wxControl
{
public:
    void SetGraphicsRenderer(wxGraphicsRenderer* renderer);
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

// Implementation
void MyControl::SetDefaultRenderer()
{
    m_renderer = wxGraphicsRenderer::GetDefaultRenderer();
}

#if __WXMSW__
void MyControl::SetRendererGDI()
{
    m_renderer = wxGraphicsRenderer::GetGDIPlusRenderer();
}

void MyControl::SetRendererDirect2D()
{
    m_renderer = wxGraphicsRenderer::GetDirect2DRenderer();
}
#else
void MyControl::SetRendererCairo()
{
    m_renderer = wxGraphicsRenderer::GetCairoRenderer();
}
#endif
```

### 4.3 Hit Detection with Color-Coded Bitmap

```cpp
class MyControl : public wxControl
{
private:
    wxBitmap* m_hitMapBitmap;
    wxVector<wxColour> m_hitMapColors;

    void CreateHitMap();
    int HitTest(int x, int y);
};

void MyControl::CreateHitMap()
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

int MyControl::HitTest(int x, int y)
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
```

---

## 5. GRADIENT RENDERING (from MySimpleProgressBar)

```cpp
void MyControl::DrawGradient(wxDC& dc, int x, int y, int width, int height,
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

---

## 6. IMAGE CACHING (from MyResizableImgPanel)

```cpp
class MyControl : public wxPanel
{
private:
    wxImage m_originalImage;
    wxBitmap m_cachedBitmap;
    int m_cachedWidth, m_cachedHeight;
};

void MyControl::DoDraw(wxDC& dc)
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

---

## 7. STATE MANAGEMENT (from MyGraphicButton)

```cpp
class MyControl : public wxControl
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

void MyControl::UpdateState(State newState)
{
    if (m_currentState != newState)
    {
        m_currentState = newState;
        PaintNow();
    }
}

void MyControl::OnMouseEnter(wxMouseEvent& event)
{
    UpdateState(STATE_HOVER);
    event.Skip();
}

void MyControl::OnMouseLeave(wxMouseEvent& event)
{
    UpdateState(STATE_NORMAL);
    event.Skip();
}

void MyControl::OnLeftDown(wxMouseEvent& event)
{
    UpdateState(STATE_ACTIVE);
    event.Skip();
}

void MyControl::DoDraw(wxDC& dc)
{
    wxBitmap* bmp = nullptr;
    
    switch (m_currentState)
    {
        case STATE_HOVER:
            bmp = m_bitmapHover.IsOk() ? &m_bitmapHover : &m_bitmapNormal;
            break;
        case STATE_ACTIVE:
            bmp = m_bitmapActive.IsOk() ? &m_bitmapActive : &m_bitmapNormal;
            break;
        case STATE_DISABLED:
            // Draw disabled version
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

---

## 8. COMMON BASE CLASS PROPOSAL

### 8.1 Base Class: bwxCustomControl

```cpp
namespace bwx_sdk {
namespace gui {

/// Base class for all bwx_sdk custom controls
/// Provides common infrastructure for drawing, events, and state management
class bwxCustomControl : public wxControl
{
public:
    bwxCustomControl();
    virtual ~bwxCustomControl();

protected:
    // ========================================================================
    // Virtual methods for derived classes to override
    // ========================================================================

    /// Initialize control-specific data (called from Create())
    virtual void InitControl() = 0;

    /// Central drawing method (called from OnPaint and PaintNow)
    virtual void DoDraw(wxDC& dc) = 0;

    /// Return control's best size (must override)
    wxSize DoGetBestSize() const override = 0;

    // ========================================================================
    // Common infrastructure
    // ========================================================================

    /// Immediate redraw using wxClientDC
    void PaintNow();

    /// Standard event handlers (can be overridden)
    virtual void OnPaint(wxPaintEvent& event);
    virtual void OnSize(wxSizeEvent& event);

    // ========================================================================
    // Graphics context support
    // ========================================================================

    /// Enable/disable antialiasing
    void SetAntialiasing(bool enable) { m_antialiasing = enable; }
    bool GetAntialiasing() const { return m_antialiasing; }

    /// Create graphics context with current settings
    wxGraphicsContext* CreateGraphicsContext(wxDC& dc);

private:
    bool m_antialiasing = true;
    wxGraphicsRenderer* m_renderer = nullptr;

    wxDECLARE_ABSTRACT_CLASS(bwxCustomControl);
};

} // namespace gui
} // namespace bwx_sdk
```

### 8.2 Usage Example

```cpp
class bwxTextEditor : public bwxCustomControl
{
public:
    bwxTextEditor();
    virtual ~bwxTextEditor();

    bool Create(wxWindow* parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0);

protected:
    void InitControl() override;
    void DoDraw(wxDC& dc) override;
    wxSize DoGetBestSize() const override;

private:
    // Editor-specific members
    std::vector<std::string> m_lines;
    int m_caretLine = 0;
    int m_caretColumn = 0;

    wxDECLARE_DYNAMIC_CLASS(bwxTextEditor);
};

// Implementation
void bwxTextEditor::InitControl()
{
    // Editor-specific initialization
    m_lines.push_back("");
    
    Bind(wxEVT_CHAR, &bwxTextEditor::OnChar, this);
}

void bwxTextEditor::DoDraw(wxDC& dc)
{
    wxGraphicsContext* gc = CreateGraphicsContext(dc);
    if (!gc) return;

    // Draw editor content with antialiasing
    DrawTextLines(gc);
    DrawCaret(gc);

    delete gc;
}
```

---

## 9. KEY DESIGN DECISIONS

### 9.1 When to Use Each Pattern

| Pattern | Use Case | Example |
|---------|----------|---------|
| **wxControl base** | Interactive controls with keyboard support | Calendar, Button, ProgressBar |
| **wxPanel base** | Display-only controls | Image viewer, Gamepad display |
| **Custom events** | Control state changes need notification | Button clicked, Calendar date selected |
| **wxGraphicsContext** | Need antialiasing, advanced drawing | Rounded shapes, smooth curves |
| **Hit detection bitmap** | Complex clickable regions | Gamepad buttons, irregular shapes |
| **State management** | Visual feedback for interaction | Button hover/active, control focus |
| **Image caching** | Expensive scaling operations | Resizable images, thumbnails |
| **Gradient rendering** | Smooth color transitions | Progress bars, gauges |

### 9.2 Performance Considerations

1. **Double Buffering:** Always use wxBufferedDC or wxBufferedPaintDC to prevent flicker
2. **Cache Heavy Operations:** Scale images once, reuse until size changes
3. **Antialiasing Toggle:** Provide option to disable for performance-critical scenarios
4. **Hit Map Updates:** Only regenerate when control geometry changes
5. **PaintNow() Guard:** Check `IsShown()` before triggering immediate redraws

### 9.3 Memory Management

1. **Graphics Context:** Always delete after use (`delete gc;`)
2. **Event Binding:** Unbind in destructor to prevent dangling pointers
3. **Bitmap Resources:** Use smart pointers or RAII wrappers
4. **Renderer Selection:** Store renderer pointer, don't recreate repeatedly

---

## 10. CHECKLIST FOR NEW CUSTOM CONTROL

When creating a new custom control in bwx_sdk, follow this checklist:

- [ ] Choose base class (wxControl or wxPanel)
- [ ] Define RTTI macros (wxDECLARE_DYNAMIC_CLASS, wxIMPLEMENT_DYNAMIC_CLASS)
- [ ] Define custom events (if needed)
- [ ] Implement two-phase construction (default constructor + Create())
- [ ] Implement Init() method
- [ ] Override DoGetBestSize()
- [ ] Implement OnPaint() handler
- [ ] Implement DoDraw() centralized drawing
- [ ] Implement PaintNow() for immediate redraws
- [ ] Bind events in Init()
- [ ] Unbind events in destructor
- [ ] Consider wxGraphicsContext for advanced drawing
- [ ] Add state management if interactive
- [ ] Cache expensive operations (images, calculations)
- [ ] Test on all platforms (Windows, Linux, macOS)
- [ ] Document public API with Doxygen comments

---

## 11. INTEGRATION WITH KALAHARI

**Strategy:** "Need in Kalahari → Solution in bwx_sdk"

1. **Identify Need:** Kalahari requires custom text editor with page view
2. **Design in bwx_sdk:** Create `bwxTextEditor` using this template
3. **Implement Incrementally:** Start with basic text display, add features
4. **Test Standalone:** Unit tests in bwx_sdk before Kalahari integration
5. **Integrate:** Add to Kalahari when stable and tested

**Example Controls for Kalahari:**
- `bwxTextEditor` - Rich text editor with page view
- `bwxTimelineControl` - Story timeline visualization
- `bwxMindMapControl` - Character/plot relationship graph
- `bwxStatisticsGauge` - Writing progress visualization

---

## 12. REFERENCES

**Source Code Analyzed:**
1. `bwxGamepadCtrl` - Advanced graphics, hit detection, transformations
   - Location: `E:\C++\Projekty\Libs\bwx_sdk_stare_podejście\include\bwx_gui\bwx_gamepadctrl.h`
   - Key features: wxGraphicsContext, antialiasing, color-coded hit map
   
2. `MyCalendar` - Custom events, grid layout
   - Book example: Custom calendar with date selection events
   
3. `MyGraphicButton` - State management, bitmap states
   - Book example: Button with hover/active visual feedback
   
4. `MySimpleProgressBar` - Gradient rendering, style flags
   - Book example: Customizable progress bar with color gradients
   
5. `MyResizableImgPanel` - Image caching, dynamic scaling
   - Book example: Auto-scaling image panel

**This template synthesizes best practices from all sources for robust custom control development.**
