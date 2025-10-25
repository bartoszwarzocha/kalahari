# Branding & Visual Identity

> African-inspired design for a writer's companion

**Status:** ✅ Finalized
**Last Updated:** 2025-10-24

---

## Brand Overview

**Kalahari** embraces an **African wildlife theme** that reflects:
- Vast creative potential (like the Kalahari Desert)
- Natural, organic writing process
- Diverse personalities (8 animal assistants)
- Warm, inviting aesthetic (not cold/corporate)

---

## Name & Meaning

### Kalahari

**Origin:** Kalahari Desert, Southern Africa

**Meaning & Symbolism:**
- **Vast & Expansive** - Like a writer's imagination
- **Life thrives in harsh conditions** - Writers persevering through challenges
- **Beautiful & Evocative** - Memorable, pleasant to say
- **Natural** - Organic, not artificial/tech

**Pronunciation:** /ˌkæləˈhɑːri/ (kah-lah-HAR-ee)

### African Ecosystem Naming

All tools in the ecosystem follow the African naming convention:

| Tool | Named After | Type | Status |
|------|-------------|------|--------|
| **Kalahari** | Kalahari Desert | Writer's IDE | In development |
| **Serengeti** | Serengeti Plains | Collaborative writing | Planned |
| **Okavango** | Okavango Delta | Research & knowledge mgmt | Planned |
| **Kilimanjaro** | Mount Kilimanjaro | Project management | Planned |
| **Victoria** | Lake Victoria | Cloud sync service | Planned |
| **Zambezi** | Zambezi River | Publishing toolkit | Planned |
| **Sahara** | Sahara Desert | Mobile companion apps | Planned |

**Theme:** Natural landmarks, wildlife, exploration

---

## Color Palette

### Primary Colors

**Sandy Beige** - `#E6D5B8`
- Background, panels
- Warm, neutral base
- Easy on eyes for long writing sessions

**Warm Orange** - `#D97642`
- Accent color, highlights
- Call-to-action buttons
- Represents sunrise/sunset in savanna

**Sunset Red** - `#8B3A3A`
- Error states, warnings (sparingly)
- Active/selected states
- Vibrant but not harsh

### Secondary Colors

**Savanna Green** - `#6B8E23`
- Success states, checkmarks
- Nature, growth theme
- Accent for positive feedback

**Sky Blue** - `#87CEEB`
- Information, tooltips
- Links, hyperlinks
- Calm, open feeling

### Neutral Colors

**Dark Gray** - `#2C2C2C`
- Text (on light backgrounds)
- Icons, borders

**Light Gray** - `#F5F5F5`
- Subtle backgrounds
- Dividers, disabled states

**White** - `#FFFFFF`
- Main backgrounds (light theme)
- Contrast element

**Black** - `#000000`
- Text (dark theme)
- Deep shadows

---

## Logo System

### App Icon (Solo Lion)

**Primary Icon:** Lion head + book + quill

**Concept:**
- **Lion head** - Majestic, authoritative (storyteller, king of animals)
- **Open book** - Writing, literature
- **Quill** - Classic writing tool, author craft

**Style:** Realistic (photorealistic rendering or high-quality illustration)

**Sizes:**
- 1024x1024 (macOS, high-res)
- 512x512 (Windows, Linux)
- 256x256, 128x128, 64x64, 32x32, 16x16 (scaled versions)

**File Formats:**
- .icns (macOS)
- .ico (Windows - multi-size)
- .png (Linux, web)
- .svg (vector source)

**Usage:**
- Application icon (desktop, taskbar)
- File association icon (.klh files)
- About dialog
- Installer

### Marketing Logo (Multi-Animal)

**Composition:** All 8 animals in artistic composition

**Animals:**
1. Lion (center, prominent)
2. Meerkat
3. Elephant
4. Cheetah
5. Giraffe
6. Buffalo
7. Parrot
8. Chameleon

**Arrangement:**
- Lion in center (largest)
- Other animals arranged around (balanced composition)
- All looking forward or at viewer (engaging)
- Natural poses (not cartoonish)

**Style:** Realistic, watercolor or digital painting aesthetic

**Usage:**
- Website header
- Social media banners
- Marketing materials (posters, flyers)
- Presentations, pitch decks
- App splash screen (optional)

### Splash Screen

**Concept:** Dynamic - shows random animal each launch

**Design:**
- Full-screen or centered on launch
- Animal image (photorealistic)
- "Kalahari" logotype below
- Loading progress bar (optional)
- Fades in (300ms), displays for 1-2 seconds, fades out (300ms)

**Implementation:**
```cpp
std::vector<std::string> animals = {
    "lion", "meerkat", "elephant", "cheetah",
    "giraffe", "buffalo", "parrot", "chameleon"
};
std::string random_animal = animals[rand() % animals.size()];
show_splash(f"assets/splash/{random_animal}.png");
```

