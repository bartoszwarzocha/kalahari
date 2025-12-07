#!/usr/bin/env python3
"""
Kalahari Workflow Orchestrator - Main Entry Point

Usage:
    python -m orchestrator.main "nowe zadanie - panel statystyk"
    python -m orchestrator.main --mock "nowe zadanie - panel statystyk"
    python -m orchestrator.main --verbose --mock "test task"
"""

import argparse
import asyncio
import json
import sys
import time
from pathlib import Path
from typing import Optional, Dict, Any

from .protocol import StatusParser, PromptInjector, WorkflowStatus
from .engine import RuleEngine, RuleMatch
from .state import WorkflowState
from .runner import AgentRunner
from .ui import WorkflowUI


def load_config(config_path: Path) -> Dict[str, Any]:
    """Load workflow configuration from JSON file."""
    if not config_path.exists():
        raise FileNotFoundError(f"Workflow config not found: {config_path}")

    with open(config_path, "r", encoding="utf-8") as f:
        return json.load(f)


class WorkflowOrchestrator:
    """Main orchestrator class that runs the workflow."""

    AVAILABLE_AGENTS = [
        "task-manager",
        "architect",
        "code-writer",
        "code-editor",
        "ui-designer",
        "code-reviewer",
        "tester",
    ]

    def __init__(
        self,
        project_dir: Path,
        config: Dict[str, Any],
        mock: bool = False,
        verbose: bool = False,
    ):
        """
        Initialize orchestrator.

        Args:
            project_dir: Project root directory
            config: Loaded workflow configuration
            mock: Use mock mode
            verbose: Print verbose output
        """
        self.project_dir = project_dir
        self.config = config.get("workflow", config)
        self.mock = mock
        self.verbose = verbose

        # Initialize components
        protocol_config = self.config.get("protocol", {})
        limits_config = self.config.get("limits", {})
        rules = self.config.get("rules", [])
        injection_config = self.config.get("prompt_injection", {})

        self.parser = StatusParser(protocol_config)
        self.injector = PromptInjector(enabled=injection_config.get("enabled", True))
        self.engine = RuleEngine(rules)
        self.state = WorkflowState(limits_config)
        self.runner = AgentRunner(
            project_dir=project_dir,
            mock=mock,
            verbose=verbose,
            timeout=limits_config.get("agent_timeout_seconds", 300),
        )
        self.ui = WorkflowUI(use_colors=True)

    async def run(self, initial_prompt: str) -> bool:
        """
        Run the workflow.

        Args:
            initial_prompt: Initial task description

        Returns:
            True if workflow completed successfully
        """
        mode_str = "MOCK MODE" if self.mock else "SDK MODE"
        self.ui.print_header(f"KALAHARI WORKFLOW ORCHESTRATOR ({mode_str})")

        # Check if session state file exists
        session_file = self.project_dir / ".claude" / "session-state.json"
        session_file_exists = session_file.exists()

        if session_file_exists:
            self.ui.print_info("Session state detected - checking for restore...")

        # Find initial rule (session_start has priority if session exists)
        initial_rule = self.engine.find_initial(
            context=initial_prompt,
            session_file_exists=session_file_exists
        )
        if not initial_rule:
            self.ui.print_error("No initial rule found in workflow.json")
            return False

        # Start workflow
        current_agent = initial_rule.get("action", {}).get("agent", "task-manager")
        current_prompt = initial_prompt
        current_rule_id = initial_rule.get("id", "initial")

        try:
            while True:
                # Check limits
                if self.state.is_at_limit():
                    if not self.ui.ask_continue_after_limit("Max iterations reached"):
                        self.state.failed = True
                        break

                if self.state.is_in_loop():
                    if not self.ui.ask_continue_after_limit("Loop detected"):
                        self.state.failed = True
                        break

                self.state.iteration += 1

                # Print iteration header
                self.ui.print_iteration(
                    self.state.iteration, current_agent, self.mock
                )

                # Prepare prompt with protocol injection
                full_prompt = self.injector.inject(current_prompt)

                if self.verbose:
                    print(f"  Prompt: {current_prompt[:100]}...")

                # Run agent
                start_time = time.time()
                output = await self.runner.run(current_agent, full_prompt)
                duration = time.time() - start_time

                if self.verbose:
                    self.ui.print_output_preview(output)

                # Parse status
                status = self.parser.parse(output)
                self.ui.print_status(status.status, status.context, status.source)

                # Record execution (subtract 1 because we already incremented)
                self.state.iteration -= 1
                self.state.record(
                    agent=current_agent,
                    prompt=current_prompt,
                    status=status,
                    duration=duration,
                    rule_id=current_rule_id,
                )

                # Handle FAILED status
                if status.status == "FAILED":
                    self.ui.print_error(f"Agent failed: {status.context}")
                    self.state.failed = True
                    break

                # Handle UNKNOWN status
                if status.status == "UNKNOWN":
                    result = self.ui.ask_fallback(self.AVAILABLE_AGENTS)
                    if result is None:
                        break
                    current_agent, current_prompt = result
                    current_rule_id = "manual"
                    continue

                # Find matching rule
                match = self.engine.match(current_agent, status)

                if not match:
                    self.ui.print_no_match()
                    result = self.ui.ask_fallback(self.AVAILABLE_AGENTS)
                    if result is None:
                        break
                    current_agent, current_prompt = result
                    current_rule_id = "manual"
                    continue

                self.ui.print_rule_match(
                    match.rule_id,
                    match.rule.get("description", ""),
                )

                action = match.action

                # Handle complete action
                if action.get("type") == "complete":
                    self.ui.print_complete(action.get("message", "Workflow complete!"))
                    self.state.complete = True
                    break

                # Handle decision action
                if action.get("type") == "decision":
                    choice = self.ui.ask_decision(
                        action.get("message", "Choose next action:"),
                        action.get("options", []),
                    )
                    if choice is None:
                        break

                    current_agent = choice.get("agent", "task-manager")
                    prompt_template = action.get("prompt_template", "{context}")
                    current_prompt = prompt_template.replace(
                        "{context}", status.context
                    )
                    current_rule_id = match.rule_id + "_decision"
                    continue

                # Handle retry limits
                if match.has_retry:
                    if not self.state.can_retry(match.rule_id, match.max_retries):
                        retry_config = match.rule.get("retry", {})
                        on_exhausted = retry_config.get("on_exhausted", {})

                        if on_exhausted.get("type") == "ask_user":
                            self.ui.print_error(
                                on_exhausted.get("message", "Retry limit reached")
                            )
                            result = self.ui.ask_fallback(self.AVAILABLE_AGENTS)
                            if result is None:
                                break
                            current_agent, current_prompt = result
                            current_rule_id = "manual"
                            continue
                        else:
                            self.state.failed = True
                            break

                    self.state.increment_retry(match.rule_id)

                # Normal transition
                current_agent = action.get("agent", current_agent)
                prompt_template = action.get("prompt", "Continue workflow")
                current_prompt = prompt_template.replace("{context}", status.context)
                current_rule_id = match.rule_id

        except KeyboardInterrupt:
            print("\n")
            self.ui.print_error("Interrupted by user")
            self.state.failed = True

        # Print summary
        print(self.state.summary())

        # Save log
        log_dir = self.project_dir / ".claude" / "logs"
        log_file = self.state.save_log(log_dir)
        print(f"  Log saved to: {log_file}")

        return self.state.complete


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Kalahari Workflow Orchestrator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python -m orchestrator.main "nowe zadanie - panel statystyk"
    python -m orchestrator.main --mock "test task"
    python -m orchestrator.main --verbose --mock "test"
        """,
    )

    parser.add_argument(
        "prompt",
        nargs="?",
        help="Initial task description",
    )

    parser.add_argument(
        "--mock",
        action="store_true",
        help="Use mock mode (no real agent calls)",
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Print verbose output",
    )

    parser.add_argument(
        "--config",
        type=Path,
        default=None,
        help="Path to workflow.json (default: .claude/workflow.json)",
    )

    parser.add_argument(
        "--project-dir",
        type=Path,
        default=None,
        help="Project directory (default: current directory)",
    )

    args = parser.parse_args()

    # Determine project directory
    if args.project_dir:
        project_dir = args.project_dir.resolve()
    else:
        # Try to find project root by looking for .claude directory
        cwd = Path.cwd()
        if (cwd / ".claude").exists():
            project_dir = cwd
        elif (cwd.parent / ".claude").exists():
            project_dir = cwd.parent
        else:
            project_dir = cwd

    # Determine config path
    if args.config:
        config_path = args.config
    else:
        config_path = project_dir / ".claude" / "workflow.json"

    # Get prompt
    if args.prompt:
        prompt = args.prompt
    else:
        try:
            prompt = input("Task description: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nCancelled.")
            sys.exit(1)

        if not prompt:
            print("Error: No task description provided")
            sys.exit(1)

    # Load config
    try:
        config = load_config(config_path)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error parsing workflow.json: {e}")
        sys.exit(1)

    # Create and run orchestrator
    orchestrator = WorkflowOrchestrator(
        project_dir=project_dir,
        config=config,
        mock=args.mock,
        verbose=args.verbose,
    )

    success = asyncio.run(orchestrator.run(prompt))
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
