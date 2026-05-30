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

import argparse
import json
import sys

from prof_client import ProfClient

###############################################################################
# subcommand handlers
###############################################################################

class ClientCmd(object):

    def __init__(self, client, handler):
        self.client = client
        self.handler = handler

    def __call__(self, *args):
        return self.handler(self.client, *args)

def cmd_list_classes(client, args):
    for cls in client.list_loaded_classes():
        print(cls)


def cmd_get_methods(client, args):
    for method in client.get_class_methods(args.class_name):
        print(method)


def cmd_instrument_method(client, args):
    client.instrument_method(args.class_name, args.method_sig)


def cmd_deinstrument_method(client, args):
    client.deinstrument_method(args.class_name, args.method_sig)


def cmd_shutdown(client, args):
    client.shutdown()


def cmd_dump_stats(client, args):
    stats = client.get_stats()
    text = json.dumps(stats, indent=2)
    if args.file == '-':
        print(text)
    else:
        with open(args.file, 'w') as f:
            f.write(text)

###############################################################################
# entry point
###############################################################################



def main():
    parser = argparse.ArgumentParser(prog='jauto-prof')
    parser.add_argument(
        '--socket', metavar='PATH',
        help=(
            'profiler socket path '
            '(default: $JAUTO_PROF_SOCKET or /tmp/jauto-profiler.sock)'
        ),
    )

    sub = parser.add_subparsers(dest='command', required=True)

    sub.add_parser('list-classes', help='list classes loaded in the JVM')

    p = sub.add_parser('get-methods', help='list methods of a loaded class')
    p.add_argument('class_name', metavar='class')

    p = sub.add_parser(
        'instrument-method', help='instrument a method for profiling'
    )
    p.add_argument('class_name', metavar='class')
    p.add_argument('method_sig', metavar='method')

    p = sub.add_parser(
        'deinstrument-method', help='remove instrumentation from a method'
    )
    p.add_argument('class_name', metavar='class')
    p.add_argument('method_sig', metavar='method')

    sub.add_parser('shutdown', help='request the profiled JVM to shut down')

    p = sub.add_parser('dump-stats', help='dump profiling stats as JSON')
    p.add_argument(
        'file', metavar='file', nargs='?', default='-',
        help='output file path (default: stdout)',
    )

    args = parser.parse_args()

    client = ProfClient(args.socket)

    commands = {
        'list-classes':       ClientCmd(client, cmd_list_classes),
        'get-methods':        ClientCmd(client, cmd_get_methods),
        'instrument-method':  ClientCmd(client, cmd_instrument_method),
        'deinstrument-method': ClientCmd(client, cmd_deinstrument_method),
        'dump-stats':         ClientCmd(client, cmd_dump_stats),
        'shutdown':           ClientCmd(client, cmd_shutdown),
    }

    try:
        commands[args.command](args)
    except (RuntimeError, ConnectionError, ValueError) as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