**Purpose:**
- Brand reinforcement (users see different animals)
- Engaging, not boring
- Fun surprise element

---

## Typography

### Headings

**Font Family:** "Merriweather" or "Lora"
- Serif font (professional, literary feel)
- Good readability
- Free & open source (Google Fonts)

**Sizes:**
- H1: 32px, bold
- H2: 24px, bold
- H3: 18px, semibold

### Body Text

**Font Family:** "Open Sans" or "Roboto"
- Sans-serif (clean, modern)
- Excellent readability at small sizes
- Wide range of weights

**Sizes:**
- Body: 14px, regular
- Small: 12px, regular
- Caption: 11px, regular

### Code / Monospace

**Font Family:** "JetBrains Mono" or "Fira Code"
- Monospace (for code examples, logs)
- Ligatures supported
- Programming-focused design

**Size:**
- Code: 13px, regular

### Editor Text (User Content)

**Font Family:** User-configurable, defaults:
- Serif: "Merriweather", "Georgia", "Times New Roman"
- Sans-serif: "Open Sans", "Arial", "Helvetica"
- Monospace: "Courier New", "Consolas"

**Size:** User-configurable, default 14px

---

## Animal Assistant Designs

### Design Principles

**Style:** Realistic (photorealistic renderings or high-quality digital paintings)

**Why Realistic (not cartoon)?**
- Professional feel (not childish)
- Engaging, beautiful to look at
- Memorable, iconic
- Aligns with brand identity (nature, authenticity)

**Moods per Animal:** 6-8 expressions

**Standard Moods:**
1. **Neutral** - Default, resting
2. **Happy** - Smile, bright eyes (positive feedback)
3. **Proud** - Confident, chest out (goal reached)
4. **Worried** - Concerned expression (break reminder, warning)
5. **Thinking** - Contemplative (analyzing, processing)
6. **Sleeping** - Resting, eyes closed (user idle, night mode)

**Optional Moods:**
7. **Excited** - Animated, energetic
8. **Confused** - Puzzled expression (error, unexpected state)

### Image Specifications

**Format:** PNG with transparency (alpha channel)
**Size:** 400x400px (base size, will be scaled down in UI)
**Resolution:** 2x for retina displays (800x800px rendered at 400x400)

**File Naming:**
```
assets/assistant/
├── lion/
│   ├── neutral.png
│   ├── happy.png
│   ├── proud.png
│   ├── worried.png
│   ├── thinking.png
│   └── sleeping.png
├── meerkat/
│   └── ...
└── ...
```

### 8 Animal Personalities

**MVP (Phase 2-3):**
1. **Lion** - Majestic, authoritative
2. **Meerkat** - Friendly, enthusiastic
3. **Elephant** - Wise, patient
4. **Cheetah** - Fast, energetic

**Phase 4+:**
5. **Giraffe** - Gentle, big-picture
6. **Buffalo** - Persistent, strong
7. **Parrot** - Talkative, linguistic
8. **Chameleon** - Adaptive, flexible

---

## UI Design System

### Theme: Light (Default)

**Background:**
- Main: `#FFFFFF` (white)
- Panel: `#F5F5F5` (light gray)
- Hover: `#E6D5B8` (sandy beige)

**Text:**
- Primary: `#2C2C2C` (dark gray)
- Secondary: `#6C6C6C` (medium gray)
- Disabled: `#AAAAAA` (light gray)

**Accent:**
- Primary: `#D97642` (warm orange)
- Secondary: `#6B8E23` (savanna green)

**Borders:**
- Default: `#DDDDDD`
- Focus: `#D97642` (warm orange)

### Theme: Dark

**Background:**
- Main: `#1E1E1E` (dark gray)
- Panel: `#2C2C2C` (lighter dark gray)
- Hover: `#3C3C3C`

**Text:**
- Primary: `#FFFFFF` (white)
- Secondary: `#CCCCCC` (light gray)
- Disabled: `#777777`

**Accent:**
- Primary: `#D97642` (warm orange - same as light)
- Secondary: `#6B8E23` (savanna green)

**Borders:**
- Default: `#3C3C3C`
- Focus: `#D97642`

### Theme: Savanna

**Background:**
- Main: `#FFF8E7` (warm cream)
- Panel: `#F5E6D3`
- Hover: `#E6D5B8`

**Text:**
- Primary: `#3C2F2F` (dark brown)
- Secondary: `#6C5D4F`

**Accent:**
- Primary: `#D97642` (warm orange)
- Secondary: `#8B3A3A` (sunset red)

**Borders:**
- Default: `#D9C8B0`
- Focus: `#D97642`

### Theme: Midnight

**Background:**
- Main: `#0D1117` (very dark blue-gray)
- Panel: `#161B22`
- Hover: `#1F2937`

**Text:**
- Primary: `#C9D1D9` (light blue-gray)
- Secondary: `#8B949E`

