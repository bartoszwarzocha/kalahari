"""
State module - Workflow execution state tracking.

Handles:
- Recording execution history
- Tracking retry counts per rule
- Detecting loops and limits
- Generating execution summary
"""

import json
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional, Any


@dataclass
class ExecutionRecord:
    """Record of a single agent execution."""

    agent: str
    prompt: str
    status: str
    context: str
    source: str
    timestamp: str
    duration_seconds: float
    rule_id: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "agent": self.agent,
            "prompt": self.prompt[:200] + "..." if len(self.prompt) > 200 else self.prompt,
            "status": self.status,
            "context": self.context,
            "source": self.source,
            "timestamp": self.timestamp,
            "duration_seconds": self.duration_seconds,
            "rule_id": self.rule_id,
        }


class WorkflowState:
    """Track workflow execution state."""

    def __init__(self, limits: Dict[str, Any]):
        """
        Initialize state with limits from config.

        Args:
            limits: Limits section from workflow.json
        """
        self.limits = limits
        self.iteration: int = 0
        self.history: List[ExecutionRecord] = []
        self.retry_counts: Dict[str, int] = {}
        self.complete: bool = False
        self.failed: bool = False
        self.start_time: datetime = datetime.now()
        self.last_context: str = ""

    def record(
        self,
        agent: str,
        prompt: str,
        status: "WorkflowStatus",
        duration: float,
        rule_id: Optional[str] = None,
    ):
        """
        Record an agent execution.

        Args:
            agent: Agent name
            prompt: Prompt sent to agent
            status: Parsed WorkflowStatus
            duration: Execution time in seconds
            rule_id: ID of the matched rule
        """
        self.history.append(
            ExecutionRecord(
                agent=agent,
                prompt=prompt,
                status=status.status,
                context=status.context,
                source=status.source,
                timestamp=datetime.now().isoformat(),
                duration_seconds=duration,
                rule_id=rule_id,
            )
        )
        self.iteration += 1
        self.last_context = status.context

    def increment_retry(self, rule_id: str):
        """Increment retry count for a rule."""
        self.retry_counts[rule_id] = self.retry_counts.get(rule_id, 0) + 1

    def get_retry_count(self, rule_id: str) -> int:
        """Get current retry count for a rule."""
        return self.retry_counts.get(rule_id, 0)

    def can_retry(self, rule_id: str, max_retries: int) -> bool:
        """Check if retry is allowed for a rule."""
        return self.get_retry_count(rule_id) < max_retries

    def is_at_limit(self) -> bool:
        """Check if max iterations reached."""
        max_iter = self.limits.get("max_workflow_iterations", 20)
        return self.iteration >= max_iter

    def is_in_loop(self, lookback: int = 6) -> bool:
        """
        Detect if we're stuck in a loop.

        Checks if the same 2-agent pattern repeats.
        """
        if len(self.history) < lookback:
            return False

        recent = [r.agent for r in self.history[-lookback:]]

        # Check for A-B-A-B pattern
        if len(recent) >= 4:
            if (
                recent[-1] == recent[-3]
                and recent[-2] == recent[-4]
                and recent[-1] != recent[-2]
            ):
                return True

        # Check for A-A-A pattern (same agent 3 times)
        if len(recent) >= 3:
            if recent[-1] == recent[-2] == recent[-3]:
                return True

        return False

    def get_agents_used(self) -> List[str]:
        """Get list of unique agents used."""
        return list(set(r.agent for r in self.history))

    def get_total_duration(self) -> float:
        """Get total execution time."""
        return sum(r.duration_seconds for r in self.history)

    def get_last_status(self) -> Optional[str]:
        """Get the last recorded status."""
        if self.history:
            return self.history[-1].status
        return None

    def summary(self) -> str:
        """Generate human-readable summary."""
        agents = self.get_agents_used()
        total_time = self.get_total_duration()
        last_status = self.get_last_status() or "N/A"

        status_icon = "[OK]" if self.complete else ("[XX]" if self.failed else "[..]")

        lines = [
            "",
            "=" * 65,
            "                    WORKFLOW SUMMARY                            ",
            "=" * 65,
            f"  Status:      {status_icon} {'COMPLETE' if self.complete else ('FAILED' if self.failed else 'STOPPED')}",
            f"  Iterations:  {self.iteration}",
            f"  Agents used: {', '.join(agents) if agents else 'None'}",
            f"  Total time:  {total_time:.1f}s",
            f"  Last status: {last_status}",
            "",
            "  Execution trace:",
        ]

        for i, record in enumerate(self.history, 1):
            status_char = {
                "READY": "+",
                "BLOCKED": "!",
                "FAILED": "X",
                "DECISION_NEEDED": "?",
                "UNKNOWN": ".",
            }.get(record.status, ".")

            lines.append(
                f"    {i}. [{status_char}] {record.agent} -> {record.status} ({record.duration_seconds:.1f}s)"
            )

        lines.append("=" * 65)
        lines.append("")

        return "\n".join(lines)

    def save_log(self, log_dir: Path):
        """
        Save execution log to JSON file.

        Args:
            log_dir: Directory to save log file
        """
        log_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = log_dir / f"workflow-{timestamp}.json"

        log_data = {
            "start_time": self.start_time.isoformat(),
            "end_time": datetime.now().isoformat(),
            "iterations": self.iteration,
            "complete": self.complete,
            "failed": self.failed,
            "agents_used": self.get_agents_used(),
            "total_duration_seconds": self.get_total_duration(),
            "retry_counts": self.retry_counts,
            "history": [r.to_dict() for r in self.history],
        }

        with open(log_file, "w", encoding="utf-8") as f:
            json.dump(log_data, f, indent=2, ensure_ascii=False)

        return log_file
