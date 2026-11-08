// NVN proc table. Every name the game's proc loader asks for gets a synthetic host
// address through hostfn_register before hostfn_seal; nvnBootstrapLoader and nvnDeviceGetProcAddress hand them out.
// nvnp_<name> is the implementation of one proc: the weak definitions here stop with the proc's name, nvn_api_*.c
// and nvn_ext.c define the strong ones. The name list is runtime/nvn_procs.inc (tools/gen_nvn_procs.py).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "gfx.h"
#include "host.h"
#include "platform/platform.h"

// The strong nvnp_nvnDeviceGetProcAddress below replaces the stand-in generated for the same row.
#define nvnp_nvnDeviceGetProcAddress nvnp_stub_nvnDeviceGetProcAddress

#define NVN_PROC(n) void nvnp_##n(Cpu *c);
#include "nvn_procs.inc"
#undef NVN_PROC

#define NVN_PROC(n)                                                                                                  \
    __attribute__((weak)) void nvnp_##n(Cpu *c) {                                                                     \
        (void)c;                                                                                                     \
        rt_fatal("NVN " #n " is not implemented");                                                                   \
    }
#include "nvn_procs.inc"
#undef NVN_PROC
#undef nvnp_nvnDeviceGetProcAddress
void nvnp_nvnDeviceGetProcAddress(Cpu *c);

typedef struct { const char *name; uint64_t addr; } Proc;

#define NVN_PROC(n) +1
enum { PROC_COUNT = 0
#include "nvn_procs.inc"
};
#undef NVN_PROC

static Proc procs[PROC_COUNT];
static int nprocs;

