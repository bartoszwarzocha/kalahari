"""
Engine module - Rule matching for workflow transitions.

Handles:
- Finding initial rule to start workflow
- Matching rules based on agent and status
- Extracting captured groups from context patterns
"""

import re
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any


@dataclass
class RuleMatch:
    """Result of matching a rule."""

    rule: Dict[str, Any]
    captured: Dict[str, Any] = field(default_factory=dict)

    @property
    def rule_id(self) -> str:
        return self.rule.get("id", "unknown")

    @property
    def action(self) -> Dict[str, Any]:
        return self.rule.get("action", {})

    @property
    def has_retry(self) -> bool:
        return "retry" in self.rule

    @property
    def max_retries(self) -> int:
        return self.rule.get("retry", {}).get("max", 3)


class RuleEngine:
    """Match rules based on agent output and status."""

    def __init__(self, rules: List[Dict[str, Any]]):
        """
        Initialize engine with rules from workflow.json.

        Args:
            rules: List of rule dictionaries
        """
        self.rules = rules

    def find_initial(self, context: str = "", session_file_exists: bool = False) -> Optional[Dict[str, Any]]:
        """
        Find the initial rule to start the workflow.

        Checks rules in priority order:
        1. session_start (if session file exists)
        2. start (default initial)

        Args:
            context: Current conversation context (for pattern matching)
            session_file_exists: Whether .claude/session-state.json exists

        Returns:
            Matching initial rule, or None
        """
        # Sort rules by priority (higher first)
        sorted_rules = sorted(
            self.rules,
            key=lambda r: r.get("priority", 0),
            reverse=True
        )

        for rule in sorted_rules:
            trigger = rule.get("trigger", {})
            trigger_type = trigger.get("type")

            # Check session_start trigger
            if trigger_type == "session_start":
                requires_file = trigger.get("requires_session_file", False)
                if requires_file and session_file_exists:
                    return rule
                elif not requires_file:
                    # Check for pattern in context
                    pattern = trigger.get("pattern", "")
                    if pattern and pattern in context:
                        return rule

            # Check standard start trigger
            elif trigger_type == "start":
                return rule

        return None

    def match(self, agent: str, status: "WorkflowStatus") -> Optional[RuleMatch]:
        """
        Find matching rule for agent and status.

        Args:
            agent: Name of the agent that just completed
            status: WorkflowStatus from the agent's output

        Returns:
            RuleMatch if found, None otherwise
        """
        from .protocol import WorkflowStatus  # Avoid circular import

        for rule in self.rules:
            trigger = rule.get("trigger", {})

            # Skip initial rules (already handled)
            trigger_type = trigger.get("type")
            if trigger_type in ("start", "session_start", "session", "sesja"):
                continue

            # Check agent match
            if not self._agent_matches(trigger, agent):
                continue

            # Check status match
            if not self._status_matches(trigger, status):
                continue

            # Check context pattern (optional)
            captured = self._check_context_pattern(trigger, status)
            if captured is None:
                continue  # Pattern required but didn't match

            return RuleMatch(rule=rule, captured=captured)

        return None

    def _agent_matches(self, trigger: Dict, agent: str) -> bool:
        """Check if agent matches trigger."""
        trigger_agent = trigger.get("agent")

        if trigger_agent is None:
            return True  # No agent filter

        if isinstance(trigger_agent, list):
            return agent in trigger_agent

        return trigger_agent == agent

    def _status_matches(self, trigger: Dict, status: "WorkflowStatus") -> bool:
        """Check if status matches trigger."""
        trigger_status = trigger.get("status")

        if trigger_status is None:
            return True  # No status filter

        return trigger_status == status.status

    def _check_context_pattern(
        self, trigger: Dict, status: "WorkflowStatus"
    ) -> Optional[Dict]:
        """
        Check context_contains and context_excludes patterns.

        Returns:
            - Empty dict if no pattern required
            - Dict with match groups if pattern matched
            - None if pattern required but didn't match or exclusion matched
        """
        # Check exclusion pattern first
        exclude_pattern = trigger.get("context_excludes")
        if exclude_pattern:
            if re.search(exclude_pattern, status.context, re.IGNORECASE):
                return None  # Exclusion matched, skip this rule

        # Check inclusion pattern
        context_pattern = trigger.get("context_contains")

        if context_pattern is None:
            return {}  # No pattern required

        match = re.search(context_pattern, status.context, re.IGNORECASE)
        if match:
            return {"match": match, "groups": match.groups()}

        return None  # Pattern didn't match

    def get_rule_by_id(self, rule_id: str) -> Optional[Dict[str, Any]]:
        """
        Get rule by its ID.

        Args:
            rule_id: The rule's id field

        Returns:
            Rule dict or None
        """
        for rule in self.rules:
            if rule.get("id") == rule_id:
                return rule
        return None
