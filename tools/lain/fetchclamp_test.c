// fetchclamp_test.c: applies spirv_fetch_clamp (extracted from gfx_shader.c by this tool's generator) to one .spv: <in> <out>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int rt_knob(const char *n, const char *d) { (void)n; (void)d; return 1; }
#define rt_fatal(...) (fprintf(stderr, __VA_ARGS__), exit(2))
#define rt_log(...) ((void)0)
static int plat_inc32(volatile int32_t *p) { return ++*p; }
static void spirv_fetch_clamp(uint32_t **spirv, uint64_t *words) {
    enum { OP_EXT_IMPORT = 11, OP_EXT_INST = 12, OP_CAPABILITY = 17, OP_TYPE_INT = 21, OP_TYPE_VECTOR = 23, OP_TYPE_IMAGE = 25,
           OP_CONSTANT = 43, OP_CONSTANT_COMPOSITE = 44, OP_CONSTANT_NULL = 46, OP_FUNCTION = 54, OP_LOAD = 61, OP_COPY = 83,
           OP_IMAGE_FETCH = 95, OP_IMAGE = 100, OP_QUERY_SIZE_LOD = 103, OP_QUERY_SIZE = 104, OP_ISUB = 130,
           CAP_IMAGE_QUERY = 50, DIM_3D = 2, DIM_CUBE = 3, DIM_RECT = 4, DIM_BUFFER = 5, MASK_LOD = 2, GLSL_SCLAMP = 45 };
    static int on = -1;
    if (on < 0) on = rt_knob("FETCH_CLAMP", "1");
    if (!on) return;
    const uint32_t *w = *spirv;
    uint64_t n = *words, first_cap = 0, first_func = 0, fetches = 0;
    uint32_t bound = w[3], glsl = 0, int_s = 0, vec[5] = {0}, has_query = 0;
    uint32_t *type_of = calloc(bound, 4), *image_info = calloc(bound, 4);   // image_info: comps | dim << 8 | ms << 16, plus 1
    if (!type_of || !image_info) rt_fatal("gfx_shader: out of memory");
    for (uint64_t i = 5; i < n; i += w[i] >> 16) {
        uint32_t op = w[i] & 0xffff;
        if (op == OP_CAPABILITY) {
            if (!first_cap) first_cap = i;
            has_query |= w[i + 1] == CAP_IMAGE_QUERY;
        } else if (op == OP_EXT_IMPORT && !strcmp((const char *)(w + i + 2), "GLSL.std.450")) glsl = w[i + 1];
        else if (op == OP_TYPE_INT && w[i + 2] == 32 && w[i + 3] == 1) int_s = w[i + 1];
        else if (op == OP_TYPE_VECTOR && w[i + 2] == int_s && int_s && w[i + 3] <= 4) vec[w[i + 3]] = w[i + 1];
        else if (op == OP_TYPE_IMAGE) {
            uint32_t dim = w[i + 3], comps = dim == DIM_3D ? 3 : dim == DIM_CUBE ? 0 : dim == 1 || dim == DIM_RECT ? 2 : 1;
            image_info[w[i + 1]] = (comps ? comps + w[i + 5] : 0) | dim << 8 | w[i + 6] << 16 | 1u << 24;
        } else if (op == OP_LOAD || op == OP_IMAGE || op == OP_COPY) type_of[w[i + 2]] = w[i + 1];
        else if (op == OP_FUNCTION && !first_func) first_func = i;
        else if (op == OP_IMAGE_FETCH) fetches++;
    }
    vec[1] = int_s;
    if (!fetches || !glsl || !int_s || !first_cap || !first_func) goto out;
    uint32_t *o = malloc((n + 40 + fetches * 18) * 4);
    if (!o) rt_fatal("gfx_shader: out of memory");
    uint32_t zero[5], one[5], lod0 = bound++, one1 = bound++;
    for (int c = 1; c <= 4; c++) zero[c] = bound++, one[c] = c == 1 ? one1 : bound++;
    uint64_t k = 0;
    memcpy(o, w, 5 * 4), k = 5;
    for (uint64_t i = 5; i < n; i += w[i] >> 16) {
        uint32_t op = w[i] & 0xffff, len = w[i] >> 16;
        if (i == first_cap && !has_query) o[k++] = 2u << 16 | OP_CAPABILITY, o[k++] = CAP_IMAGE_QUERY;
        if (i == first_func) {
            o[k++] = 4u << 16 | OP_CONSTANT, o[k++] = int_s, o[k++] = lod0, o[k++] = 0;
            o[k++] = 4u << 16 | OP_CONSTANT, o[k++] = int_s, o[k++] = one1, o[k++] = 1;
            for (int c = 1; c <= 4; c++) {
                if (!vec[c]) continue;
                o[k++] = 3u << 16 | OP_CONSTANT_NULL, o[k++] = vec[c], o[k++] = zero[c];
                if (c == 1) continue;
                o[k++] = (uint32_t)(3 + c) << 16 | OP_CONSTANT_COMPOSITE, o[k++] = vec[c], o[k++] = one[c];
                for (int d = 0; d < c; d++) o[k++] = one1;
            }
        }
        uint32_t info = 0, comps = 0;
        if (op == OP_IMAGE_FETCH) {
            info = type_of[w[i + 3]] ? image_info[type_of[w[i + 3]]] : 0;
            comps = info & 0xff;
        }
        if (!info || comps < 1 || comps > 4 || !vec[comps]) {   // not a fetch, or one this cannot name the types of
            memcpy(o + k, w + i, len * 4), k += len;
            continue;
        }
        uint32_t ty = vec[comps], dim = info >> 8 & 0xff, ms = info >> 16 & 0xff, size = bound++, hi = bound++, cl = bound++;
        int by_size = ms || dim == DIM_RECT || dim == DIM_BUFFER;
        uint32_t mask = len > 5 ? w[i + 5] : 0, lod = mask & MASK_LOD ? w[i + 6] : lod0;
        if (by_size) o[k++] = 4u << 16 | OP_QUERY_SIZE, o[k++] = ty, o[k++] = size, o[k++] = w[i + 3];
        else o[k++] = 5u << 16 | OP_QUERY_SIZE_LOD, o[k++] = ty, o[k++] = size, o[k++] = w[i + 3], o[k++] = lod;
        o[k++] = 5u << 16 | OP_ISUB, o[k++] = ty, o[k++] = hi, o[k++] = size, o[k++] = one[comps];
        o[k++] = 8u << 16 | OP_EXT_INST, o[k++] = ty, o[k++] = cl, o[k++] = glsl, o[k++] = GLSL_SCLAMP, o[k++] = w[i + 4];
        o[k++] = zero[comps], o[k++] = hi;
        memcpy(o + k, w + i, len * 4), o[k + 4] = cl, k += len;
    }
    o[3] = bound;
    free(*spirv);
    *spirv = o, *words = k;
    static volatile int32_t marked;
    int32_t c = plat_inc32(&marked);
    if (c == 1 || c % 500 == 0) rt_log("gfx_shader: %d modules have their image fetches clamped so far", c);
out:
    free(type_of), free(image_info);
}


int main(int c, char **v) {
    FILE *f = fopen(v[1], "rb"); if (!f) return 3;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint32_t *w = malloc(n); fread(w, 1, n, f); fclose(f);
    uint64_t words = n / 4; uint32_t *p = w;
    spirv_fetch_clamp(&p, &words);
    f = fopen(v[2], "wb"); fwrite(p, 4, words, f); fclose(f);
    printf("%lld", (long long)(words - n / 4));
    return 0;
}
