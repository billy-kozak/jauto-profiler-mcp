# jauto-prof Distribution

This archive contains a self-contained release of the jauto-profiler MCP
server and associated tools for JVM profiling automation.

## Contents

| Path | Description |
|------|-------------|
| `bin/jauto-profiler.so` | JVMTI agent library — attach to the target JVM via `-agentpath:jauto-profiler.so` |
| `bin/jauto-prof-lib.jar` | Profiler support JAR — add to the target JVM via `-Xbootclasspath/a:jauto-prof-lib.jar` |
| `bin/jauto-prof-mcp` | MCP server entry point — register this with your agent harness |
| `py-src/` | Python source for the MCP server and CLI |
| `docs/SKILL.md` | Agent skill guide — install this in your agent harness (see below) |
| `setup` | Installation script — run this first |
| `COPYING` | GNU Lesser General Public License v3 |
| `COPYING.LESSER` | GNU Lesser General Public License v3 (additional terms) |

## Installation

1. Extract this archive.
2. Run the `setup` script from the extracted directory:
   ```
   ./setup
   ```
   This creates a Python virtual environment and installs the CLI
   (`jauto-prof`) and MCP server (`jauto-prof-mcp`) into `bin/`.

## Installing the Skill Guide

`docs/SKILL.md` is an agent skill guide written for LLM agents using the
jauto-profiler MCP server. It describes the workflow, constraints, and
warnings that agents need to use the profiler correctly and safely.

To make the skill guide available to your agent, consult the documentation of your
specific harness.

In claude code, for instance, place the file at `.claude/skills/jauto-prof/SKILL.md`.
