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

import time
from datetime import date, datetime

# Each entry: (strptime format string, fill_year, fill_month, fill_day).
# Tried in order; the first successful parse wins.  Fields marked True are
# absent from the format and must be filled in from today's date.
_DATETIME_FORMATS = [
    ("%Y-%m-%d-%H:%M:%S", False, False, False),
    (  "%m-%d-%H:%M:%S", True,  False, False),
    (     "%d-%H:%M:%S", True,  True,  False),
    (        "%H:%M:%S", True,  True,  True ),
]


def resolve_time_arg(v: str | float) -> float:
    """Resolve a time argument that is either a number or a string.

    If v is a number it is treated as seconds in the past (0 = now).
    If v is a string it is forwarded to parse_time_arg.
    """
    if isinstance(v, (int, float)):
        return time.time() - v
    return parse_time_arg(v)


def parse_time_arg(s: str) -> float:
    """Parse a CLI time argument and return a Unix timestamp (float seconds).

    Two forms are accepted:

    [yyyy-][mm-][dd-]hh:mm:ss
        A specific local-time clock reading.  Components are optional from the
        left: if only hh:mm:ss is given the date defaults to today; one leading
        component is the day-of-month; two are month and day; three are the
        full year, month, and day.

    <sec>
        A number of seconds in the past relative to the current clock time.
        0 means "right now".
    """
    if ':' not in s:
        try:
            offset = float(s)
        except ValueError:
            raise ValueError(
                f"cannot parse time argument {s!r}: "
                "expected [yyyy-][mm-][dd-]hh:mm:ss or a number of seconds"
            )
        return time.time() - offset

    today = date.today()
    for fmt, fill_year, fill_month, fill_day in _DATETIME_FORMATS:
        try:
            dt = datetime.strptime(s, fmt)
        except ValueError:
            continue
        dt = dt.replace(
            year=today.year   if fill_year  else dt.year,
            month=today.month if fill_month else dt.month,
            day=today.day     if fill_day   else dt.day,
        )
        return dt.timestamp()

    raise ValueError(
        f"cannot parse time argument {s!r}: "
        "expected [yyyy-][mm-][dd-]hh:mm:ss or a number of seconds"
    )
