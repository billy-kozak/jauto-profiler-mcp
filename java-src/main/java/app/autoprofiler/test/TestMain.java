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

package app.autoprofiler.test;

import java.lang.Math;
import java.util.Random;

class TestMain {

    private static Random rand = new Random();

    private static void profilerCountdownTest(int count) throws InterruptedException {
        for (int i = count - 1; i >= 0; i--) {
            count(i);
            Thread.sleep(1000);
        }
    }

    private static void count(int i) {
        if((i % 2) == 0) {
            countEven(i);
        } else {
            countOdd(i);
        }
    }

    private static int randWalk() {
        int r = Math.abs(rand.nextInt());
        for(int i = 0; i < 10000; i++) {
            r = rand.nextInt(r) + 1;
        }
        return r;
    }

    private static void countOdd(int i) {
        System.out.println("Countdown Odd: " + i + " - your random number is " + randWalk());
    }

    private static void countEven(int i) {
        System.out.println("Countdown Even: " + i + "- your random number is " + randWalk());
    }

    public static void main(String[] args) throws InterruptedException {

        int count;

        if (args.length < 1) {
            count = 3600 * 24;
        } else {
            count = Integer.parseInt(args[0]);
        }
        profilerCountdownTest(count);
    }
}
