"""
Runner module - Agent execution via SDK or mock.

Handles:
- Running agents via Claude Agent SDK
- Mock mode for testing
- Timeout handling
"""

import asyncio
import time
from pathlib import Path
from typing import Optional

# Try to import SDK, but don't fail if not available
try:
    from claude_agent_sdk import query, ClaudeAgentOptions, AssistantMessage, TextBlock
    SDK_AVAILABLE = True
except ImportError:
    SDK_AVAILABLE = False


class AgentRunner:
    """Execute agents via SDK or mock mode."""

    # Mock responses for each agent type
    MOCK_RESPONSES = {
        "task-manager": """
# Task Manager Report

Analyzing request...

Created new task based on requirements.

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: OpenSpec #00029 created - panel statystyk
next_hint: architect should design solution
═══════════════════════════════════════════════════════════════
""",
        "architect": """
# Architect Analysis


## Design

### Files to Modify
- src/gui/main_window.cpp - add panel registration

### New Files
- include/kalahari/gui/panels/stats_panel.h
- src/gui/panels/stats_panel.cpp

### Class Structure
- StatsPanel : QDockWidget
  - m_wordCountLabel : QLabel*
  - updateStats() : void

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: Design complete for StatsPanel
next_hint: implementation needed
═══════════════════════════════════════════════════════════════
""",
        "code-writer": """
# Code Writer Report

Created new files:
- include/kalahari/gui/panels/stats_panel.h
- src/gui/panels/stats_panel.cpp

Build: PASS

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: New files created, build successful
next_hint: code review needed
═══════════════════════════════════════════════════════════════
""",
        "code-editor": """
# Code Editor Report

Modified files:
- src/gui/main_window.cpp - added panel registration

Build: PASS

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: Files modified, build successful
next_hint: code review needed
═══════════════════════════════════════════════════════════════
""",
        "ui-designer": """
# UI Designer Report

Created UI component:
- Type: QDockWidget (panel)
- Layout: QVBoxLayout with labels
- Theme-aware colors applied

Build: PASS

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: UI component created, build successful
next_hint: code review needed
═══════════════════════════════════════════════════════════════
""",
        "code-reviewer": """
# Code Review Report

Files reviewed: 3

## Checks
- [x] Icons via ArtProvider
- [x] Strings via tr()
- [x] Naming conventions OK
- [x] No hardcoded values

Decision: APPROVE

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: Code review APPROVED
next_hint: run tests
═══════════════════════════════════════════════════════════════
""",
        "tester": """
# Test Report

Build: PASS
Tests: 42/42 passed
Duration: 2.3s

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: All tests PASS
next_hint: ready to close task
═══════════════════════════════════════════════════════════════
""",
    }

    # Alternative mock for task-manager closing
    MOCK_CLOSE_RESPONSE = """
# Task Manager - Closing Task

Verified:
- [x] All tasks complete
- [x] CHANGELOG.md updated
- [x] Tests passed

Status changed to: DEPLOYED

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: Task closed - DEPLOYED
next_hint: workflow complete
═══════════════════════════════════════════════════════════════
"""

    def __init__(
        self,
        project_dir: Path,
        mock: bool = False,
        verbose: bool = False,
        timeout: int = 300,
    ):
        """
        Initialize runner.

        Args:
            project_dir: Project root directory
            mock: Use mock mode instead of SDK
            verbose: Print verbose output
            timeout: Agent timeout in seconds
        """
        self.project_dir = project_dir
        self.mock = mock
        self.verbose = verbose
        self.timeout = timeout
        self._call_count = {}  # Track calls per agent for varied responses

        if not mock and not SDK_AVAILABLE:
            print("[!] claude-agent-sdk not installed. Falling back to mock mode.")
            print("    Install with: pip install claude-agent-sdk")
            self.mock = True

    async def run(self, agent: str, prompt: str) -> str:
        """
        Run an agent with the given prompt.

        Args:
            agent: Agent name (e.g., "task-manager", "architect")
            prompt: Prompt to send to agent

        Returns:
            Agent's output as string
        """
        if self.mock:
            return await self._mock_run(agent, prompt)
        else:
            return await self._sdk_run(agent, prompt)

    async def _mock_run(self, agent: str, prompt: str) -> str:
        """
        Mock agent execution for testing.
        """
        # Track call count for this agent
        self._call_count[agent] = self._call_count.get(agent, 0) + 1
        call_num = self._call_count[agent]

        if self.verbose:
            print(f"  [MOCK] Agent: {agent}")
            print(f"  [MOCK] Prompt: {prompt[:100]}...")

        # Simulate some delay
        await asyncio.sleep(0.5)

        # Special case: task-manager closing (second call)
        if agent == "task-manager" and call_num > 1:
            return self.MOCK_CLOSE_RESPONSE

        # Return mock response for agent
        response = self.MOCK_RESPONSES.get(agent, f"""
# {agent} Response

Executed task.

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: {agent} completed
next_hint: continue workflow
═══════════════════════════════════════════════════════════════
""")

        return response

    async def _sdk_run(self, agent: str, prompt: str) -> str:
        """
        Run agent via Claude Agent SDK.

        Uses the Task tool to spawn a subagent of the specified type.
        """
        if self.verbose:
            print(f"  [SDK] Agent: {agent}")
            print(f"  [SDK] Prompt: {prompt[:100]}...")

        # Build prompt that instructs Claude to use the Task tool with specific subagent
        full_prompt = f"""Use the Task tool to spawn a '{agent}' subagent with this prompt:

{prompt}

IMPORTANT:
- Use subagent_type="{agent}"
- Wait for the agent to complete and return its full output
- The agent MUST end its response with a [WORKFLOW_STATUS] block"""

        options = ClaudeAgentOptions(
            cwd=str(self.project_dir),
            permission_mode="acceptEdits",
            allowed_tools=["Task", "Read", "Write", "Edit", "Bash", "Glob", "Grep"],
        )

        result_parts = []

        try:
            async with asyncio.timeout(self.timeout):
                async for message in query(prompt=full_prompt, options=options):
                    # Handle AssistantMessage with content blocks
                    if isinstance(message, AssistantMessage):
                        for block in message.content:
                            if isinstance(block, TextBlock):
                                result_parts.append(block.text)
                    # Fallback for other message types
                    elif hasattr(message, "content"):
                        for block in message.content:
                            if hasattr(block, "text"):
                                result_parts.append(block.text)
        except asyncio.TimeoutError:
            return f"""
===============================================================
[WORKFLOW_STATUS]
status: FAILED
context: Agent timeout after {self.timeout} seconds
next_hint: check agent configuration
===============================================================
"""
        except Exception as e:
            return f"""
===============================================================
[WORKFLOW_STATUS]
status: FAILED
context: SDK error - {str(e)}
next_hint: check SDK installation and configuration
===============================================================
"""

        return "\n".join(result_parts)
