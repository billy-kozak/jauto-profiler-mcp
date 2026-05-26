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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

class ProfilerRegistry {

	private static final ProfilerRegistry INSTANCE = new ProfilerRegistry();

	private final CopyOnWriteArrayList<ProfilerEntry> entries = new CopyOnWriteArrayList<>();
	private final Thread collectorThread;

	private ProfilerRegistry() {
		collectorThread = new Thread(this::collect, "jauto-profiler-collector");
		collectorThread.setDaemon(true);
		collectorThread.start();
	}

	synchronized static int create(String className, String methodSig) {
		ProfilerEntry entry = new ProfilerEntry(new Profiler(className, methodSig));
		int id = INSTANCE.entries.size();
		INSTANCE.entries.add(entry);
		return id;
	}

	static byte[] getStats() {
		List<ProfilerEntry> entries = new ArrayList<>(INSTANCE.entries);
		int n = entries.size();

		byte[][] classNameBytes = new byte[n][];
		byte[][] methodSigBytes = new byte[n][];
		ProfilerSnapshot[][] snapshots = new ProfilerSnapshot[n][];

		int totalSize = Integer.BYTES;
		for (int i = 0; i < n; i++) {
			classNameBytes[i] = entries.get(i).profiler.className.getBytes(StandardCharsets.UTF_8);
			methodSigBytes[i] = entries.get(i).profiler.methodSig.getBytes(StandardCharsets.UTF_8);
			snapshots[i] = entries.get(i).getOrderedSnapshots();
			totalSize += Short.BYTES + classNameBytes[i].length;
			totalSize += Short.BYTES + methodSigBytes[i].length;
			totalSize += Integer.BYTES + snapshots[i].length * (Long.BYTES * 3);
		}

		ByteBuffer buf = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN);
		buf.putInt(n);
		for (int i = 0; i < n; i++) {
			buf.putShort((short) classNameBytes[i].length);
			buf.put(classNameBytes[i]);
			buf.putShort((short) methodSigBytes[i].length);
			buf.put(methodSigBytes[i]);
			buf.putInt(snapshots[i].length);
			for (ProfilerSnapshot snap : snapshots[i]) {
				buf.putLong(snap.timestamp);
				buf.putLong(snap.callCount);
				buf.putLong(snap.totalNanos);
			}
		}
		return buf.array();
	}

	static void enter(int id) {
		INSTANCE.entries.get(id).profiler.enter();
	}

	static void exit(int id) {
		INSTANCE.entries.get(id).profiler.exit();
	}

	private void collect() {
		while (!Thread.currentThread().isInterrupted()) {
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				Thread.currentThread().interrupt();
				break;
			}
			long timestamp = System.currentTimeMillis() / 1000;
			for (ProfilerEntry entry : entries) {
				entry.record(timestamp);
			}
		}
	}
}
