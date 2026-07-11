# jauto-prof Distribution

This archive contains a self-contained release of the jauto-profiler MCP
server and associated tools for JVM profiling automation.

## Contents

| Path | Description |
|------|-------------|
| `bin/jauto-profiler.so` | JVMTI agent library — attach to the target JVM via `-agentpath:jauto-profiler.so` |
| `bin/jauto-prof-lib.jar` | Profiler support JAR — add to the target JVM via `-Xbootclasspath/a:jauto-prof-lib.jar` |
| `bin/jauto-prof-mcp` | MCP server entry point — register this with your agent harness |
| `bin/jauto-completions` | Bash tab completion helper — not meant to be run by hand, see below |
| `py-src/` | Python source for the MCP server and CLI |
| `docs/SKILL.md` | Agent skill guide — install this in your agent harness (see below) |
| `docs/jauto-prof.bash-completion` | Bash tab completion for the CLI (see below) |
| `install` | System install script — copies this distribution into `$PREFIX` and runs `setup` there |
| `setup` | Creates the Python virtual environment in place; run directly for an in-place (non-copying) setup |
| `COPYING` | GNU Lesser General Public License v3 |
| `COPYING.LESSER` | GNU Lesser General Public License v3 (additional terms) |

## Installation

To install system-wide (recommended):

1. Extract this archive.
2. Run the `install` script:
   ```
   ./install
   ```
   This copies the distribution into `/opt/jauto-prof-mcp` by default,
   creates a Python virtual environment there, and installs the CLI
   (`jauto-prof`) and MCP server (`jauto-prof-mcp`) into its `bin/`.

   To install elsewhere, set `PREFIX`:
   ```
   PREFIX=/usr/local/jauto-prof-mcp ./install
   ```

   On success, `install` prints the lines to add to your `~/.bashrc` to put
   `jauto-prof` on your `$PATH` and enable tab completion.

To use the tool directly from the extracted directory instead of installing
it elsewhere, run `./setup` instead — this only creates the virtual
environment in place, without copying anything.

## Installing the Skill Guide

`docs/SKILL.md` is an agent skill guide written for LLM agents using the
jauto-profiler MCP server. It describes the workflow, constraints, and
warnings that agents need to use the profiler correctly and safely.

To make the skill guide available to your agent, consult the documentation of your
specific harness.

In claude code, for instance, place the file at `.claude/skills/jauto-prof/SKILL.md`.

## Installing Tab Completion

`docs/jauto-prof.bash-completion` adds bash tab completion for the `jauto-prof`
CLI (subcommand names and file path arguments).

Source it from your shell's startup file, e.g.:
```
echo 'source /path/to/docs/jauto-prof.bash-completion' >> ~/.bashrc
```

This registers completion for the bare `jauto-prof` command, so it assumes
`bin/jauto-prof` is on your `$PATH`. If you invoke it via a path instead,
register that literal invocation too — see the comment at the top of the
file.
