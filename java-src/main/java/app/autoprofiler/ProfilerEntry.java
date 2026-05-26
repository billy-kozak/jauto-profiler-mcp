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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

class ProfilerEntry {

	static final int RING_BUFFER_SIZE = 3600;

	final Profiler profiler;
	private final ProfilerSnapshot[] buffer = new ProfilerSnapshot[RING_BUFFER_SIZE];
	private int head = 0;

	ProfilerEntry(Profiler profiler) {
		this.profiler = profiler;
	}

	synchronized void record(long timestamp) {
		buffer[head] = new ProfilerSnapshot(
			timestamp, profiler.getCallCount(), profiler.getTotalNanos()
		);
		head = (head + 1) % RING_BUFFER_SIZE;
	}

	ProfilerSnapshot[] getOrderedSnapshots() {
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
}
