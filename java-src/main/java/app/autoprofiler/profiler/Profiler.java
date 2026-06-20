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

import java.util.concurrent.atomic.LongAdder;

public class Profiler {

    public final long instrumentId;
    public final String name;
    public volatile boolean active = true;

    private final LongAdder callCount = new LongAdder();
    private final LongAdder totalNanos = new LongAdder();

    /* [0] = re-entry depth, [1] = System.nanoTime() at outermost entry */
    private final ThreadLocal<long[]> threadState = ThreadLocal.withInitial(() -> new long[2]);

    /* [0] = active flag (1=seen entry, 0=not), [1] = System.nanoTime() at entry */
    private final ThreadLocal<long[]> lineState = ThreadLocal.withInitial(() -> new long[2]);

    public Profiler(long instrumentId, String name) {
        this.instrumentId = instrumentId;
        this.name = name;
    }

    public void enter() {
        long[] state = threadState.get();
        if (state[0] == 0) {
            state[1] = System.nanoTime();
        }
        state[0]++;
    }

    public void exit() {
        long[] state = threadState.get();
        state[0]--;

        if (state[0] == 0) {
            totalNanos.add(System.nanoTime() - state[1]);
            callCount.increment();
        }
    }

    public void enterLine() {
        long[] state = lineState.get();
        state[0] = 1;
        state[1] = System.nanoTime();
    }

    public void exitLine() {
        long[] state = lineState.get();
        if (state[0] == 1) {
            totalNanos.add(System.nanoTime() - state[1]);
            callCount.increment();
            state[0] = 0;
        }
    }

    public long getCallCount() {
        return callCount.sum();
    }

    public long getTotalNanos() {
        return totalNanos.sum();
    }
}
