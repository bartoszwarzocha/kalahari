## ADDED Requirements

### Requirement: Pending Format Application
The BookEditor SHALL apply pending formatting (bold, italic, font, size) to text typed after toggling format with no selection.

#### Scenario: Toggle bold then type
- **WHEN** user toggles bold with no selection, then types "hello"
- **THEN** "hello" is bold

### Requirement: Clear Formatting
The BookEditor SHALL provide a clear formatting command that removes all inline character formatting from selection.

#### Scenario: Clear bold text
- **WHEN** user selects bold red 16pt text and invokes clear formatting
- **THEN** text reverts to default font, size, color (no bold)
