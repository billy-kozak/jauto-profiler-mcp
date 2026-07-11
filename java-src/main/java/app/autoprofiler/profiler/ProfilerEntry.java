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

import app.autoprofiler.util.Logger;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ProfilerEntry {

    private static final Logger LOGGER = Logger.init("profiler-entry");

    private static final String NSLOTS_ENV     = "JAUTO_PROF_NSLOTS";
    private static final int    DEFAULT_NSLOTS = 3600;

    public static final int RING_BUFFER_SIZE = resolveNSlots();

    public final Profiler profiler;
    private final ProfilerSnapshot[] buffer = new ProfilerSnapshot[RING_BUFFER_SIZE];
    private int head = 0;

    public ProfilerEntry(Profiler profiler) {
        this.profiler = profiler;
    }

    public synchronized void record(long timestamp) {
        buffer[head] = new ProfilerSnapshot(
            timestamp, profiler.getCallCount(), profiler.getTotalNanos()
        );
        head = (head + 1) % RING_BUFFER_SIZE;
    }

    /* Repeats the previous cumulative counts under a new timestamp, i.e. a
     * snapshot with a zero delta. Used to backfill a tick the collector
     * couldn't sample on time, so a stall shows up in the data as an
     * explicit flatline instead of being silently folded into a wider
     * real interval. */
    public synchronized void recordZero(long timestamp) {
        ProfilerSnapshot last = buffer[(head - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE];
        long callCount  = (last != null) ? last.callCount  : 0;
        long totalNanos = (last != null) ? last.totalNanos : 0;
        buffer[head] = new ProfilerSnapshot(timestamp, callCount, totalNanos);
        head = (head + 1) % RING_BUFFER_SIZE;
    }

    public ProfilerSnapshot[] getOrderedSnapshots() {
        List<ProfilerSnapshot> result = new ArrayList<>(RING_BUFFER_SIZE);
        synchronized (this) {
            for (ProfilerSnapshot snap : buffer) {
                if (snap != null) {
                    result.add(snap);
                }
            }
        }
        Collections.sort(result);
        return result.toArray(new ProfilerSnapshot[0]);
    }

    private static int resolveNSlots() {
        String env = System.getenv(NSLOTS_ENV);
        if (env == null || env.isEmpty()) {
            return DEFAULT_NSLOTS;
        }
        try {
            int n = Integer.parseInt(env);
            if (n <= 0) {
                LOGGER.warn(
                    NSLOTS_ENV + " must be a positive integer, got '" + env
                        + "'. Defaulting to " + DEFAULT_NSLOTS + "."
                );
                return DEFAULT_NSLOTS;
            }
            return n;
        } catch (NumberFormatException e) {
            LOGGER.warn(
                NSLOTS_ENV + " must be a positive integer, got '" + env
                    + "'. Defaulting to " + DEFAULT_NSLOTS + "."
            );
            return DEFAULT_NSLOTS;
        }
    }
}
