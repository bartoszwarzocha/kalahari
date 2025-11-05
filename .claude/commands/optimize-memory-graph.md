---
description: Optimize Memory MCP knowledge graph by consolidating entities and cleaning relations
---

**Smart Memory MCP graph optimization**

**When to use:**

- Knowledge graph grows too large (>60 entities)
- Duplicate entities detected (e.g., "Kalahari" + "Kalahari Project")
- Outdated session/task entities accumulate
- Query performance degrades
- After major milestones (phase completion, architecture changes)

**What this command does:**

1. **Analyze graph structure:**
   - Count entities by type (Project, Task, Technology, Decision, etc.)
   - Detect duplicates (similar names, overlapping observations)
   - Identify outdated entities (old sessions, completed tasks, obsolete decisions)
   - Calculate observations per entity (flag entities with >10 observations)
   - Check relation integrity (orphan relations, circular dependencies)
   - Report findings with statistics

2. **Identify optimization candidates:**
   - **Duplicate entities:** Same concept, different names
     - Example: "Kalahari" + "Kalahari Project" → merge into single entity
   - **Outdated sessions:** Old WorkSession entities (keep only 2 most recent)
     - Example: "Session-2025-10-26-CI-Fix" (obsolete after completion)
   - **Obsolete tasks:** Completed tasks older than 1 month
     - Example: "Task-00000-Fix-Linux-CI" (completed, archived)
   - **Bloated observations:** Entities with 10+ observations (consolidate to 5-7 key facts)
     - Example: "Phase 0 Foundation" has 15 observations (reduce to 7)
   - **Orphan relations:** Relations pointing to non-existent entities
   - Present optimization plan with before/after graph structure

3. **Execute approved optimization:**
   - **Merge duplicates:**
     - Combine observations from both entities
     - Update all relations pointing to old entity
     - Delete old entity
     - Example: Merge "Kalahari" → "Kalahari Project"

   - **Delete outdated entities:**
     - Remove old session entities (keep 2 most recent)
     - Remove completed task entities (older than 1 month)
     - Clean up all relations involving deleted entities

   - **Consolidate observations:**
     - Keep 5-7 most important observations per entity
     - Move historical details to Serena memory files
     - Prioritize: current state > historical details

   - **Clean relations:**
     - Remove orphan relations (no matching entity)
     - Remove duplicate relations (same from/to/type)
     - Remove circular relations if logically invalid

4. **Safety rules (NEVER violate):**
   - ❌ NEVER delete without user approval
   - ❌ NEVER delete core entities (Project, Technology, Architecture Pattern)
   - ❌ NEVER delete recent entities (created in last 7 days)
   - ❌ NEVER merge entities with different entityTypes
   - ✅ ALWAYS preserve critical decisions and architecture info
   - ✅ ALWAYS update relations when merging entities
   - ✅ ALWAYS verify graph integrity after changes
   - ✅ ALWAYS report what was changed and why

5. **Generate optimization report:**
   - Entities merged: [count] ([list pairs])
   - Entities deleted: [count] ([list names with types])
   - Observations consolidated: [count reduced] (X entities affected)
   - Relations cleaned: [count removed] ([orphans/duplicates/invalid])
   - Graph reduction: [before entities] → [after entities] (-XX%)
   - Estimated query performance improvement: [qualitative]
   - Graph integrity: ✅ VERIFIED (all relations valid)

**Example workflow:**

```text
User: /optimize-memory-graph

Agent:
📊 Memory Graph Analysis:
- Total: 79 entities, 138 relations
- Entity types: 15 Project, 12 Task, 8 Technology, 10 Decision, 34 other
- Duplicates detected: 3 pairs
- Outdated entities: 8 (old sessions/tasks)
- Bloated observations: 12 entities (10+ observations each)
- Orphan relations: 5

🧹 Optimization Plan:
1. Merge duplicates:
   - "Kalahari" + "Kalahari Project" → "Kalahari Project"
   - "Phase 0 Foundation" + "Phase 0 Week 2" → "Phase 0 Foundation"
   - "Testing Strategy" + "Kalahari Testing Infrastructure" → "Testing Strategy"

2. Delete outdated entities (8 total):
   - Sessions: Session-2025-10-26-CI-Fix, Phase 0 Week 3-4 Session Complete
   - Tasks: Task-00000-Fix-Linux-CI, Task-Management-Workflow (obsolete)
   - Old milestones: Phase 0 Week 2 (merged into Phase 0 Foundation)

3. Consolidate observations (12 entities affected):
   - "Phase 0 Foundation": 15 obs → 7 obs (remove historical details)
   - "Kalahari Project": 20 obs → 10 obs (keep current status only)
   - "bwx_sdk": 10 obs → 5 obs (core facts only)
   - (9 more entities...)

4. Clean relations:
   - Remove 5 orphan relations (pointing to deleted entities)
   - Remove 2 duplicate relations

Expected result: 79 → 65 entities (-18%), 138 → 130 relations (-6%)

Proceed? (User confirms)

✅ Optimization Complete:
- Merged: 3 entity pairs
- Deleted: 8 outdated entities
- Consolidated: 45 observations removed from 12 entities
- Relations cleaned: 7 removed (5 orphans, 2 duplicates)
- Result: 65 entities, 130 relations
- Graph integrity: ✅ VERIFIED
- Performance: Estimated 20-25% faster queries
```

**Technical implementation:**

1. Use `mcp__memory__read_graph` to get full graph state
2. Analyze entity names, types, observations for duplicates/bloat
3. Parse relations to detect orphans and invalid connections
4. Generate optimization plan with specific entity/relation changes
5. Wait for user approval ("Wykonaj", "Proceed", "Yes")
6. Execute changes using Memory MCP tools:
   - `mcp__memory__delete_entities` (outdated entities)
   - `mcp__memory__add_observations` (merge duplicates)
   - `mcp__memory__delete_observations` (reduce bloat)
   - `mcp__memory__create_relations` (update after merge)
   - `mcp__memory__delete_relations` (orphans/duplicates)
7. Verify final state with `mcp__memory__read_graph`
8. Report results with metrics

**Graph optimization guidelines:**

- **Target size:** 40-60 entities (optimal for query performance)
- **Observations per entity:** 5-7 (key facts only)
- **Entity retention:** Keep entities from last 30 days + core architecture
- **Session retention:** Keep 2 most recent sessions, archive older
- **Task retention:** Keep active + recently completed (1 month), delete older
- **Relation integrity:** All relations must point to existing entities

**Output:** Optimization report with graph metrics and integrity verification
