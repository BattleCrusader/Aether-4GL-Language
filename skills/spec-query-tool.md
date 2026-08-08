---
name: spec-query-tool
description: Search SPECIFICATION.md for language features.
version: 0.1.0
tags: spec,reference,query
---

# spec-query-tool

## When to Use
- Locate exact requirement or feature in Aether spec.
- Retrieve implementation details of a language construct.
- Verify if a functionality is implemented or planned.
- Find related test cases or examples.
- Retrieve contract specifications.

## Prerequisites
- Access to SPECIFICATION.md file.
- Hermes session with file access.

## How to Run
1. Load skill: skill_manage create spec-query-tool
2. Invoke query: spec_query "<term>"
   - Example: spec_query "memory management" returns matching excerpts.

## Quick Reference
spec_query <query>
    - Returns matching spec excerpts with line numbers.
    - Supports simple term search.

## Procedure
1. Ensure SPECIFICATION.md is present.
2. Use terminal to run spec_query <term>.
3. Review returned excerpts; note relevant sections.
4. If deeper context needed, use read_file on SPECIFICATION.md.

## Pitfalls
- Skill only works if spec file is not pruned.
- Queries are term‑based; keep terms concise.
- Excerpts may be partial; consult full spec for details.
- Does not parse complex natural language; use keywords.

## Verification
- Run spec_query "struct" and confirm non‑empty result.
- Command should exit with status 0.

## References
- SPECIFICATION.md path: /Volumes/Backup/Development/Project_Aether/compiler/SPECIFICATION.md
- Tools used: read_file, search_files, terminal
