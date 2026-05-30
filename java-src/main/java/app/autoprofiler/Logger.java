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

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class Logger {

    private static final String PATH_ENV     = "JAUTO_PROF_LOG";
    private static final String LEVEL_ENV    = "JAUTO_PROF_LOG_LEVEL";
    private static final String DEFAULT_PATH = "/tmp/jauto-prof.log";

    private static final DateTimeFormatter TS_FMT = DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm:ss.SSS");

    private final String tag;

    public enum Level {

        OFF(-1), ERROR(0), WARN(1), INFO(2), DEBUG(3);

        final int value;

        Level(int value) {
            this.value = value;
        }

        static Level parse(String s) {
            switch (s) {
                case "OFF":   return OFF;
                case "ERROR": return ERROR;
                case "WARN":  return WARN;
                case "INFO":  return INFO;
                case "DEBUG": return DEBUG;
                default:      return INFO;
            }
        }

        String label() {
            switch (this) {
                case ERROR: return "ERROR";
                case WARN:  return "WARN ";
                case INFO:  return "INFO ";
                case DEBUG: return "DEBUG";
                default:    return "?    ";
            }
        }
    }

    private static final Level threshold;
    private static final OutputStream out;

    static {
        Logger logger = new Logger("log");
        String pathEnv  = System.getenv(PATH_ENV);
        String levelEnv = System.getenv(LEVEL_ENV);

        Level lvl = Level.INFO;
        if (levelEnv != null && !levelEnv.isEmpty()) {
            lvl = Level.parse(levelEnv);
        }
        threshold = lvl;

        OutputStream stream = System.err;

        if (threshold != Level.OFF) {
            String path = (pathEnv != null) ? pathEnv : DEFAULT_PATH;
            if (!path.isEmpty()) {
                try {
                    stream = new FileOutputStream(path, true);
                } catch (IOException e) {
                    logger.warn(
                        "JVM failed to open log file '" + path + "'. Defaulting to stderr."
                    );
                }
            }
        }
        out = stream;
    }

    private Logger(String tag) {
        this.tag = tag;
    }

    public void error(String msg) {
        write(Level.ERROR, tag, msg);
    }

    public void warn(String msg) {
        write(Level.WARN, tag, msg);
    }

    public void info(String msg) {
        write(Level.INFO, tag, msg);
    }

    public void debug(String msg) {
        write(Level.DEBUG, tag, msg);
    }


    private static void write(Level level, String tag, String msg) {
        if (level.value > threshold.value) {
            return;
        }
        String line = LocalDateTime.now().format(TS_FMT) + " " + level.label() + " [" + tag + "] " + msg + "\n";
        byte[] bytes = line.getBytes(StandardCharsets.UTF_8);
        try {
            synchronized (out) {
                out.write(bytes);
            }
        } catch (IOException e) {
            // nothing useful to do if the log destination itself fails
        }
    }

    public static Logger init(String tag) {
        return new Logger(tag);
    }
}
