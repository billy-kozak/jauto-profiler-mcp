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

Subcommand names and options are read directly off of jauto-prof's own
argparse parser, so they can never drift out of sync with the real CLI. Each
subcommand with anything smarter to offer than "nothing" has its own
completer function below, responsible for knowing that subcommand's own
argument layout and dispatching to the right completion primitive (live
class/method/instrument-id lookups against a running profiler, or a
filesystem lookup delegated to bash). Live profiler queries use a short
timeout so a stuck or unreachable profiler can't hang a terminal's tab
completion; any failure to connect or respond is treated as "no completions"
rather than an error.
"""
import argparse
import subprocess
import sys

from .jauto_prof_cli import build_parser
from .prof_client import ProfClient

_CLASS_QUERY_SOCK_TIMEOUT = 0.3
_CLASS_QUERY_RESP_TIMEOUT = 0.3


###############################################################################
# completion primitives
###############################################################################

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


def _query_profiler(socket_path, fetch):
    """Run `fetch(client)` against a live profiler, swallowing failures.

    Returns `fetch`'s result, or None if the profiler couldn't be reached or
    didn't respond within the short query timeout.
    """
    try:
        client = ProfClient(
            socket_path,
            sock_timeout=_CLASS_QUERY_SOCK_TIMEOUT,
            resp_timeout=_CLASS_QUERY_RESP_TIMEOUT,
        )
        try:
            return fetch(client)
        finally:
            client.close()
    except (RuntimeError, ConnectionError, ValueError, OSError):
        return None


def _complete_classes(prefix, socket_path):
    """Class-name completion: all classes loaded in the JVM."""
    classes = _query_profiler(socket_path, ProfClient.get_loaded_classes)
    if classes is None:
        return []
    return [c for c in classes if c.startswith(prefix)]


def _complete_instrumented_classes(prefix, socket_path):
    """Class-name completion: only classes with an instrumented method."""
    entries = _query_profiler(socket_path, ProfClient.get_instruments)
    if entries is None:
        return []
    classes = {e['class_name'] for e in entries if e.get('type') == 'method'}
    return [c for c in classes if c.startswith(prefix)]


def _complete_methods(class_name, prefix, socket_path):
    """Method-signature completion: all methods of a loaded class."""
    methods = _query_profiler(socket_path, lambda c: c.get_class_methods(class_name))
    if methods is None:
        return []
    return [m for m in methods if m.startswith(prefix)]


def _complete_instrumented_methods(class_name, prefix, socket_path):
    """Method-signature completion: only methods instrumented on this class."""
    entries = _query_profiler(socket_path, ProfClient.get_instruments)
    if entries is None:
        return []
    methods = {
        e['method_sig'] for e in entries
        if e.get('type') == 'method' and e.get('class_name') == class_name
    }
    return [m for m in methods if m.startswith(prefix)]


def _complete_stat_names(prefix, socket_path):
    """Instrument-name completion: names present in the current stats snapshot."""
    stats = _query_profiler(socket_path, ProfClient.get_stats)
    if stats is None:
        return []
    names = {s['name'] for s in stats['probes']}
    return [n for n in names if n.startswith(prefix)]


def _complete_instrument_ids(prefix, socket_path):
    """Instrument-ID completion: IDs of all instrumented/deferred entries."""
    entries = _query_profiler(socket_path, ProfClient.get_instruments)
    if entries is None:
        return []
    ids = {str(e['instrument_id']) for e in entries}
    return [i for i in ids if i.startswith(prefix)]


###############################################################################
# argparse introspection helpers
###############################################################################

def _option_map(actions_container):
    """Map each option string of a parser (or subparser) to whether it takes a value."""
    options = {}
    for action in actions_container._actions:
        for opt in action.option_strings:
            options[opt] = action.nargs != 0
    return options


def _subparsers_action(parser):
    for action in parser._actions:
        if isinstance(action, argparse._SubParsersAction):
            return action
    return None


def _split_args(sub_options, words_after):
    """Split already-typed subcommand words into positionals and a pending option.

    Returns (positionals, pending): `positionals` is the list of positional
    argument values typed so far, in order (so a completer needing e.g. the
    class already typed for a method argument can look at positionals[0]).
    `pending` is the option string whose value `cur` (the word following
    `words_after`) will fill, or None if `cur` is itself a positional/option
    name.
    """
    positionals = []
    i = 0
    while i < len(words_after):
        tok = words_after[i]
        if tok in sub_options:
            i += 2 if sub_options[tok] else 1
            continue
        if tok.startswith('-'):
            i += 1
            continue
        positionals.append(tok)
        i += 1

    pending = words_after[-1] if words_after else None
    if not (pending in sub_options and sub_options[pending]):
        pending = None

    return positionals, pending


###############################################################################
# per-subcommand completers
#
# Each takes (subparser, words_after, cur, socket_path) -- `words_after` are
# the already-typed words following the subcommand name, not including `cur`
# -- and returns a list of candidate completions. Each is responsible for
# knowing its own subcommand's argument layout.
###############################################################################

def _complete_instrument_method(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending is not None:
        return []
    if len(positionals) == 0:  # class
        return _complete_classes(cur, socket_path)
    if len(positionals) == 1:  # method
        return _complete_methods(positionals[0], cur, socket_path)
    return []


def _complete_instrument_line(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending == '--exit-class':
        return _complete_classes(cur, socket_path)
    if pending is not None:
        return []
    if len(positionals) == 0:  # entry-class
        return _complete_classes(cur, socket_path)
    # entry-line (1), exit-line (2): no completion
    return []


def _complete_deinstrument_method(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending is not None:
        return []
    if len(positionals) == 0:  # class
        return _complete_instrumented_classes(cur, socket_path)
    if len(positionals) == 1:  # method
        return _complete_instrumented_methods(positionals[0], cur, socket_path)
    return []


def _complete_dump_stats(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending is None and len(positionals) == 0:  # file
        return _compgen_f(cur)
    return []


def _complete_stat_summary(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending is not None:
        return []
    if len(positionals) == 0:  # name
        return _complete_stat_names(cur, socket_path)
    # start (1), end (2): no completion
    return []


def _complete_deinstrument_by_id(subparser, words_after, cur, socket_path):
    positionals, pending = _split_args(_option_map(subparser), words_after)
    if pending is None and len(positionals) == 0:  # id
        return _complete_instrument_ids(cur, socket_path)
    return []


_SUBCOMMAND_COMPLETERS = {
    'instrument-method': _complete_instrument_method,
    'instrument-line': _complete_instrument_line,
    'deinstrument-method': _complete_deinstrument_method,
    'deinstrument-by-id': _complete_deinstrument_by_id,
    'dump-stats': _complete_dump_stats,
    'stat-summary': _complete_stat_summary,
}


###############################################################################
# entry point
###############################################################################

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
    options = _option_map(parser)
    sp_action = _subparsers_action(parser)
    subcommands = list(sp_action.choices) if sp_action else []

    # Walk the words already typed, skipping global options (and their
    # values), to find the subcommand (if any has been typed yet) and any
    # --socket/--tcp override so profiler queries hit the right server.
    subcommand = None
    sub_index = None
    socket_path = None
    i = 0
    while i < len(prior):
        tok = prior[i]
        if tok == '--socket' and i + 1 < len(prior):
            socket_path = prior[i + 1]
            i += 2
            continue
        if tok == '--tcp' and i + 1 < len(prior):
            socket_path = f'tcp://{prior[i + 1]}'
            i += 2
            continue
        if tok in options:
            i += 2 if options[tok] else 1
            continue
        if tok.startswith('-'):
            i += 1
            continue
        subcommand = tok
        sub_index = i
        break

    if subcommand is None:
        prev = prior[-1] if prior else None
        if prev == '--socket':
            return _compgen_f(cur)
        if prev in options and options[prev]:
            return []  # value of e.g. --tcp / --sock-timeout: no completion
        candidates = subcommands + list(options)
        return [c for c in candidates if c.startswith(cur)]

    completer = _SUBCOMMAND_COMPLETERS.get(subcommand)
    if completer is None:
        return []

    subparser = sp_action.choices[subcommand]
    words_after = prior[sub_index + 1:]
    return completer(subparser, words_after, cur, socket_path)


def main():
    for match in complete(sys.argv[1:]):
        print(match)


if __name__ == '__main__':
    main()
