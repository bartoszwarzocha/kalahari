## ADDED Requirements

### Requirement: Create Style from Selection
The user SHALL be able to create a named style from the current selection's formatting.

#### Scenario: Create "My Quote" style
- **WHEN** user formats text as 14pt italic indented, then invokes Create Style
- **THEN** dialog captures formatting, user names it "My Quote", style is saved

### Requirement: Style Palette Widget
The application SHALL provide a style palette listing built-in and user-defined styles.

#### Scenario: Apply user style
- **WHEN** user clicks "My Quote" in style palette
- **THEN** selected text/paragraph reformats to "My Quote" style

### Requirement: Style Persistence
User-defined styles SHALL persist in the project database across sessions.

#### Scenario: Reopen project
- **WHEN** user closes and reopens project
- **THEN** "My Quote" style appears in style palette
