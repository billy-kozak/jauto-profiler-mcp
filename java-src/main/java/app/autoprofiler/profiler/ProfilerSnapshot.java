/*
 * This file is part of jauto-profiler
 * Copyright (c) 2026 Bill Kozak
 *
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

package app.autoprofiler.profiler;

public class ProfilerSnapshot implements Comparable<ProfilerSnapshot> {

    public final long timestamp;
    public final long callCount;
    public final long totalNanos;

    public ProfilerSnapshot(long timestamp, long callCount, long totalNanos) {
        this.timestamp = timestamp;
        this.callCount = callCount;
        this.totalNanos = totalNanos;
    }

    @Override
    public int compareTo(ProfilerSnapshot other) {
        return Long.compare(this.timestamp, other.timestamp);
    }
}
