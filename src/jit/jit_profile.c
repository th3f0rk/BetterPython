/*
 * BetterPython JIT Profiler
 * Tracks function call counts and identifies hot functions
 */

#include "jit.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

// Default code cache size (4MB)
#define DEFAULT_CACHE_SIZE (4 * 1024 * 1024)

void jit_init(JitContext *jit, size_t func_count) {
    memset(jit, 0, sizeof(*jit));

    // Allocate profile data for each function
    jit->profile_count = func_count;
    if (func_count > 0) {
        jit->profiles = bp_xmalloc(func_count * sizeof(JitProfile));
        memset(jit->profiles, 0, func_count * sizeof(JitProfile));
    }

    // Allocate executable code cache
    jit->cache_size = DEFAULT_CACHE_SIZE;
    jit->code_cache = mmap(NULL, jit->cache_size,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (jit->code_cache == MAP_FAILED) {
        jit->code_cache = NULL;
        jit->cache_size = 0;
        fprintf(stderr, "JIT: Failed to allocate executable memory\n");
    }
    jit->cache_used = 0;

    jit->enabled = (jit->code_cache != NULL);
    jit->debug = false;

    jit->total_compilations = 0;
    jit->total_executions = 0;
    jit->native_executions = 0;
    jit->interp_executions = 0;
}

void jit_shutdown(JitContext *jit) {
    // Free profile data
    if (jit->profiles) {
        for (size_t i = 0; i < jit->profile_count; i++) {
            free(jit->profiles[i].loop_counts);
        }
        free(jit->profiles);
    }

    // Free code cache
    if (jit->code_cache) {
        munmap(jit->code_cache, jit->cache_size);
    }

    memset(jit, 0, sizeof(*jit));
}

void jit_record_call(JitContext *jit, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) return;

    JitProfile *profile = &jit->profiles[func_idx];
    jit->total_executions++;

    // Already compiled or failed
    if (profile->state >= JIT_STATE_COMPILED) {
        return;
    }

    profile->call_count++;

    // Update state based on count
    if (profile->state == JIT_STATE_COLD) {
        if (profile->call_count >= JIT_FUNC_THRESHOLD / 2) {
            profile->state = JIT_STATE_WARM;
        }
    } else if (profile->state == JIT_STATE_WARM) {
        if (profile->call_count >= JIT_FUNC_THRESHOLD) {
            profile->state = JIT_STATE_HOT;
            if (jit->debug) {
                fprintf(stderr, "JIT: Function %u is hot (calls=%u)\n",
                        func_idx, profile->call_count);
            }
        }
    }
}

bool jit_should_compile(JitContext *jit, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) return false;

    JitProfile *profile = &jit->profiles[func_idx];
    return profile->state == JIT_STATE_HOT;
}

bool jit_has_native(JitContext *jit, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) return false;

    JitProfile *profile = &jit->profiles[func_idx];
    return profile->state == JIT_STATE_COMPILED && profile->native_code != NULL;
}

void *jit_get_native(JitContext *jit, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) return NULL;

    JitProfile *profile = &jit->profiles[func_idx];
    if (profile->state == JIT_STATE_COMPILED) {
        return profile->native_code;
    }
    return NULL;
}

void jit_invalidate(JitContext *jit, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) return;

    JitProfile *profile = &jit->profiles[func_idx];

    // Note: We don't actually free the native code from the cache
    // since the cache is a simple bump allocator. The code just becomes
    // unreachable. A more sophisticated implementation would track
    // and reclaim this memory.

    profile->native_code = NULL;
    profile->native_size = 0;
    profile->state = JIT_STATE_COLD;
    profile->call_count = 0;

    if (jit->debug) {
        fprintf(stderr, "JIT: Invalidated function %u\n", func_idx);
    }
}

void jit_print_stats(JitContext *jit) {
    printf("=== JIT Statistics ===\n");
    printf("Enabled: %s\n", jit->enabled ? "yes" : "no");
    printf("Functions tracked: %zu\n", jit->profile_count);
    printf("Code cache: %zu / %zu bytes (%.1f%%)\n",
           jit->cache_used, jit->cache_size,
           jit->cache_size > 0 ? 100.0 * jit->cache_used / jit->cache_size : 0.0);
    printf("Total compilations: %lu\n", (unsigned long)jit->total_compilations);
    printf("Total executions: %lu\n", (unsigned long)jit->total_executions);
    printf("Native executions: %lu (%.1f%%)\n",
           (unsigned long)jit->native_executions,
           jit->total_executions > 0
               ? 100.0 * jit->native_executions / jit->total_executions : 0.0);
    printf("Interpreted: %lu\n", (unsigned long)jit->interp_executions);

    // Count functions by state
    size_t cold = 0, warm = 0, hot = 0, compiled = 0, failed = 0;
    for (size_t i = 0; i < jit->profile_count; i++) {
        switch (jit->profiles[i].state) {
            case JIT_STATE_COLD: cold++; break;
            case JIT_STATE_WARM: warm++; break;
            case JIT_STATE_HOT: hot++; break;
            case JIT_STATE_COMPILED: compiled++; break;
            case JIT_STATE_FAILED: failed++; break;
            default: break;
        }
    }
    printf("Function states: cold=%zu warm=%zu hot=%zu compiled=%zu failed=%zu\n",
           cold, warm, hot, compiled, failed);
    printf("======================\n");
}

// Allocate space from the code cache
// Returns NULL if not enough space
void *jit_alloc_code(JitContext *jit, size_t size) {
    if (!jit->code_cache) return NULL;

    // Align to 16 bytes for performance
    size = (size + 15) & ~15;

    if (jit->cache_used + size > jit->cache_size) {
        fprintf(stderr, "JIT: Code cache full (%zu/%zu bytes)\n",
                jit->cache_used, jit->cache_size);
        return NULL;
    }

    void *ptr = jit->code_cache + jit->cache_used;
    jit->cache_used += size;
    return ptr;
}
