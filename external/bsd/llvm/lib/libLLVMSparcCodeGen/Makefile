#	$NetBSD: Makefile,v 1.7 2014/01/15 22:21:34 joerg Exp $

LIB=	LLVMSparcCodeGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/Sparc

SRCS+=	DelaySlotFiller.cpp \
	SparcAsmPrinter.cpp \
	SparcCodeEmitter.cpp \
	SparcInstrInfo.cpp \
	SparcISelDAGToDAG.cpp \
	SparcISelLowering.cpp \
	SparcJITInfo.cpp \
	SparcFrameLowering.cpp \
	SparcMCInstLower.cpp \
	SparcMachineFunctionInfo.cpp \
	SparcRegisterInfo.cpp \
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcSelectionDAGInfo.cpp


TABLEGEN_SRC=		Sparc.td
TABLEGEN_INCLUDES=	-I${LLVM_SRCDIR}/lib/Target/Sparc
TABLEGEN_OUTPUT= \
	SparcGenDAGISel.inc|-gen-dag-isel \
	SparcGenAsmMatcher.inc|-gen-asm-matcher \
	SparcGenAsmWriter.inc|-gen-asm-writer \
	SparcGenCallingConv.inc|-gen-callingconv \
	SparcGenCodeEmitter.inc|-gen-emitter \
	SparcGenDisassemblerTables.inc|-gen-disassembler \
	SparcGenInstrInfo.inc|-gen-instr-info \
	SparcGenMCCodeEmitter.inc|-gen-emitter^-mc-emitter \
	SparcGenRegisterInfo.inc|-gen-register-info \
	SparcGenSubtargetInfo.inc|-gen-subtarget

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
