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

from prof_client import ProfClient, compute_stat_summary
from time_util import parse_time_arg

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
    print(client.instrument_method(args.class_name, args.method_sig))


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


def cmd_stat_summary(client, args):
    start = parse_time_arg(args.start_time)
    end   = parse_time_arg(args.end_time)
    stats = client.get_stats()
    result = compute_stat_summary(
        stats, args.class_name, args.method_sig, start, end
    )
    print(json.dumps(result, indent=2))


def cmd_resume(client, args):
    print(client.resume())


def cmd_pause_threads(client, args):
    print(client.pause_threads())


def cmd_list_instrumented(client, args):
    for entry in client.list_instrumented_methods():
        print(
            f"{entry['status']}\t{entry['class_name']}\t{entry['method_sig']}"
        )


def cmd_get_async_errors(client, args):
    import datetime
    for entry in client.get_async_errors():
        ts = datetime.datetime.fromtimestamp(entry['timestamp'])
        print(f"{ts}\t{entry['message']}")



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
    p.add_argument( 'file', metavar='file', nargs='?', default='-',
        help='output file path (default: stdout)',
    )

    p = sub.add_parser(
        'stat-summary',
        help=(
            'Summarise runs and average runtime for a method over a time window. The start and end time arguments '
            'are strings which are either in one of the formats [yyyy-][mm-][dd-]hh:mm:ss OR <sec>, where the '
            'first form denotes a specific time in the system time zone, and the second form is the a number of '
            'seconds in the past from the current system clock time. If left unset, the end-time is presumed to be '
            'the current time (equivalent to 0.0).'
        ),
    )
    p.add_argument('class_name', metavar='class')
    p.add_argument('method_sig', metavar='method')
    p.add_argument('start_time', metavar='start')
    p.add_argument('end_time', metavar='end', nargs='?', default='0')

    sub.add_parser('resume', help='resume a JVM paused at startup')

    sub.add_parser('pause-threads', help='suspend all application threads')

    sub.add_parser(
        'list-instrumented',
        help='list all instrumented and deferred methods'
    )

    sub.add_parser(
        'get-async-errors',
        help='show errors from the asynchronous error log'
    )

    args = parser.parse_args()

    client = ProfClient(args.socket)

    commands = {
        'list-classes':       ClientCmd(client, cmd_list_classes),
        'get-methods':        ClientCmd(client, cmd_get_methods),
        'instrument-method':  ClientCmd(client, cmd_instrument_method),
        'deinstrument-method': ClientCmd(client, cmd_deinstrument_method),
        'dump-stats':         ClientCmd(client, cmd_dump_stats),
        'stat-summary':       ClientCmd(client, cmd_stat_summary),
        'shutdown':           ClientCmd(client, cmd_shutdown),
        'resume':             ClientCmd(client, cmd_resume),
        'pause-threads':      ClientCmd(client, cmd_pause_threads),
        'list-instrumented':  ClientCmd(client, cmd_list_instrumented),
        'get-async-errors':   ClientCmd(client, cmd_get_async_errors),
    }

    try:
        commands[args.command](args)
    except (RuntimeError, ConnectionError, ValueError, OSError) as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f'error: unexpected error ({type(e).__name__}): {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
