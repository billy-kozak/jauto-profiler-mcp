#!/usr/bin/env python3
###############################################################################
# Copyright (C) 2026  Billy Kozak                                             #
#                                                                             #
# This file is part of the jauto-profiler program                             #
#                                                                             #
# This program is free software: you can redistribute it and/or modify        #
# it under the terms of the GNU Lesser General Public License as published by #
# the Free Software Foundation, either version 3 of the License, or           #
# (at your option) any later version.                                         #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU Lesser General Public License for more details.                         #
#                                                                             #
# You should have received a copy of the GNU Lesser General Public License    #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.       #
###############################################################################
"""Backing helper for the jauto-prof bash tab completion function.

Not meant to be run by hand and not installed on $PATH. It is invoked with
COMP_WORDS as argv (argv[0] is the invoked command name, argv[-1] is the word
currently being completed) and prints one candidate completion per line.

Subcommand names and global options are read directly off of jauto-prof's own
argparse parser, so this can never drift out of sync with the real CLI.
"""
import argparse
import subprocess
import sys

from .jauto_prof_cli import build_parser

# Subcommands whose (sole, unlabeled) positional argument is a file path.
_FILE_ARG_SUBCOMMANDS = {'dump-stats'}


def _compgen_f(prefix):
    """File-path completion, delegated to bash's own `compgen -f` builtin.

    Bash already knows how to do this correctly (tilde expansion, quoting of
    special characters, etc.); reimplementing it in Python isn't worth it.
    """
    try:
        proc = subprocess.run(
            ['bash', '-c', 'compgen -f -- "$1"', '_', prefix],
            capture_output=True, text=True,
        )
    except OSError:
        return []
    return proc.stdout.splitlines()


def _global_options(parser):
    """Map each global option string to whether it takes a value."""
    options = {}
    for action in parser._actions:
        for opt in action.option_strings:
            options[opt] = action.nargs != 0
    return options


def _subcommand_names(parser):
    for action in parser._actions:
        if isinstance(action, argparse._SubParsersAction):
            return list(action.choices)
    return []


def complete(words):
    """Return candidate completions for the command line `words`.

    `words` is COMP_WORDS: words[0] is the invoked command, words[-1] is the
    word currently being completed.
    """
    if len(words) < 2:
        return []

    cur = words[-1]
    prior = words[1:-1]

    parser = build_parser()
    options = _global_options(parser)
    subcommands = _subcommand_names(parser)

    # Walk the words already typed, skipping global options (and their
    # values), to find the subcommand (if any has been typed yet).
    subcommand = None
    i = 0
    while i < len(prior):
        tok = prior[i]
        if tok in options:
            i += 2 if options[tok] else 1
            continue
        if tok.startswith('-'):
            i += 1
            continue
        subcommand = tok
        break

    if subcommand is None:
        prev = prior[-1] if prior else None
        if prev == '--socket':
            return _compgen_f(cur)
        if prev in options and options[prev]:
            return []  # value of e.g. --tcp / --sock-timeout: no completion
        candidates = subcommands + list(options)
        return [c for c in candidates if c.startswith(cur)]

    if subcommand in _FILE_ARG_SUBCOMMANDS:
        return _compgen_f(cur)

    return []


def main():
    for match in complete(sys.argv[1:]):
        print(match)


if __name__ == '__main__':
    main()
