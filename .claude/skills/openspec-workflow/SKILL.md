---
name: openspec-workflow
description: OpenSpec task management workflow. Use for creating, tracking, and closing tasks.
---

# OpenSpec Workflow

## Structure

```
openspec/
├── specs/                     # Main specs — living documentation (source of truth)
│   └── <area>.md
├── changes/                   # Active changes
│   └── NNNNN-name/
│       ├── proposal.md        # Goal, scope, acceptance criteria
│       ├── tasks.md           # Hierarchical checklist (max 15-25 items)
│       └── specs/             # Delta specs (what changes in requirements)
│           └── <area>.md
├── archive/                   # Completed changes
├── project.md                 # Project context
└── AGENTS.md                  # Conventions
```

Numbering: 5-digit with leading zeros. Find last: `ls openspec/changes/ | sort -r | head -1`

## Task Sizing (CRITICAL)

**Max 15-25 tasks per OpenSpec.** If a feature needs more, split into multiple OpenSpecs.

Each task should be completable in one session. Hierarchical numbering: 1.1, 1.2, 2.1.

## Delta Specs

Each change includes `specs/` with delta operations:
```markdown
## ADDED Requirements
### Requirement: <Name>
Description.

## MODIFIED Requirements
### Requirement: <Existing Name>
Updated description.

## REMOVED Requirements
### Requirement: <Name>
Reason for removal.
```

At archive, deltas merge into `openspec/specs/` (order: RENAMED → REMOVED → MODIFIED → ADDED).

## Lifecycle

`PENDING → IN_PROGRESS → DEPLOYED → archived`

## Commands

| Command | Purpose |
|---------|---------|
| `/openspec:explore` | Discuss idea before committing |
| `/openspec:proposal` | Create formal change with delta specs |
| `/openspec:apply` | Implement tasks |
| `/openspec:verify` | Validate implementation vs spec |
| `/openspec:archive` | Merge deltas, move to archive |
