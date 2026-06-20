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

package app.autoprofiler.transform;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class BytecodeTransformer {

    private static final String REGISTRY = "app/autoprofiler/ProfilerRegistry";

    static byte[] transform(
        byte[] classBytes,
        String[] methodSigs,
        int[] methodProfilerIds,
        int[] lineNumbers,
        int[] lineTypes,
        int[] lineProfilerIds
    ) {

        Map<String, MethodInstrument> methodInstruments = new HashMap<>();
        Map<Integer, List<LineInstrument>> lineInstruments = new HashMap<>();

        for(int i = 0; i < methodSigs.length; i++) {
            methodInstruments.put(methodSigs[i], new MethodInstrument(methodProfilerIds[i]));
        }
        for(int i = 0; i < lineNumbers.length; i++) {
            lineInstruments
                .computeIfAbsent(lineNumbers[i], k -> new ArrayList<>())
                .add(new LineInstrument(InstrumentType.fromCode(lineTypes[i]), lineProfilerIds[i]));
        }

        ClassReader cr = new ClassReader(classBytes);
        ClassWriter cw = new SafeClassWriter(cr, ClassWriter.COMPUTE_FRAMES);
        InstrumentingVisitor iv = new InstrumentingVisitor(cw, methodInstruments, lineInstruments);
        cr.accept(iv, ClassReader.SKIP_FRAMES);

        int totalLineInstruments = 0;
        for(List<LineInstrument> list : lineInstruments.values()) {
            totalLineInstruments += list.size();
        }
        int expectedInstruments = methodInstruments.size() + totalLineInstruments;

        if (iv.matchedCount < expectedInstruments) {
            return null;
        }
        return cw.toByteArray();
    }

    /*
     * Falls back to java/lang/Object for any type pair that can't be resolved
     * via the system classloader — target application classes may not be
     * visible here, and COMPUTE_FRAMES only needs the hierarchy for frame
     * merges at branch points, none of which occur in our injected code.
     */
    private static class SafeClassWriter extends ClassWriter {

        SafeClassWriter(ClassReader cr, int flags) {
            super(cr, flags);
        }

        @Override
        protected String getCommonSuperClass(String type1, String type2) {
            try {
                return super.getCommonSuperClass(type1, type2);
            } catch (Exception e) {
                return "java/lang/Object";
            }
        }
    }

    private static class InstrumentingVisitor extends ClassVisitor {

        private final Map<String, MethodInstrument> methodInstruments;
        private final Map<Integer, List<LineInstrument>> lineInstruments;
        int matchedCount = 0;

        InstrumentingVisitor(
            ClassVisitor cv,
            Map<String, MethodInstrument> methodInstruments,
            Map<Integer, List<LineInstrument>> lineInstruments
        ) {
            super(Opcodes.ASM9, cv);
            this.methodInstruments = methodInstruments;
            this.lineInstruments = lineInstruments;
        }

        @Override
        public MethodVisitor visitMethod(
            int access,
            String name,
            String descriptor,
            String signature,
            String[] exceptions
        ) {
            MethodVisitor mv = super.visitMethod(
                access, name, descriptor, signature, exceptions
            );
            String sig = name + ":" + descriptor;
            MethodInstrument instr = methodInstruments.get(sig);

            if(instr != null) {
                return new MethodInstrumentingAdapter(mv, this::countMatch, instr.profilerId, lineInstruments);
            } else {
                return new LineInstrumentingAdapter(mv, this::countMatch, lineInstruments);
            }
        }

        private void countMatch() {
            matchedCount++;
        }
    }


    private static abstract class InstrumentingAdapter extends MethodVisitor {

        public  InstrumentingAdapter(int api, MethodVisitor mv) {
            super(api, mv);
        }

        protected void pushInt(int value) {
            if (value >= -1 && value <= 5) {
                visitInsn(Opcodes.ICONST_0 + value);
            } else if (value >= Byte.MIN_VALUE && value <= Byte.MAX_VALUE) {
                visitIntInsn(Opcodes.BIPUSH, value);
            } else if (value >= Short.MIN_VALUE && value <= Short.MAX_VALUE) {
                visitIntInsn(Opcodes.SIPUSH, value);
            } else {
                visitLdcInsn(value);
            }
        }
    }

    private static class LineInstrumentingAdapter extends InstrumentingAdapter {
        private final Map<Integer, List<LineInstrument>> lineInstruments;
        private final Runnable countMatch;

        LineInstrumentingAdapter(MethodVisitor mv, Runnable countMatch, Map<Integer, List<LineInstrument>> lineInstruments) {
            super(Opcodes.ASM9, mv);
            this.lineInstruments = lineInstruments;
            this.countMatch = countMatch;
        }

        @Override
        public void visitLineNumber(int line, Label start) {
            List<LineInstrument> instruments = lineInstruments.get(line);

            super.visitLineNumber(line, start);
            if(instruments != null) {
                for(LineInstrument i : instruments) {
                    if(i.type == InstrumentType.INSTRUMENT_ENTER) {
                        emitEnterLine(i.profilerId);
                    } else {
                        emitExitLine(i.profilerId);
                    }
                    if(i.mark()) {
                        countMatch.run();
                    }
                }
            }
        }

        private void emitEnterLine(int profilerId) {
            pushInt(profilerId);
            visitMethodInsn(
                Opcodes.INVOKESTATIC, REGISTRY, "enterLine", "(I)V", false
            );
        }

        private void emitExitLine(int profilerId) {
            pushInt(profilerId);
            visitMethodInsn(
                Opcodes.INVOKESTATIC, REGISTRY, "exitLine", "(I)V", false
            );
        }
    }

    private static class MethodInstrumentingAdapter extends LineInstrumentingAdapter {

        private final int profilerId;
        private final Label startFinally;

        MethodInstrumentingAdapter(
                MethodVisitor mv, Runnable countMatch, int profilerId, Map<Integer, List<LineInstrument>> lineInstruments
        ) {
            super(mv, countMatch, lineInstruments);
            this.profilerId = profilerId;
            this.startFinally = new Label();

            countMatch.run();
        }

        @Override
        public void visitCode() {
            super.visitCode();
            visitLabel(startFinally);
            emitEnter();
        }

        /*
         * Intercept every return opcode (IRETURN..RETURN are consecutive in
         * the spec) and inject exit before it.  ATHROW is left alone — the
         * catch-all handler in visitMaxs covers both explicit throws and
         * exceptions that propagate implicitly from called methods.
         */
        @Override
        public void visitInsn(int opcode) {
            if (opcode >= Opcodes.IRETURN && opcode <= Opcodes.RETURN) {
                emitExit();
            }
            super.visitInsn(opcode);
        }

        /*
         * visitMaxs is the last callback before visitEnd, making it the
         * natural place to append the catch-all handler.  endFinally serves
         * as both the exclusive end of the protected range and the handler
         * entry point, so the rethrown exception cannot re-trigger the handler.
         */
        @Override
        public void visitMaxs(int maxStack, int maxLocals) {
            Label endFinally = new Label();
            visitTryCatchBlock(startFinally, endFinally, endFinally, null);
            visitLabel(endFinally);
            emitExit();
            super.visitInsn(Opcodes.ATHROW);
            super.visitMaxs(maxStack, maxLocals);
        }

        private void emitEnter() {
            pushInt(profilerId);
            visitMethodInsn(
                Opcodes.INVOKESTATIC, REGISTRY, "enter", "(I)V", false
            );
        }

        private void emitExit() {
            pushInt(profilerId);
            visitMethodInsn(
                Opcodes.INVOKESTATIC, REGISTRY, "exit", "(I)V", false
            );
        }
    }


    private static class MethodInstrument {
        public final int profilerId;

        public MethodInstrument(int id) {
            profilerId = id;
        }
    }

    private static class LineInstrument {
        public final int profilerId;
        public final InstrumentType type;
        private boolean markFlag;

        public LineInstrument(InstrumentType type, int profilerId) {
            this.type = type;
            this.profilerId = profilerId;
            this.markFlag = false;
        }

        public boolean mark() {
            boolean ret = !markFlag;
            markFlag = true;
            return ret;
        }
    }

    private enum InstrumentType {
        INSTRUMENT_ENTER,
        INSTRUMENT_EXIT;

        public static InstrumentType fromCode(int code) {
            switch(code) {
                case(0):
                    return INSTRUMENT_ENTER;
                case(1):
                    return INSTRUMENT_EXIT;
                default:
                    throw new IllegalArgumentException("Invalid instrument type code " + code);
            }
        }
    }
}
