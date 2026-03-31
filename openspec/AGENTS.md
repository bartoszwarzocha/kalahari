# OpenSpec Conventions — Kalahari

## Change Structure

```
openspec/changes/NNNNN-name/
├── proposal.md      # Goal, scope, acceptance criteria, status
├── tasks.md         # Hierarchical checklist (max 15-25 items)
└── specs/           # Delta specs (ADDED/MODIFIED/REMOVED requirements)
    └── <area>.md
```

## Rules

- **Numbering:** 5-digit with leading zeros (00001, 00045)
- **Task sizing:** Max 15-25 tasks per OpenSpec. Split larger features.
- **Naming:** `NNNNN-verb-description` (e.g. `00046-add-spell-checker`)
- **Status lifecycle:** `PENDING → IN_PROGRESS → DEPLOYED → archived`
- **No code during proposal** — only design documents
- **Delta specs required** — document what changes in requirements

## Delta Spec Format

```markdown
## ADDED Requirements
### Requirement: <Name>
The system SHALL ...

#### Scenario: <Success case>
- **WHEN** user performs action
- **THEN** expected result

## MODIFIED Requirements
### Requirement: <Existing Name>
Full updated requirement text (replaces original).

## REMOVED Requirements
### Requirement: <Name>
**Reason**: Why removing.
```

Every requirement MUST have at least one `#### Scenario:`.

## Workflow

| Command | Purpose |
|---------|---------|
| `/openspec:explore` | Discuss idea before committing |
| `/openspec:proposal` | Create change with delta specs |
| `/openspec:apply` | Implement tasks |
| `/openspec:verify` | Validate implementation vs spec |
| `/openspec:archive` | Merge deltas into openspec/specs/, move to archive |

## Main Specs

`openspec/specs/` is the source of truth — living documentation built from archived delta specs.

## Best Practices

- Default to <100 lines of new code per task
- Single-file implementations until proven insufficient
- Prefer modifying existing specs over creating duplicates
- Use `file.cpp:42` format for code references
