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

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

class BytecodeTransformer {

    private static final String REGISTRY = "app/autoprofiler/ProfilerRegistry";

    static byte[] transform(
        byte[] classBytes,
        String methodName,
        String methodDesc,
        int profilerId
    ) {
        ClassReader cr = new ClassReader(classBytes);
        ClassWriter cw = new SafeClassWriter(cr, ClassWriter.COMPUTE_FRAMES);
        cr.accept(
            new InstrumentingVisitor(cw, methodName, methodDesc, profilerId),
            ClassReader.SKIP_FRAMES
        );
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

        private final String targetName;
        private final String targetDesc;
        private final int profilerId;

        InstrumentingVisitor(
            ClassVisitor cv,
            String targetName,
            String targetDesc,
            int profilerId
        ) {
            super(Opcodes.ASM9, cv);
            this.targetName = targetName;
            this.targetDesc = targetDesc;
            this.profilerId = profilerId;
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
            if (name.equals(targetName) && descriptor.equals(targetDesc)) {
                return new InstrumentingAdapter(mv, profilerId);
            }
            return mv;
        }
    }

    private static class InstrumentingAdapter extends MethodVisitor {

        private final int profilerId;
        private Label startFinally;

        InstrumentingAdapter(MethodVisitor mv, int profilerId) {
            super(Opcodes.ASM9, mv);
            this.profilerId = profilerId;
        }

        @Override
        public void visitCode() {
            super.visitCode();
            startFinally = new Label();
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

        /*
         * ICONST_M1..ICONST_5 opcodes are consecutive starting at ICONST_0-1,
         * so ICONST_0 + value gives the right opcode for values in [-1, 5].
         */
        private void pushInt(int value) {
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
}
