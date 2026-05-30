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

package app.autoprofiler;

import java.util.concurrent.atomic.AtomicLong;

class Profiler {

    final String className;
    final String methodSig;
    volatile boolean active = true;

    private final AtomicLong callCount = new AtomicLong(0);
    private final AtomicLong totalNanos = new AtomicLong(0);

    /* [0] = re-entry depth, [1] = System.nanoTime() at outermost entry */
    private final ThreadLocal<long[]> threadState = ThreadLocal.withInitial(() -> new long[2]);

    Profiler(String className, String methodSig) {
        this.className = className;
        this.methodSig = methodSig;
    }

    void enter() {
        long[] state = threadState.get();
        if (state[0] == 0) {
            state[1] = System.nanoTime();
        }
        state[0]++;
    }

    void exit() {
        long[] state = threadState.get();
        state[0]--;

        if (state[0] == 0) {
            totalNanos.addAndGet(System.nanoTime() - state[1]);
            callCount.incrementAndGet();
        }
    }

    long getCallCount() {
        return callCount.get();
    }

    long getTotalNanos() {
        return totalNanos.get();
    }
}
