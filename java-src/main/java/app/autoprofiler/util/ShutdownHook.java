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

package app.autoprofiler.util;

import java.util.ArrayList;
import java.util.List;

public class ShutdownHook {

    private static final List<long[]> callbacks = new ArrayList<>();

    private ShutdownHook() {}

    private static native void nativeShutdownCallback();

    public static synchronized void addCallback(long fnPtr, long dataPtr) {
        callbacks.add(new long[]{fnPtr, dataPtr});
    }

    static synchronized long[] getCallbacks() {
        long[] result = new long[callbacks.size() * 2];
        int i = 0;
        for (long[] pair : callbacks) {
            result[i++] = pair[0];
            result[i++] = pair[1];
        }
        return result;
    }

    public static void registerShutdownHook() {
        Thread hook = new Thread(
            ShutdownHook::nativeShutdownCallback,
            "jauto-profiler-shutdown-hook"
        );
        Runtime.getRuntime().addShutdownHook(hook);
    }
}