// Unknown procs (NVN2 names the NVN1-era table lacks): a NVN2 driver has every name the game asks for, so a NULL
// answer crashes the game at the first call (main+0xc31484 called one). Each unknown name is bound on demand to one
// of TRAP_COUNT pre-registered traps (the hostfn table is sealed after boot). A trap logs its name and caller on the
// first call and returns 0 -- a no-op for the setters most of these are; NX2NAVI_NVN_TRAP=fatal stops instead.
#define TRAP_COUNT 512
static char trap_name[TRAP_COUNT][96];
static uint64_t trap_addr[TRAP_COUNT];
static volatile int32_t trap_called[TRAP_COUNT];
static int ntraps;
static void nvn_trap(Cpu *c, int i) {
    if (!plat_xchg32(&trap_called[i], 1) || (cfg_get("NX2NAVI_NVN_TRAP") && !strcmp(cfg_get("NX2NAVI_NVN_TRAP"), "fatal"))) {
        char at[160];
        rt_log("nvn: unimplemented NVN2 proc %s called from %s (x0 %#llx x1 %#llx x2 %#llx x3 %#llx x4 %#llx x5 %#llx): returning 0", trap_name[i],
               ldr_describe(c->x[30], at, sizeof at), (unsigned long long)c->x[0], (unsigned long long)c->x[1],
               (unsigned long long)c->x[2], (unsigned long long)c->x[3], (unsigned long long)c->x[4], (unsigned long long)c->x[5]);
        const char *mode = cfg_get("NX2NAVI_NVN_TRAP");
        if (mode && !strcmp(mode, "fatal")) rt_fatal("nvn: %s is not implemented", trap_name[i]);
    }
    ret_x(c, 0);
}
// trap_0 .. trap_511, one per pool slot
static void trap_0(Cpu *c) { nvn_trap(c, 0); }
static void trap_1(Cpu *c) { nvn_trap(c, 1); }
static void trap_2(Cpu *c) { nvn_trap(c, 2); }
static void trap_3(Cpu *c) { nvn_trap(c, 3); }
static void trap_4(Cpu *c) { nvn_trap(c, 4); }
static void trap_5(Cpu *c) { nvn_trap(c, 5); }
static void trap_6(Cpu *c) { nvn_trap(c, 6); }
static void trap_7(Cpu *c) { nvn_trap(c, 7); }
static void trap_8(Cpu *c) { nvn_trap(c, 8); }
static void trap_9(Cpu *c) { nvn_trap(c, 9); }
static void trap_10(Cpu *c) { nvn_trap(c, 10); }
static void trap_11(Cpu *c) { nvn_trap(c, 11); }
static void trap_12(Cpu *c) { nvn_trap(c, 12); }
static void trap_13(Cpu *c) { nvn_trap(c, 13); }
static void trap_14(Cpu *c) { nvn_trap(c, 14); }
static void trap_15(Cpu *c) { nvn_trap(c, 15); }
static void trap_16(Cpu *c) { nvn_trap(c, 16); }
static void trap_17(Cpu *c) { nvn_trap(c, 17); }
static void trap_18(Cpu *c) { nvn_trap(c, 18); }
static void trap_19(Cpu *c) { nvn_trap(c, 19); }
static void trap_20(Cpu *c) { nvn_trap(c, 20); }
static void trap_21(Cpu *c) { nvn_trap(c, 21); }
static void trap_22(Cpu *c) { nvn_trap(c, 22); }
static void trap_23(Cpu *c) { nvn_trap(c, 23); }
static void trap_24(Cpu *c) { nvn_trap(c, 24); }
static void trap_25(Cpu *c) { nvn_trap(c, 25); }
static void trap_26(Cpu *c) { nvn_trap(c, 26); }
static void trap_27(Cpu *c) { nvn_trap(c, 27); }
static void trap_28(Cpu *c) { nvn_trap(c, 28); }
static void trap_29(Cpu *c) { nvn_trap(c, 29); }
static void trap_30(Cpu *c) { nvn_trap(c, 30); }
static void trap_31(Cpu *c) { nvn_trap(c, 31); }
static void trap_32(Cpu *c) { nvn_trap(c, 32); }
static void trap_33(Cpu *c) { nvn_trap(c, 33); }
static void trap_34(Cpu *c) { nvn_trap(c, 34); }
static void trap_35(Cpu *c) { nvn_trap(c, 35); }
static void trap_36(Cpu *c) { nvn_trap(c, 36); }
static void trap_37(Cpu *c) { nvn_trap(c, 37); }
static void trap_38(Cpu *c) { nvn_trap(c, 38); }
static void trap_39(Cpu *c) { nvn_trap(c, 39); }
static void trap_40(Cpu *c) { nvn_trap(c, 40); }
static void trap_41(Cpu *c) { nvn_trap(c, 41); }
static void trap_42(Cpu *c) { nvn_trap(c, 42); }
static void trap_43(Cpu *c) { nvn_trap(c, 43); }
static void trap_44(Cpu *c) { nvn_trap(c, 44); }
static void trap_45(Cpu *c) { nvn_trap(c, 45); }
static void trap_46(Cpu *c) { nvn_trap(c, 46); }
static void trap_47(Cpu *c) { nvn_trap(c, 47); }
static void trap_48(Cpu *c) { nvn_trap(c, 48); }
static void trap_49(Cpu *c) { nvn_trap(c, 49); }
static void trap_50(Cpu *c) { nvn_trap(c, 50); }
static void trap_51(Cpu *c) { nvn_trap(c, 51); }
static void trap_52(Cpu *c) { nvn_trap(c, 52); }
static void trap_53(Cpu *c) { nvn_trap(c, 53); }
static void trap_54(Cpu *c) { nvn_trap(c, 54); }
static void trap_55(Cpu *c) { nvn_trap(c, 55); }
static void trap_56(Cpu *c) { nvn_trap(c, 56); }
static void trap_57(Cpu *c) { nvn_trap(c, 57); }
static void trap_58(Cpu *c) { nvn_trap(c, 58); }
static void trap_59(Cpu *c) { nvn_trap(c, 59); }
static void trap_60(Cpu *c) { nvn_trap(c, 60); }
static void trap_61(Cpu *c) { nvn_trap(c, 61); }
static void trap_62(Cpu *c) { nvn_trap(c, 62); }
static void trap_63(Cpu *c) { nvn_trap(c, 63); }
static void trap_64(Cpu *c) { nvn_trap(c, 64); }
static void trap_65(Cpu *c) { nvn_trap(c, 65); }
static void trap_66(Cpu *c) { nvn_trap(c, 66); }
static void trap_67(Cpu *c) { nvn_trap(c, 67); }
static void trap_68(Cpu *c) { nvn_trap(c, 68); }
static void trap_69(Cpu *c) { nvn_trap(c, 69); }
static void trap_70(Cpu *c) { nvn_trap(c, 70); }
static void trap_71(Cpu *c) { nvn_trap(c, 71); }
static void trap_72(Cpu *c) { nvn_trap(c, 72); }
static void trap_73(Cpu *c) { nvn_trap(c, 73); }
static void trap_74(Cpu *c) { nvn_trap(c, 74); }
static void trap_75(Cpu *c) { nvn_trap(c, 75); }
static void trap_76(Cpu *c) { nvn_trap(c, 76); }
static void trap_77(Cpu *c) { nvn_trap(c, 77); }
static void trap_78(Cpu *c) { nvn_trap(c, 78); }
static void trap_79(Cpu *c) { nvn_trap(c, 79); }
static void trap_80(Cpu *c) { nvn_trap(c, 80); }
static void trap_81(Cpu *c) { nvn_trap(c, 81); }
static void trap_82(Cpu *c) { nvn_trap(c, 82); }
static void trap_83(Cpu *c) { nvn_trap(c, 83); }
static void trap_84(Cpu *c) { nvn_trap(c, 84); }
static void trap_85(Cpu *c) { nvn_trap(c, 85); }
static void trap_86(Cpu *c) { nvn_trap(c, 86); }
static void trap_87(Cpu *c) { nvn_trap(c, 87); }
static void trap_88(Cpu *c) { nvn_trap(c, 88); }
static void trap_89(Cpu *c) { nvn_trap(c, 89); }
static void trap_90(Cpu *c) { nvn_trap(c, 90); }
static void trap_91(Cpu *c) { nvn_trap(c, 91); }
static void trap_92(Cpu *c) { nvn_trap(c, 92); }
static void trap_93(Cpu *c) { nvn_trap(c, 93); }
static void trap_94(Cpu *c) { nvn_trap(c, 94); }
static void trap_95(Cpu *c) { nvn_trap(c, 95); }
static void trap_96(Cpu *c) { nvn_trap(c, 96); }
static void trap_97(Cpu *c) { nvn_trap(c, 97); }
static void trap_98(Cpu *c) { nvn_trap(c, 98); }
static void trap_99(Cpu *c) { nvn_trap(c, 99); }
static void trap_100(Cpu *c) { nvn_trap(c, 100); }
static void trap_101(Cpu *c) { nvn_trap(c, 101); }
static void trap_102(Cpu *c) { nvn_trap(c, 102); }
static void trap_103(Cpu *c) { nvn_trap(c, 103); }
static void trap_104(Cpu *c) { nvn_trap(c, 104); }
static void trap_105(Cpu *c) { nvn_trap(c, 105); }
static void trap_106(Cpu *c) { nvn_trap(c, 106); }
static void trap_107(Cpu *c) { nvn_trap(c, 107); }
static void trap_108(Cpu *c) { nvn_trap(c, 108); }
static void trap_109(Cpu *c) { nvn_trap(c, 109); }
static void trap_110(Cpu *c) { nvn_trap(c, 110); }
static void trap_111(Cpu *c) { nvn_trap(c, 111); }
static void trap_112(Cpu *c) { nvn_trap(c, 112); }
static void trap_113(Cpu *c) { nvn_trap(c, 113); }
static void trap_114(Cpu *c) { nvn_trap(c, 114); }
static void trap_115(Cpu *c) { nvn_trap(c, 115); }
static void trap_116(Cpu *c) { nvn_trap(c, 116); }
static void trap_117(Cpu *c) { nvn_trap(c, 117); }
static void trap_118(Cpu *c) { nvn_trap(c, 118); }
static void trap_119(Cpu *c) { nvn_trap(c, 119); }
static void trap_120(Cpu *c) { nvn_trap(c, 120); }
static void trap_121(Cpu *c) { nvn_trap(c, 121); }
static void trap_122(Cpu *c) { nvn_trap(c, 122); }
static void trap_123(Cpu *c) { nvn_trap(c, 123); }
static void trap_124(Cpu *c) { nvn_trap(c, 124); }
static void trap_125(Cpu *c) { nvn_trap(c, 125); }
static void trap_126(Cpu *c) { nvn_trap(c, 126); }
static void trap_127(Cpu *c) { nvn_trap(c, 127); }
static void trap_128(Cpu *c) { nvn_trap(c, 128); }
static void trap_129(Cpu *c) { nvn_trap(c, 129); }
static void trap_130(Cpu *c) { nvn_trap(c, 130); }
static void trap_131(Cpu *c) { nvn_trap(c, 131); }
static void trap_132(Cpu *c) { nvn_trap(c, 132); }
static void trap_133(Cpu *c) { nvn_trap(c, 133); }
static void trap_134(Cpu *c) { nvn_trap(c, 134); }
static void trap_135(Cpu *c) { nvn_trap(c, 135); }
static void trap_136(Cpu *c) { nvn_trap(c, 136); }
static void trap_137(Cpu *c) { nvn_trap(c, 137); }
static void trap_138(Cpu *c) { nvn_trap(c, 138); }
static void trap_139(Cpu *c) { nvn_trap(c, 139); }
static void trap_140(Cpu *c) { nvn_trap(c, 140); }
static void trap_141(Cpu *c) { nvn_trap(c, 141); }
static void trap_142(Cpu *c) { nvn_trap(c, 142); }
static void trap_143(Cpu *c) { nvn_trap(c, 143); }
static void trap_144(Cpu *c) { nvn_trap(c, 144); }
static void trap_145(Cpu *c) { nvn_trap(c, 145); }
static void trap_146(Cpu *c) { nvn_trap(c, 146); }
static void trap_147(Cpu *c) { nvn_trap(c, 147); }
static void trap_148(Cpu *c) { nvn_trap(c, 148); }
static void trap_149(Cpu *c) { nvn_trap(c, 149); }
static void trap_150(Cpu *c) { nvn_trap(c, 150); }
static void trap_151(Cpu *c) { nvn_trap(c, 151); }
static void trap_152(Cpu *c) { nvn_trap(c, 152); }
static void trap_153(Cpu *c) { nvn_trap(c, 153); }
static void trap_154(Cpu *c) { nvn_trap(c, 154); }
static void trap_155(Cpu *c) { nvn_trap(c, 155); }
static void trap_156(Cpu *c) { nvn_trap(c, 156); }
static void trap_157(Cpu *c) { nvn_trap(c, 157); }
static void trap_158(Cpu *c) { nvn_trap(c, 158); }
static void trap_159(Cpu *c) { nvn_trap(c, 159); }
static void trap_160(Cpu *c) { nvn_trap(c, 160); }
static void trap_161(Cpu *c) { nvn_trap(c, 161); }
static void trap_162(Cpu *c) { nvn_trap(c, 162); }
static void trap_163(Cpu *c) { nvn_trap(c, 163); }
static void trap_164(Cpu *c) { nvn_trap(c, 164); }
static void trap_165(Cpu *c) { nvn_trap(c, 165); }
static void trap_166(Cpu *c) { nvn_trap(c, 166); }
static void trap_167(Cpu *c) { nvn_trap(c, 167); }
static void trap_168(Cpu *c) { nvn_trap(c, 168); }
static void trap_169(Cpu *c) { nvn_trap(c, 169); }
static void trap_170(Cpu *c) { nvn_trap(c, 170); }
static void trap_171(Cpu *c) { nvn_trap(c, 171); }
static void trap_172(Cpu *c) { nvn_trap(c, 172); }
static void trap_173(Cpu *c) { nvn_trap(c, 173); }
static void trap_174(Cpu *c) { nvn_trap(c, 174); }
static void trap_175(Cpu *c) { nvn_trap(c, 175); }
static void trap_176(Cpu *c) { nvn_trap(c, 176); }
static void trap_177(Cpu *c) { nvn_trap(c, 177); }
static void trap_178(Cpu *c) { nvn_trap(c, 178); }
static void trap_179(Cpu *c) { nvn_trap(c, 179); }
static void trap_180(Cpu *c) { nvn_trap(c, 180); }
static void trap_181(Cpu *c) { nvn_trap(c, 181); }
static void trap_182(Cpu *c) { nvn_trap(c, 182); }
static void trap_183(Cpu *c) { nvn_trap(c, 183); }
static void trap_184(Cpu *c) { nvn_trap(c, 184); }
static void trap_185(Cpu *c) { nvn_trap(c, 185); }
static void trap_186(Cpu *c) { nvn_trap(c, 186); }
static void trap_187(Cpu *c) { nvn_trap(c, 187); }
static void trap_188(Cpu *c) { nvn_trap(c, 188); }
static void trap_189(Cpu *c) { nvn_trap(c, 189); }
static void trap_190(Cpu *c) { nvn_trap(c, 190); }
static void trap_191(Cpu *c) { nvn_trap(c, 191); }
static void trap_192(Cpu *c) { nvn_trap(c, 192); }
static void trap_193(Cpu *c) { nvn_trap(c, 193); }
static void trap_194(Cpu *c) { nvn_trap(c, 194); }
static void trap_195(Cpu *c) { nvn_trap(c, 195); }
static void trap_196(Cpu *c) { nvn_trap(c, 196); }
static void trap_197(Cpu *c) { nvn_trap(c, 197); }
static void trap_198(Cpu *c) { nvn_trap(c, 198); }
static void trap_199(Cpu *c) { nvn_trap(c, 199); }
static void trap_200(Cpu *c) { nvn_trap(c, 200); }
static void trap_201(Cpu *c) { nvn_trap(c, 201); }
static void trap_202(Cpu *c) { nvn_trap(c, 202); }
static void trap_203(Cpu *c) { nvn_trap(c, 203); }
static void trap_204(Cpu *c) { nvn_trap(c, 204); }
static void trap_205(Cpu *c) { nvn_trap(c, 205); }
static void trap_206(Cpu *c) { nvn_trap(c, 206); }
static void trap_207(Cpu *c) { nvn_trap(c, 207); }
static void trap_208(Cpu *c) { nvn_trap(c, 208); }
static void trap_209(Cpu *c) { nvn_trap(c, 209); }
static void trap_210(Cpu *c) { nvn_trap(c, 210); }
static void trap_211(Cpu *c) { nvn_trap(c, 211); }
static void trap_212(Cpu *c) { nvn_trap(c, 212); }
static void trap_213(Cpu *c) { nvn_trap(c, 213); }
static void trap_214(Cpu *c) { nvn_trap(c, 214); }
static void trap_215(Cpu *c) { nvn_trap(c, 215); }
static void trap_216(Cpu *c) { nvn_trap(c, 216); }
static void trap_217(Cpu *c) { nvn_trap(c, 217); }
static void trap_218(Cpu *c) { nvn_trap(c, 218); }
static void trap_219(Cpu *c) { nvn_trap(c, 219); }
static void trap_220(Cpu *c) { nvn_trap(c, 220); }
static void trap_221(Cpu *c) { nvn_trap(c, 221); }
static void trap_222(Cpu *c) { nvn_trap(c, 222); }
static void trap_223(Cpu *c) { nvn_trap(c, 223); }
static void trap_224(Cpu *c) { nvn_trap(c, 224); }
static void trap_225(Cpu *c) { nvn_trap(c, 225); }
static void trap_226(Cpu *c) { nvn_trap(c, 226); }
static void trap_227(Cpu *c) { nvn_trap(c, 227); }
static void trap_228(Cpu *c) { nvn_trap(c, 228); }
static void trap_229(Cpu *c) { nvn_trap(c, 229); }
static void trap_230(Cpu *c) { nvn_trap(c, 230); }
static void trap_231(Cpu *c) { nvn_trap(c, 231); }
static void trap_232(Cpu *c) { nvn_trap(c, 232); }
static void trap_233(Cpu *c) { nvn_trap(c, 233); }
static void trap_234(Cpu *c) { nvn_trap(c, 234); }
static void trap_235(Cpu *c) { nvn_trap(c, 235); }
static void trap_236(Cpu *c) { nvn_trap(c, 236); }
static void trap_237(Cpu *c) { nvn_trap(c, 237); }
static void trap_238(Cpu *c) { nvn_trap(c, 238); }
static void trap_239(Cpu *c) { nvn_trap(c, 239); }
static void trap_240(Cpu *c) { nvn_trap(c, 240); }
static void trap_241(Cpu *c) { nvn_trap(c, 241); }
static void trap_242(Cpu *c) { nvn_trap(c, 242); }
static void trap_243(Cpu *c) { nvn_trap(c, 243); }
static void trap_244(Cpu *c) { nvn_trap(c, 244); }
static void trap_245(Cpu *c) { nvn_trap(c, 245); }
static void trap_246(Cpu *c) { nvn_trap(c, 246); }
static void trap_247(Cpu *c) { nvn_trap(c, 247); }
static void trap_248(Cpu *c) { nvn_trap(c, 248); }
static void trap_249(Cpu *c) { nvn_trap(c, 249); }
static void trap_250(Cpu *c) { nvn_trap(c, 250); }
static void trap_251(Cpu *c) { nvn_trap(c, 251); }
static void trap_252(Cpu *c) { nvn_trap(c, 252); }
static void trap_253(Cpu *c) { nvn_trap(c, 253); }
static void trap_254(Cpu *c) { nvn_trap(c, 254); }
static void trap_255(Cpu *c) { nvn_trap(c, 255); }
static void trap_256(Cpu *c) { nvn_trap(c, 256); }
static void trap_257(Cpu *c) { nvn_trap(c, 257); }
static void trap_258(Cpu *c) { nvn_trap(c, 258); }
static void trap_259(Cpu *c) { nvn_trap(c, 259); }
static void trap_260(Cpu *c) { nvn_trap(c, 260); }
static void trap_261(Cpu *c) { nvn_trap(c, 261); }
static void trap_262(Cpu *c) { nvn_trap(c, 262); }
static void trap_263(Cpu *c) { nvn_trap(c, 263); }
static void trap_264(Cpu *c) { nvn_trap(c, 264); }
static void trap_265(Cpu *c) { nvn_trap(c, 265); }
static void trap_266(Cpu *c) { nvn_trap(c, 266); }
static void trap_267(Cpu *c) { nvn_trap(c, 267); }
static void trap_268(Cpu *c) { nvn_trap(c, 268); }
static void trap_269(Cpu *c) { nvn_trap(c, 269); }
static void trap_270(Cpu *c) { nvn_trap(c, 270); }
static void trap_271(Cpu *c) { nvn_trap(c, 271); }
static void trap_272(Cpu *c) { nvn_trap(c, 272); }
static void trap_273(Cpu *c) { nvn_trap(c, 273); }
static void trap_274(Cpu *c) { nvn_trap(c, 274); }
static void trap_275(Cpu *c) { nvn_trap(c, 275); }
static void trap_276(Cpu *c) { nvn_trap(c, 276); }
static void trap_277(Cpu *c) { nvn_trap(c, 277); }
static void trap_278(Cpu *c) { nvn_trap(c, 278); }
static void trap_279(Cpu *c) { nvn_trap(c, 279); }
static void trap_280(Cpu *c) { nvn_trap(c, 280); }
static void trap_281(Cpu *c) { nvn_trap(c, 281); }
static void trap_282(Cpu *c) { nvn_trap(c, 282); }
static void trap_283(Cpu *c) { nvn_trap(c, 283); }
static void trap_284(Cpu *c) { nvn_trap(c, 284); }
static void trap_285(Cpu *c) { nvn_trap(c, 285); }
static void trap_286(Cpu *c) { nvn_trap(c, 286); }
static void trap_287(Cpu *c) { nvn_trap(c, 287); }
static void trap_288(Cpu *c) { nvn_trap(c, 288); }
static void trap_289(Cpu *c) { nvn_trap(c, 289); }
static void trap_290(Cpu *c) { nvn_trap(c, 290); }
static void trap_291(Cpu *c) { nvn_trap(c, 291); }
static void trap_292(Cpu *c) { nvn_trap(c, 292); }
static void trap_293(Cpu *c) { nvn_trap(c, 293); }
static void trap_294(Cpu *c) { nvn_trap(c, 294); }
static void trap_295(Cpu *c) { nvn_trap(c, 295); }
static void trap_296(Cpu *c) { nvn_trap(c, 296); }
static void trap_297(Cpu *c) { nvn_trap(c, 297); }
static void trap_298(Cpu *c) { nvn_trap(c, 298); }
static void trap_299(Cpu *c) { nvn_trap(c, 299); }
static void trap_300(Cpu *c) { nvn_trap(c, 300); }
static void trap_301(Cpu *c) { nvn_trap(c, 301); }
static void trap_302(Cpu *c) { nvn_trap(c, 302); }
static void trap_303(Cpu *c) { nvn_trap(c, 303); }
static void trap_304(Cpu *c) { nvn_trap(c, 304); }
static void trap_305(Cpu *c) { nvn_trap(c, 305); }
static void trap_306(Cpu *c) { nvn_trap(c, 306); }
static void trap_307(Cpu *c) { nvn_trap(c, 307); }
static void trap_308(Cpu *c) { nvn_trap(c, 308); }
static void trap_309(Cpu *c) { nvn_trap(c, 309); }
static void trap_310(Cpu *c) { nvn_trap(c, 310); }
static void trap_311(Cpu *c) { nvn_trap(c, 311); }
static void trap_312(Cpu *c) { nvn_trap(c, 312); }
static void trap_313(Cpu *c) { nvn_trap(c, 313); }
static void trap_314(Cpu *c) { nvn_trap(c, 314); }
static void trap_315(Cpu *c) { nvn_trap(c, 315); }
static void trap_316(Cpu *c) { nvn_trap(c, 316); }
static void trap_317(Cpu *c) { nvn_trap(c, 317); }
static void trap_318(Cpu *c) { nvn_trap(c, 318); }
static void trap_319(Cpu *c) { nvn_trap(c, 319); }
static void trap_320(Cpu *c) { nvn_trap(c, 320); }
static void trap_321(Cpu *c) { nvn_trap(c, 321); }
static void trap_322(Cpu *c) { nvn_trap(c, 322); }
static void trap_323(Cpu *c) { nvn_trap(c, 323); }
static void trap_324(Cpu *c) { nvn_trap(c, 324); }
static void trap_325(Cpu *c) { nvn_trap(c, 325); }
static void trap_326(Cpu *c) { nvn_trap(c, 326); }
static void trap_327(Cpu *c) { nvn_trap(c, 327); }
static void trap_328(Cpu *c) { nvn_trap(c, 328); }
static void trap_329(Cpu *c) { nvn_trap(c, 329); }
static void trap_330(Cpu *c) { nvn_trap(c, 330); }
static void trap_331(Cpu *c) { nvn_trap(c, 331); }
static void trap_332(Cpu *c) { nvn_trap(c, 332); }
static void trap_333(Cpu *c) { nvn_trap(c, 333); }
static void trap_334(Cpu *c) { nvn_trap(c, 334); }
static void trap_335(Cpu *c) { nvn_trap(c, 335); }
static void trap_336(Cpu *c) { nvn_trap(c, 336); }
static void trap_337(Cpu *c) { nvn_trap(c, 337); }
static void trap_338(Cpu *c) { nvn_trap(c, 338); }
static void trap_339(Cpu *c) { nvn_trap(c, 339); }
static void trap_340(Cpu *c) { nvn_trap(c, 340); }
static void trap_341(Cpu *c) { nvn_trap(c, 341); }
static void trap_342(Cpu *c) { nvn_trap(c, 342); }
static void trap_343(Cpu *c) { nvn_trap(c, 343); }
static void trap_344(Cpu *c) { nvn_trap(c, 344); }
static void trap_345(Cpu *c) { nvn_trap(c, 345); }
static void trap_346(Cpu *c) { nvn_trap(c, 346); }
static void trap_347(Cpu *c) { nvn_trap(c, 347); }
static void trap_348(Cpu *c) { nvn_trap(c, 348); }
static void trap_349(Cpu *c) { nvn_trap(c, 349); }
static void trap_350(Cpu *c) { nvn_trap(c, 350); }
static void trap_351(Cpu *c) { nvn_trap(c, 351); }
static void trap_352(Cpu *c) { nvn_trap(c, 352); }
static void trap_353(Cpu *c) { nvn_trap(c, 353); }
static void trap_354(Cpu *c) { nvn_trap(c, 354); }
static void trap_355(Cpu *c) { nvn_trap(c, 355); }
static void trap_356(Cpu *c) { nvn_trap(c, 356); }
static void trap_357(Cpu *c) { nvn_trap(c, 357); }
static void trap_358(Cpu *c) { nvn_trap(c, 358); }
static void trap_359(Cpu *c) { nvn_trap(c, 359); }
static void trap_360(Cpu *c) { nvn_trap(c, 360); }
static void trap_361(Cpu *c) { nvn_trap(c, 361); }
static void trap_362(Cpu *c) { nvn_trap(c, 362); }
static void trap_363(Cpu *c) { nvn_trap(c, 363); }
static void trap_364(Cpu *c) { nvn_trap(c, 364); }
static void trap_365(Cpu *c) { nvn_trap(c, 365); }
static void trap_366(Cpu *c) { nvn_trap(c, 366); }
static void trap_367(Cpu *c) { nvn_trap(c, 367); }
static void trap_368(Cpu *c) { nvn_trap(c, 368); }
static void trap_369(Cpu *c) { nvn_trap(c, 369); }
static void trap_370(Cpu *c) { nvn_trap(c, 370); }
static void trap_371(Cpu *c) { nvn_trap(c, 371); }
static void trap_372(Cpu *c) { nvn_trap(c, 372); }
static void trap_373(Cpu *c) { nvn_trap(c, 373); }
static void trap_374(Cpu *c) { nvn_trap(c, 374); }
static void trap_375(Cpu *c) { nvn_trap(c, 375); }
static void trap_376(Cpu *c) { nvn_trap(c, 376); }
static void trap_377(Cpu *c) { nvn_trap(c, 377); }
static void trap_378(Cpu *c) { nvn_trap(c, 378); }
static void trap_379(Cpu *c) { nvn_trap(c, 379); }
static void trap_380(Cpu *c) { nvn_trap(c, 380); }
static void trap_381(Cpu *c) { nvn_trap(c, 381); }
static void trap_382(Cpu *c) { nvn_trap(c, 382); }
static void trap_383(Cpu *c) { nvn_trap(c, 383); }
static void trap_384(Cpu *c) { nvn_trap(c, 384); }
static void trap_385(Cpu *c) { nvn_trap(c, 385); }
static void trap_386(Cpu *c) { nvn_trap(c, 386); }
static void trap_387(Cpu *c) { nvn_trap(c, 387); }
static void trap_388(Cpu *c) { nvn_trap(c, 388); }
static void trap_389(Cpu *c) { nvn_trap(c, 389); }
static void trap_390(Cpu *c) { nvn_trap(c, 390); }
static void trap_391(Cpu *c) { nvn_trap(c, 391); }
static void trap_392(Cpu *c) { nvn_trap(c, 392); }
static void trap_393(Cpu *c) { nvn_trap(c, 393); }
static void trap_394(Cpu *c) { nvn_trap(c, 394); }
static void trap_395(Cpu *c) { nvn_trap(c, 395); }
static void trap_396(Cpu *c) { nvn_trap(c, 396); }
static void trap_397(Cpu *c) { nvn_trap(c, 397); }
static void trap_398(Cpu *c) { nvn_trap(c, 398); }
static void trap_399(Cpu *c) { nvn_trap(c, 399); }
static void trap_400(Cpu *c) { nvn_trap(c, 400); }
static void trap_401(Cpu *c) { nvn_trap(c, 401); }
static void trap_402(Cpu *c) { nvn_trap(c, 402); }
static void trap_403(Cpu *c) { nvn_trap(c, 403); }
static void trap_404(Cpu *c) { nvn_trap(c, 404); }
static void trap_405(Cpu *c) { nvn_trap(c, 405); }
static void trap_406(Cpu *c) { nvn_trap(c, 406); }
static void trap_407(Cpu *c) { nvn_trap(c, 407); }
static void trap_408(Cpu *c) { nvn_trap(c, 408); }
static void trap_409(Cpu *c) { nvn_trap(c, 409); }
static void trap_410(Cpu *c) { nvn_trap(c, 410); }
static void trap_411(Cpu *c) { nvn_trap(c, 411); }
static void trap_412(Cpu *c) { nvn_trap(c, 412); }
static void trap_413(Cpu *c) { nvn_trap(c, 413); }
static void trap_414(Cpu *c) { nvn_trap(c, 414); }
static void trap_415(Cpu *c) { nvn_trap(c, 415); }
static void trap_416(Cpu *c) { nvn_trap(c, 416); }
static void trap_417(Cpu *c) { nvn_trap(c, 417); }
static void trap_418(Cpu *c) { nvn_trap(c, 418); }
static void trap_419(Cpu *c) { nvn_trap(c, 419); }
static void trap_420(Cpu *c) { nvn_trap(c, 420); }
static void trap_421(Cpu *c) { nvn_trap(c, 421); }
static void trap_422(Cpu *c) { nvn_trap(c, 422); }
static void trap_423(Cpu *c) { nvn_trap(c, 423); }
static void trap_424(Cpu *c) { nvn_trap(c, 424); }
static void trap_425(Cpu *c) { nvn_trap(c, 425); }
static void trap_426(Cpu *c) { nvn_trap(c, 426); }
static void trap_427(Cpu *c) { nvn_trap(c, 427); }
static void trap_428(Cpu *c) { nvn_trap(c, 428); }
static void trap_429(Cpu *c) { nvn_trap(c, 429); }
static void trap_430(Cpu *c) { nvn_trap(c, 430); }
static void trap_431(Cpu *c) { nvn_trap(c, 431); }
static void trap_432(Cpu *c) { nvn_trap(c, 432); }
static void trap_433(Cpu *c) { nvn_trap(c, 433); }
static void trap_434(Cpu *c) { nvn_trap(c, 434); }
static void trap_435(Cpu *c) { nvn_trap(c, 435); }
static void trap_436(Cpu *c) { nvn_trap(c, 436); }
static void trap_437(Cpu *c) { nvn_trap(c, 437); }
static void trap_438(Cpu *c) { nvn_trap(c, 438); }
static void trap_439(Cpu *c) { nvn_trap(c, 439); }
static void trap_440(Cpu *c) { nvn_trap(c, 440); }
static void trap_441(Cpu *c) { nvn_trap(c, 441); }
static void trap_442(Cpu *c) { nvn_trap(c, 442); }
static void trap_443(Cpu *c) { nvn_trap(c, 443); }
static void trap_444(Cpu *c) { nvn_trap(c, 444); }
static void trap_445(Cpu *c) { nvn_trap(c, 445); }
static void trap_446(Cpu *c) { nvn_trap(c, 446); }
static void trap_447(Cpu *c) { nvn_trap(c, 447); }
static void trap_448(Cpu *c) { nvn_trap(c, 448); }
static void trap_449(Cpu *c) { nvn_trap(c, 449); }
static void trap_450(Cpu *c) { nvn_trap(c, 450); }
static void trap_451(Cpu *c) { nvn_trap(c, 451); }
static void trap_452(Cpu *c) { nvn_trap(c, 452); }
static void trap_453(Cpu *c) { nvn_trap(c, 453); }
static void trap_454(Cpu *c) { nvn_trap(c, 454); }
static void trap_455(Cpu *c) { nvn_trap(c, 455); }
static void trap_456(Cpu *c) { nvn_trap(c, 456); }
static void trap_457(Cpu *c) { nvn_trap(c, 457); }
static void trap_458(Cpu *c) { nvn_trap(c, 458); }
static void trap_459(Cpu *c) { nvn_trap(c, 459); }
static void trap_460(Cpu *c) { nvn_trap(c, 460); }
static void trap_461(Cpu *c) { nvn_trap(c, 461); }
static void trap_462(Cpu *c) { nvn_trap(c, 462); }
static void trap_463(Cpu *c) { nvn_trap(c, 463); }
static void trap_464(Cpu *c) { nvn_trap(c, 464); }
static void trap_465(Cpu *c) { nvn_trap(c, 465); }
static void trap_466(Cpu *c) { nvn_trap(c, 466); }
static void trap_467(Cpu *c) { nvn_trap(c, 467); }
static void trap_468(Cpu *c) { nvn_trap(c, 468); }
static void trap_469(Cpu *c) { nvn_trap(c, 469); }
static void trap_470(Cpu *c) { nvn_trap(c, 470); }
static void trap_471(Cpu *c) { nvn_trap(c, 471); }
static void trap_472(Cpu *c) { nvn_trap(c, 472); }
static void trap_473(Cpu *c) { nvn_trap(c, 473); }
static void trap_474(Cpu *c) { nvn_trap(c, 474); }
static void trap_475(Cpu *c) { nvn_trap(c, 475); }
static void trap_476(Cpu *c) { nvn_trap(c, 476); }
static void trap_477(Cpu *c) { nvn_trap(c, 477); }
static void trap_478(Cpu *c) { nvn_trap(c, 478); }
static void trap_479(Cpu *c) { nvn_trap(c, 479); }
static void trap_480(Cpu *c) { nvn_trap(c, 480); }
static void trap_481(Cpu *c) { nvn_trap(c, 481); }
static void trap_482(Cpu *c) { nvn_trap(c, 482); }
static void trap_483(Cpu *c) { nvn_trap(c, 483); }
static void trap_484(Cpu *c) { nvn_trap(c, 484); }
static void trap_485(Cpu *c) { nvn_trap(c, 485); }
static void trap_486(Cpu *c) { nvn_trap(c, 486); }
static void trap_487(Cpu *c) { nvn_trap(c, 487); }
static void trap_488(Cpu *c) { nvn_trap(c, 488); }
static void trap_489(Cpu *c) { nvn_trap(c, 489); }
static void trap_490(Cpu *c) { nvn_trap(c, 490); }
static void trap_491(Cpu *c) { nvn_trap(c, 491); }
static void trap_492(Cpu *c) { nvn_trap(c, 492); }
static void trap_493(Cpu *c) { nvn_trap(c, 493); }
static void trap_494(Cpu *c) { nvn_trap(c, 494); }
static void trap_495(Cpu *c) { nvn_trap(c, 495); }
static void trap_496(Cpu *c) { nvn_trap(c, 496); }
static void trap_497(Cpu *c) { nvn_trap(c, 497); }
static void trap_498(Cpu *c) { nvn_trap(c, 498); }
static void trap_499(Cpu *c) { nvn_trap(c, 499); }
static void trap_500(Cpu *c) { nvn_trap(c, 500); }
static void trap_501(Cpu *c) { nvn_trap(c, 501); }
static void trap_502(Cpu *c) { nvn_trap(c, 502); }
static void trap_503(Cpu *c) { nvn_trap(c, 503); }
static void trap_504(Cpu *c) { nvn_trap(c, 504); }
static void trap_505(Cpu *c) { nvn_trap(c, 505); }
static void trap_506(Cpu *c) { nvn_trap(c, 506); }
static void trap_507(Cpu *c) { nvn_trap(c, 507); }
static void trap_508(Cpu *c) { nvn_trap(c, 508); }
static void trap_509(Cpu *c) { nvn_trap(c, 509); }
static void trap_510(Cpu *c) { nvn_trap(c, 510); }
static void trap_511(Cpu *c) { nvn_trap(c, 511); }