**Accent:**
- Primary: `#87CEEB` (sky blue)
- Secondary: `#6B8E23` (savanna green)

**Borders:**
- Default: `#30363D`
- Focus: `#87CEEB`

---

## Iconography

### Icon Style

**Style:** Line icons (2px stroke)
- Clean, minimal
- Consistent stroke weight
- 24x24px base size (scalable)

**Color:**
- Light theme: `#2C2C2C` (dark gray)
- Dark theme: `#FFFFFF` (white)
- Accent: Use theme accent colors for active states

### Standard Icons

**File Operations:**
- New Project: Document with plus
- Open: Folder opening
- Save: Floppy disk (classic)
- Export: Arrow pointing out of box

**Editing:**
- Bold: **B**
- Italic: *I*
- Underline: <u>U</u>
- Undo: Arrow curving left
- Redo: Arrow curving right

**Formatting:**
- Heading: H with levels (H1, H2, H3)
- Alignment: Lines with alignment indicators
- List: Bullet points or numbers

**View:**
- Focus Mode: Target/bullseye
- Distraction-Free: Expand arrows
- Panels: Grid/layout icon

**Assistant:**
- Animal head silhouette
- Speech bubble

**Statistics:**
- Chart: Bar chart icon
- Word Count: "123" with counter

---

## Marketing Materials

### Website (kalahari.app)

**Hero Section:**
- Screenshot of Kalahari in action
- Headline: "Write Your Story with Kalahari"
- Subheadline: "The Writer's IDE for Book Authors"
- CTA Button: "Download Free" (warm orange)

**Features Section:**
- 4-6 key features with icons
- Screenshots/mockups
- Short descriptions

**Testimonials:**
- Quotes from beta testers
- Photos optional (or animal avatars)

**Footer:**
- Links: Docs, Blog, GitHub, Twitter
- Copyright, License info

### Social Media

**Twitter/X Banner:**
- Multi-animal logo
- "Kalahari - Writer's IDE"
- kalahari.app

**Twitter/X Profile:**
- Solo Lion icon

**GitHub Social Preview:**
- Multi-animal logo
- "Kalahari - Open Source Writer's IDE"
- "C++20 | wxWidgets | Python Plugins"

---

## Voice & Tone

### Brand Voice

**Characteristics:**
- **Warm** - Friendly, not cold/corporate
- **Professional** - Serious about writing, but not stuffy
- **Encouraging** - Motivating, positive (like assistant animals)
- **Clear** - No jargon, straightforward communication

**Avoid:**
- Corporate buzzwords ("synergy", "leverage", "disrupt")
- Overly technical jargon (unless in technical docs)
- Condescending tone ("obviously", "just", "simply")
- Hype/exaggeration ("revolutionary", "best ever")

### Example Copy

**Good:**
> "Kalahari helps you focus on your story by handling the technical details. Organize your chapters, track your characters, and export to publishing formats - all in one place."

**Bad:**
> "Kalahari revolutionizes the writing experience with cutting-edge AI-powered synergies that leverage your creative potential to disrupt the authoring space."

---

## Asset Checklist

### Phase 0-1 (Foundation)

- [ ] App icon (solo Lion) - 1024x1024 PNG
- [ ] Splash screen (8 animals) - 1920x1080 PNG each
- [ ] Light theme UI mockup

### Phase 2-3 (Plugin System)

- [ ] Lion assistant (6 moods) - 400x400 PNG each
- [ ] Meerkat assistant (6 moods)
- [ ] Elephant assistant (6 moods)
- [ ] Cheetah assistant (6 moods)
- [ ] UI icons (24x24 SVG) - ~30 icons

### Phase 5 (Release)

- [ ] Marketing logo (multi-animal) - SVG + PNG
- [ ] Website hero screenshot
- [ ] Social media banners
- [ ] Installer graphics (Windows wizard, macOS DMG background)
- [ ] Giraffe, Buffalo, Parrot, Chameleon (6 moods each)

---

## Brand Guidelines Summary

**Do:**
- ✅ Use African wildlife imagery
- ✅ Warm, natural color palette
- ✅ Realistic animal illustrations (not cartoons)
- ✅ Professional but friendly tone
- ✅ Emphasize creativity, focus, writing craft

**Don't:**
- ❌ Use unrelated imagery (tech, generic office)
- ❌ Cold, corporate colors (pure gray, blue)
- ❌ Cartoon/mascot style (too playful)
- ❌ Aggressive, pushy tone
- ❌ Overhype or exaggerate features

---

## Next Steps

- **[01_overview.md](01_overview.md)** - Project vision & goals
- **[08_gui_design.md](08_gui_design.md)** - UI/UX specifications
- **Asset creation:** Commission animal illustrations, icon set

---

**Version:** 1.0
**Status:** ✅ Finalized
**Last Updated:** 2025-10-24
