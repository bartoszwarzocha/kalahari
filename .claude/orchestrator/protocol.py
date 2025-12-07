"""
Protocol module - Status parsing and prompt injection.

Handles:
- Parsing [WORKFLOW_STATUS] blocks from agent output
- Fallback pattern matching when explicit status not found
- Injecting workflow protocol instructions into prompts
"""

import re
from dataclasses import dataclass
from typing import Literal, Optional


@dataclass
class WorkflowStatus:
    """Represents the parsed status from an agent's output."""

    status: Literal["READY", "BLOCKED", "FAILED", "DECISION_NEEDED", "UNKNOWN"]
    context: str = ""
    next_hint: str = ""
    source: Literal["explicit", "fallback", "user"] = "explicit"

    def __str__(self) -> str:
        return f"WorkflowStatus({self.status}, context='{self.context[:50]}...', source={self.source})"


class StatusParser:
    """Parse agent output to extract workflow status."""

    def __init__(self, protocol_config: dict):
        """
        Initialize parser with protocol configuration.

        Args:
            protocol_config: Protocol section from workflow.json
        """
        self.marker = protocol_config.get("status_block_marker", "[WORKFLOW_STATUS]")
        self.valid_statuses = protocol_config.get("valid_statuses",
            ["READY", "BLOCKED", "FAILED", "DECISION_NEEDED"])
        self.fallback_patterns = protocol_config.get("fallback_patterns", {})
        self.pattern_priority = protocol_config.get("pattern_priority",
            ["FAILED", "BLOCKED", "READY"])
        self.search_lines = protocol_config.get("fallback_search_lines", 10)

    def parse(self, output: str) -> WorkflowStatus:
        """
        Parse agent output to extract workflow status.

        Strategy:
        1. Try to find explicit [WORKFLOW_STATUS] block
        2. Fallback to pattern matching in last N lines
        3. Return UNKNOWN if nothing found

        Args:
            output: Raw output from agent

        Returns:
            WorkflowStatus with parsed data
        """
        # 1. Try explicit status block
        explicit = self._parse_explicit_block(output)
        if explicit:
            # Validate status
            if explicit.status in self.valid_statuses:
                return explicit
            # Invalid status treated as UNKNOWN
            return WorkflowStatus(
                status="UNKNOWN",
                context=f"Invalid status '{explicit.status}' in output",
                source="explicit"
            )

        # 2. Fallback to pattern matching
        fallback = self._parse_fallback_patterns(output)
        if fallback:
            return fallback

        # 3. Nothing found
        return WorkflowStatus(
            status="UNKNOWN",
            context="No status found in output",
            source="fallback"
        )

    def _parse_explicit_block(self, output: str) -> Optional[WorkflowStatus]:
        """
        Parse explicit [WORKFLOW_STATUS] block.

        Expected format:
        [WORKFLOW_STATUS]
        status: READY
        context: Some context
        next_hint: Some hint
        """
        # Pattern to match the status block
        # Using non-greedy matching and multiline
        block_pattern = r"\[WORKFLOW_STATUS\][\s\S]*?status:\s*(\w+)"

        match = re.search(block_pattern, output, re.IGNORECASE)
        if not match:
            return None

        status = match.group(1).upper()

        # Try to extract context
        context = ""
        context_match = re.search(
            r"\[WORKFLOW_STATUS\][\s\S]*?context:\s*(.+?)(?:\n|$)",
            output,
            re.IGNORECASE
        )
        if context_match:
            context = context_match.group(1).strip()

        # Try to extract next_hint
        next_hint = ""
        hint_match = re.search(
            r"\[WORKFLOW_STATUS\][\s\S]*?next_hint:\s*(.+?)(?:\n|$)",
            output,
            re.IGNORECASE
        )
        if hint_match:
            next_hint = hint_match.group(1).strip()

        return WorkflowStatus(
            status=status,
            context=context,
            next_hint=next_hint,
            source="explicit"
        )

    def _parse_fallback_patterns(self, output: str) -> Optional[WorkflowStatus]:
        """
        Search for status patterns in the last N lines of output.

        Uses pattern_priority to check FAILED first, then BLOCKED, then READY.
        This is a pessimistic approach - we assume failure if ambiguous.
        """
        # Get last N lines
        lines = output.strip().split("\n")
        last_lines = "\n".join(lines[-self.search_lines:])

        # Check patterns in priority order
        for status in self.pattern_priority:
            pattern = self.fallback_patterns.get(status, "")
            if pattern:
                if re.search(pattern, last_lines, re.IGNORECASE):
                    return WorkflowStatus(
                        status=status,
                        context=f"Detected via fallback pattern: {pattern}",
                        source="fallback"
                    )

        return None


class PromptInjector:
    """Inject workflow protocol instructions into agent prompts."""

    PROTOCOL_TEMPLATE = '''

───────────────────────────────────────────────────────────────
[WORKFLOW_PROTOCOL]

When you complete your task, END your response with this block:

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: <STATUS>
context: <brief outcome description>
next_hint: <suggested next step>
═══════════════════════════════════════════════════════════════

Valid STATUS values:
• READY           - Task completed successfully
• BLOCKED         - Issue found, needs fixing (describe in context)
• FAILED          - Critical error, cannot proceed
• DECISION_NEEDED - Multiple paths, user must choose

Example:
═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: OpenSpec #00028 created successfully
next_hint: architect should design solution
═══════════════════════════════════════════════════════════════
───────────────────────────────────────────────────────────────
'''

    def __init__(self, enabled: bool = True):
        """
        Initialize injector.

        Args:
            enabled: Whether to inject protocol (can be disabled)
        """
        self.enabled = enabled

    def inject(self, prompt: str) -> str:
        """
        Inject workflow protocol into prompt.

        Args:
            prompt: Original prompt

        Returns:
            Prompt with protocol instructions appended
        """
        if not self.enabled:
            return prompt

        return prompt + self.PROTOCOL_TEMPLATE