static int by_name(const void *a, const void *b) { return strcmp(((const Proc *)a)->name, ((const Proc *)b)->name); }

void nvn_procs_register(void) {
    if (nprocs) return;
#define NVN_PROC(n) procs[nprocs++] = (Proc){#n, hostfn_register("nvn:" #n, nvnp_##n)};
#include "nvn_procs.inc"
#undef NVN_PROC
    qsort(procs, (size_t)nprocs, sizeof *procs, by_name);
    static const GuestFn traps[TRAP_COUNT] = {
        trap_0, trap_1, trap_2, trap_3, trap_4, trap_5, trap_6, trap_7,
        trap_8, trap_9, trap_10, trap_11, trap_12, trap_13, trap_14, trap_15,
        trap_16, trap_17, trap_18, trap_19, trap_20, trap_21, trap_22, trap_23,
        trap_24, trap_25, trap_26, trap_27, trap_28, trap_29, trap_30, trap_31,
        trap_32, trap_33, trap_34, trap_35, trap_36, trap_37, trap_38, trap_39,
        trap_40, trap_41, trap_42, trap_43, trap_44, trap_45, trap_46, trap_47,
        trap_48, trap_49, trap_50, trap_51, trap_52, trap_53, trap_54, trap_55,
        trap_56, trap_57, trap_58, trap_59, trap_60, trap_61, trap_62, trap_63,
        trap_64, trap_65, trap_66, trap_67, trap_68, trap_69, trap_70, trap_71,
        trap_72, trap_73, trap_74, trap_75, trap_76, trap_77, trap_78, trap_79,
        trap_80, trap_81, trap_82, trap_83, trap_84, trap_85, trap_86, trap_87,
        trap_88, trap_89, trap_90, trap_91, trap_92, trap_93, trap_94, trap_95,
        trap_96, trap_97, trap_98, trap_99, trap_100, trap_101, trap_102, trap_103,
        trap_104, trap_105, trap_106, trap_107, trap_108, trap_109, trap_110, trap_111,
        trap_112, trap_113, trap_114, trap_115, trap_116, trap_117, trap_118, trap_119,
        trap_120, trap_121, trap_122, trap_123, trap_124, trap_125, trap_126, trap_127,
        trap_128, trap_129, trap_130, trap_131, trap_132, trap_133, trap_134, trap_135,
        trap_136, trap_137, trap_138, trap_139, trap_140, trap_141, trap_142, trap_143,
        trap_144, trap_145, trap_146, trap_147, trap_148, trap_149, trap_150, trap_151,
        trap_152, trap_153, trap_154, trap_155, trap_156, trap_157, trap_158, trap_159,
        trap_160, trap_161, trap_162, trap_163, trap_164, trap_165, trap_166, trap_167,
        trap_168, trap_169, trap_170, trap_171, trap_172, trap_173, trap_174, trap_175,
        trap_176, trap_177, trap_178, trap_179, trap_180, trap_181, trap_182, trap_183,
        trap_184, trap_185, trap_186, trap_187, trap_188, trap_189, trap_190, trap_191,
        trap_192, trap_193, trap_194, trap_195, trap_196, trap_197, trap_198, trap_199,
        trap_200, trap_201, trap_202, trap_203, trap_204, trap_205, trap_206, trap_207,
        trap_208, trap_209, trap_210, trap_211, trap_212, trap_213, trap_214, trap_215,
        trap_216, trap_217, trap_218, trap_219, trap_220, trap_221, trap_222, trap_223,
        trap_224, trap_225, trap_226, trap_227, trap_228, trap_229, trap_230, trap_231,
        trap_232, trap_233, trap_234, trap_235, trap_236, trap_237, trap_238, trap_239,
        trap_240, trap_241, trap_242, trap_243, trap_244, trap_245, trap_246, trap_247,
        trap_248, trap_249, trap_250, trap_251, trap_252, trap_253, trap_254, trap_255,
        trap_256, trap_257, trap_258, trap_259, trap_260, trap_261, trap_262, trap_263,
        trap_264, trap_265, trap_266, trap_267, trap_268, trap_269, trap_270, trap_271,
        trap_272, trap_273, trap_274, trap_275, trap_276, trap_277, trap_278, trap_279,
        trap_280, trap_281, trap_282, trap_283, trap_284, trap_285, trap_286, trap_287,
        trap_288, trap_289, trap_290, trap_291, trap_292, trap_293, trap_294, trap_295,
        trap_296, trap_297, trap_298, trap_299, trap_300, trap_301, trap_302, trap_303,
        trap_304, trap_305, trap_306, trap_307, trap_308, trap_309, trap_310, trap_311,
        trap_312, trap_313, trap_314, trap_315, trap_316, trap_317, trap_318, trap_319,
        trap_320, trap_321, trap_322, trap_323, trap_324, trap_325, trap_326, trap_327,
        trap_328, trap_329, trap_330, trap_331, trap_332, trap_333, trap_334, trap_335,
        trap_336, trap_337, trap_338, trap_339, trap_340, trap_341, trap_342, trap_343,
        trap_344, trap_345, trap_346, trap_347, trap_348, trap_349, trap_350, trap_351,
        trap_352, trap_353, trap_354, trap_355, trap_356, trap_357, trap_358, trap_359,
        trap_360, trap_361, trap_362, trap_363, trap_364, trap_365, trap_366, trap_367,
        trap_368, trap_369, trap_370, trap_371, trap_372, trap_373, trap_374, trap_375,
        trap_376, trap_377, trap_378, trap_379, trap_380, trap_381, trap_382, trap_383,
        trap_384, trap_385, trap_386, trap_387, trap_388, trap_389, trap_390, trap_391,
        trap_392, trap_393, trap_394, trap_395, trap_396, trap_397, trap_398, trap_399,
        trap_400, trap_401, trap_402, trap_403, trap_404, trap_405, trap_406, trap_407,
        trap_408, trap_409, trap_410, trap_411, trap_412, trap_413, trap_414, trap_415,
        trap_416, trap_417, trap_418, trap_419, trap_420, trap_421, trap_422, trap_423,
        trap_424, trap_425, trap_426, trap_427, trap_428, trap_429, trap_430, trap_431,
        trap_432, trap_433, trap_434, trap_435, trap_436, trap_437, trap_438, trap_439,
        trap_440, trap_441, trap_442, trap_443, trap_444, trap_445, trap_446, trap_447,
        trap_448, trap_449, trap_450, trap_451, trap_452, trap_453, trap_454, trap_455,
        trap_456, trap_457, trap_458, trap_459, trap_460, trap_461, trap_462, trap_463,
        trap_464, trap_465, trap_466, trap_467, trap_468, trap_469, trap_470, trap_471,
        trap_472, trap_473, trap_474, trap_475, trap_476, trap_477, trap_478, trap_479,
        trap_480, trap_481, trap_482, trap_483, trap_484, trap_485, trap_486, trap_487,
        trap_488, trap_489, trap_490, trap_491, trap_492, trap_493, trap_494, trap_495,
        trap_496, trap_497, trap_498, trap_499, trap_500, trap_501, trap_502, trap_503,
        trap_504, trap_505, trap_506, trap_507, trap_508, trap_509, trap_510, trap_511,
    };
    for (int i = 0; i < TRAP_COUNT; i++) {
        char n[32];
        snprintf(n, sizeof n, "nvn:unknown proc %d", i);
        trap_addr[i] = hostfn_register(strdup(n), traps[i]);
    }
}

