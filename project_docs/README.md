# Kalahari - Project Documentation

> **Writer's IDE** - Advanced writing environment for book authors

**Status:** 🟢 Architecture Phase
**Version:** 1.3 (Documentation Restructure Complete)
**Last Updated:** 2025-10-29

---

## 📚 Documentation Index

This directory contains the complete technical documentation for the Kalahari project. Documents are organized by topic for easy navigation.

### Core Documentation

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 01 | [**Overview**](01_overview.md) | Project vision, goals, target audience | ✅ Complete |
| 02 | [**Tech Stack**](02_tech_stack.md) | C++20, wxWidgets, vcpkg, libraries | ✅ Complete |
| 03 | [**Architecture**](03_architecture.md) | Core architecture, design patterns, structure | ✅ Complete |
| 04 | [**Plugin System**](04_plugin_system.md) | Plugin API, extension points, event bus | ✅ Complete |
| 05 | [**Business Model**](05_business_model.md) | Open Core, premium plugins, pricing | ✅ Complete |
| 06 | [**Roadmap Rules**](06_roadmap.md) | Rules for maintaining ROADMAP.md and CHANGELOG.md | ✅ Complete |
| 07 | [**MVP Tasks**](07_mvp_tasks.md) | Detailed task breakdown for MVP (18 months) | ✅ Complete |
| 08 | [**GUI Design**](08_gui_design.md) | GUI panels, wxAUI, perspectives, customizable toolbars | ✅ Complete |
| 09 | [**Internationalization**](09_i18n.md) | i18n/l10n system, gettext, translations (bwx_sdk pattern) | ✅ Complete |
| 10 | [**Branding**](10_branding.md) | Logo, colors, animals, style guide | ✅ Complete |
| 11 | [**User Docs Plan**](11_user_documentation_plan.md) | MkDocs + Material, phased delivery strategy | ✅ Complete |
| 12 | [**Dev Protocols**](12_dev_protocols.md) | Task workflow, MCP tools, session management | ✅ Complete |

### Supporting Files

- **diagrams/** - Architecture diagrams, flowcharts (SVG format)
- **../concept_files/** - Concept documents and design materials (archived & ongoing)

---

## 🚀 Quick Start for Developers

**New to the project?** Read in this order:

1. **[Overview](01_overview.md)** - Understand what Kalahari is and why
2. **[Tech Stack](02_tech_stack.md)** - Know the technologies we use
3. **[Architecture](03_architecture.md)** - Learn the high-level structure
4. **[Plugin System](04_plugin_system.md)** - Understand the plugin architecture
5. **[MVP Tasks](07_mvp_tasks.md)** - See the implementation roadmap

**Want to contribute?** Start with [ROADMAP.md](../ROADMAP.md) to see current phase and priorities.

---

## 📖 Document Conventions

### Formatting
- All documents in **English** (code, comments, docs)
- Markdown format with GitHub-flavored extensions
- Code examples in C++20 or Python 3.11
- Diagrams in SVG format (diagrams/ folder)

### Structure
- Each document is self-contained (can be read standalone)
- Cross-references use relative links
- Code examples are complete and runnable
- Diagrams have alt text for accessibility

### Status Indicators
- ✅ **Complete** - Finalized, no major changes expected
- 🔄 **In Progress** - Being actively written/updated
- ⏳ **Pending** - Waiting for external input or decision
- 📝 **Draft** - Initial version, needs review

---

## 🔄 Document History

### v1.3 - 2025-10-29
- ✅ **12_dev_protocols.md** created - Comprehensive development protocols
  - Task Management Workflow (PLAN → APPROVAL → IMPLEMENTATION)
  - Session Management Protocol (End-of-Session Checklist)
  - MCP tools usage (Serena, Context7 detailed examples)
  - wxWidgets layout protocols (sizer hierarchy, common patterns)
  - Code quality standards (C++20, testing, documentation)
- ✅ **06_roadmap.md** converted to meta-document
  - NOW: Rules for maintaining ROADMAP.md and CHANGELOG.md
  - BEFORE: Full roadmap content (moved to root /ROADMAP.md)
- ✅ **README.md** updated
  - Fixed Quick Start link (now points to /ROADMAP.md)
  - Added document #12 to core documentation table
  - Updated 06_roadmap.md description
- 🎯 **12/12 documents complete, 100% coverage**

### v1.2 - 2025-10-25
- ✅ **11_user_documentation_plan.md** completed - User docs strategy
  - MkDocs + Material Theme stack
  - 100-120 pages target (EN + PL)
  - Phased delivery aligned with development
  - Multi-language strategy (EN primary, PL MVP, +4 post-1.0)
- 📝 **Status correction:** 03, 04, 07 marked as In Progress (placeholders)
- 🎯 **8/11 documents complete, 3 in active development**

### v1.1 - 2025-10-25
- ✅ **08_gui_design.md** completed - Comprehensive GUI architecture
  - Command Registry system (unified command system for core + plugins)
  - Customizable toolbars (6 default toolbars, QAT, Live Customization Mode)
  - 4 Perspectives (Writer, Editor, Researcher, Planner)
  - Panel catalog (9 core panels + plugin support)
  - Gamification system (25+ badges, challenges, streak tracking)
  - Focus modes (Normal, Focused, Distraction-Free)
  - Command Palette (VS Code style)

### v1.0 - 2025-10-24
- 📁 Initial documentation structure created
- ✅ Migrated from concept_files/ to organized structure
- 🏗️ Finalized C++ architecture (was Python before)
- 📚 Created 10 core documents
- 🎯 Separated concerns (tech, business, design)
- ✅ 09_i18n.md based on bwx_sdk proven pattern

---

## 📞 Document Maintenance

### Who Updates These Docs?
- **Core team** - Major architectural decisions
- **Contributors** - Feature-specific sections with PR
- **Claude (AI)** - With user approval, keeps docs in sync with CLAUDE.md

### When to Update?
- ✅ After finalizing architectural decision
- ✅ After major milestone completion
- ✅ When adding/removing features from roadmap
- ✅ When plugin API changes (versioning!)

### How to Propose Changes?
1. Open issue describing the change
2. Discuss with core team
3. Create PR with updated doc(s)
4. Get approval before merge

---

## 🔗 Related Files

- **[../CLAUDE.md](../CLAUDE.md)** - Master project file (AI instructions + decisions)
- **../concept_files/** - Concept documents and design materials (archived & ongoing)
- **../.serena/project.yml** - Serena MCP configuration

---

## 📧 Questions?

If something is unclear in the documentation:
1. Check CLAUDE.md for high-level decisions
2. Open a GitHub issue with label `documentation`
3. Ask in the project discussion forum

---

**Happy coding! 🦁**