uint64_t nvn_proc_address(const char *name) {
    if (!name) return 0;
    const Proc *p = bsearch(&(Proc){name, 0}, procs, (size_t)nprocs, sizeof *procs, by_name);
    if (p) return p->addr;
    // Unknown names get a trap (see nvn_trap), one per distinct name; NULL once the pool is used up.
    static PlatMutex lock = PLAT_MUTEX_INIT;
    plat_mutex_lock(&lock);
    int i;
    for (i = 0; i < ntraps; i++)
        if (!strncmp(trap_name[i], name, sizeof trap_name[i] - 1)) break;
    int fresh = i == ntraps && ntraps < TRAP_COUNT && trap_addr[0];
    if (fresh) strncpy(trap_name[ntraps++], name, sizeof trap_name[0] - 1);
    uint64_t a = i < ntraps ? trap_addr[i] : 0;
    plat_mutex_unlock(&lock);
    if (fresh) rt_log("nvn: no such proc \"%s\": a trap that logs its first call", name);
    else if (!a) rt_log("nvn: no such proc \"%s\", returning NULL", name);
    return a;
}

// PFNNVNGENERICFUNC nvnDeviceGetProcAddress(const NVNdevice *device, const char *name)
void nvnp_nvnDeviceGetProcAddress(Cpu *c) {
    const char *name = arg_p(c, 1);
    ret_x(c, nvn_proc_address(name));
}
