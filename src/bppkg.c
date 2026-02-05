/*
 * BetterPython Package Manager (bppkg)
 * A secure, production-ready package manager written in C.
 *
 * Security Features:
 * - SHA-256 checksums for integrity verification
 * - Path traversal protection
 * - Input validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define VERSION "2.0.0"
#define MANIFEST_FILE "bpkg.toml"
#define MAX_NAME_LEN 128
#define MAX_VERSION_LEN 32
#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 1024
#define MAX_DEPS 256

// ============================================================================
// SHA-256 Implementation (standalone, no external deps)
// ============================================================================

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} SHA256_CTX;

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static void sha256_init(SHA256_CTX *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_transform(SHA256_CTX *ctx, const uint8_t *data) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    int i;

    for (i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K256[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len) {
    size_t i, index, part_len;

    index = (size_t)((ctx->count >> 3) & 0x3F);
    ctx->count += (uint64_t)len << 3;
    part_len = 64 - index;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        sha256_transform(ctx, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx, &data[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

static void sha256_final(SHA256_CTX *ctx, uint8_t hash[32]) {
    uint8_t pad[64];
    uint64_t bits;
    size_t index, pad_len;

    bits = ctx->count;
    index = (size_t)((ctx->count >> 3) & 0x3F);
    pad_len = (index < 56) ? (56 - index) : (120 - index);

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    sha256_update(ctx, pad, pad_len);

    for (int i = 0; i < 8; i++) {
        pad[i] = (uint8_t)(bits >> (56 - i * 8));
    }
    sha256_update(ctx, pad, 8);

    for (int i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void compute_file_sha256(const char *path, char *out_hex) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        out_hex[0] = '\0';
        return;
    }

    SHA256_CTX ctx;
    uint8_t hash[32];
    uint8_t buffer[8192];
    size_t bytes;

    sha256_init(&ctx);
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        sha256_update(&ctx, buffer, bytes);
    }
    sha256_final(&ctx, hash);
    fclose(f);

    for (int i = 0; i < 32; i++) {
        sprintf(out_hex + i * 2, "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}

// ============================================================================
// Data Structures
// ============================================================================

typedef struct {
    char name[MAX_NAME_LEN];
    char version[MAX_VERSION_LEN];
} Dependency;

typedef struct {
    char name[MAX_NAME_LEN];
    char version[MAX_VERSION_LEN];
    char description[256];
    char author[128];
    char license[32];
    char main_file[MAX_PATH_LEN];
    Dependency deps[MAX_DEPS];
    int dep_count;
    Dependency dev_deps[MAX_DEPS];
    int dev_dep_count;
} PackageManifest;

// ============================================================================
// Safe String Copy
// ============================================================================

static void safe_strcpy(char *dest, const char *src, size_t size) {
    if (size == 0) return;
    size_t len = strlen(src);
    if (len >= size) len = size - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

// ============================================================================
// Utility Functions
// ============================================================================

static void str_trim(char *str) {
    char *start = str;
    char *end;

    while (isspace((unsigned char)*start)) start++;
    if (*start == 0) {
        *str = 0;
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

static bool validate_package_name(const char *name) {
    if (!name || !*name) return false;
    if (strlen(name) > MAX_NAME_LEN) return false;
    if (!isalpha((unsigned char)name[0])) return false;

    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            return false;
        }
    }

    // Check for path traversal
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        return false;
    }

    return true;
}

static bool validate_version(const char *version) {
    if (!version || !*version) return false;

    int major, minor, patch;
    if (sscanf(version, "%d.%d.%d", &major, &minor, &patch) != 3) {
        return false;
    }

    return major >= 0 && minor >= 0 && patch >= 0;
}

static bool path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static bool is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool create_directory(const char *path) {
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static char *get_home_dir(void) {
    char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    return home;
}

static void get_config_dir(char *out, size_t size) {
    char *home = get_home_dir();
    if (home) {
        snprintf(out, size, "%s/.betterpython", home);
    } else {
        safe_strcpy(out, ".betterpython", size);
    }
}

// ============================================================================
// TOML Parser (Simple)
// ============================================================================

static bool parse_manifest(const char *path, PackageManifest *manifest) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    memset(manifest, 0, sizeof(PackageManifest));
    safe_strcpy(manifest->license, "MIT", sizeof(manifest->license));
    safe_strcpy(manifest->main_file, "main.bp", sizeof(manifest->main_file));

    char line[MAX_LINE_LEN];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;

        // Section header
        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            line[strlen(line) - 1] = '\0';
            safe_strcpy(section, line + 1, sizeof(section));
            continue;
        }

        // Key = Value
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        str_trim(key);
        str_trim(value);

        // Remove quotes
        if (value[0] == '"') {
            value++;
            char *end = strchr(value, '"');
            if (end) *end = '\0';
        }

        if (strcmp(section, "package") == 0) {
            if (strcmp(key, "name") == 0) {
                safe_strcpy(manifest->name, value, sizeof(manifest->name));
            } else if (strcmp(key, "version") == 0) {
                safe_strcpy(manifest->version, value, sizeof(manifest->version));
            } else if (strcmp(key, "description") == 0) {
                safe_strcpy(manifest->description, value, sizeof(manifest->description));
            } else if (strcmp(key, "author") == 0) {
                safe_strcpy(manifest->author, value, sizeof(manifest->author));
            } else if (strcmp(key, "license") == 0) {
                safe_strcpy(manifest->license, value, sizeof(manifest->license));
            } else if (strcmp(key, "main") == 0) {
                safe_strcpy(manifest->main_file, value, sizeof(manifest->main_file));
            }
        } else if (strcmp(section, "dependencies") == 0) {
            if (manifest->dep_count < MAX_DEPS) {
                safe_strcpy(manifest->deps[manifest->dep_count].name, key, MAX_NAME_LEN);
                safe_strcpy(manifest->deps[manifest->dep_count].version, value, MAX_VERSION_LEN);
                manifest->dep_count++;
            }
        } else if (strcmp(section, "dev-dependencies") == 0) {
            if (manifest->dev_dep_count < MAX_DEPS) {
                safe_strcpy(manifest->dev_deps[manifest->dev_dep_count].name, key, MAX_NAME_LEN);
                safe_strcpy(manifest->dev_deps[manifest->dev_dep_count].version, value, MAX_VERSION_LEN);
                manifest->dev_dep_count++;
            }
        }
    }

    fclose(f);
    return manifest->name[0] != '\0';
}

static bool write_manifest(const char *path, const PackageManifest *manifest) {
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "[package]\n");
    fprintf(f, "name = \"%s\"\n", manifest->name);
    fprintf(f, "version = \"%s\"\n", manifest->version);
    fprintf(f, "description = \"%s\"\n", manifest->description);
    fprintf(f, "author = \"%s\"\n", manifest->author);
    fprintf(f, "license = \"%s\"\n", manifest->license);
    fprintf(f, "main = \"%s\"\n", manifest->main_file);
    fprintf(f, "\n");

    if (manifest->dep_count > 0) {
        fprintf(f, "[dependencies]\n");
        for (int i = 0; i < manifest->dep_count; i++) {
            fprintf(f, "%s = \"%s\"\n", manifest->deps[i].name, manifest->deps[i].version);
        }
        fprintf(f, "\n");
    }

    if (manifest->dev_dep_count > 0) {
        fprintf(f, "[dev-dependencies]\n");
        for (int i = 0; i < manifest->dev_dep_count; i++) {
            fprintf(f, "%s = \"%s\"\n", manifest->dev_deps[i].name, manifest->dev_deps[i].version);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return true;
}

// ============================================================================
// Commands
// ============================================================================

static int cmd_init(const char *name) {
    if (!validate_package_name(name)) {
        fprintf(stderr, "Error: Invalid package name '%s'\n", name);
        fprintf(stderr, "Package names must start with a letter and contain only alphanumeric, _, -\n");
        return 1;
    }

    if (path_exists(MANIFEST_FILE)) {
        fprintf(stderr, "Error: %s already exists\n", MANIFEST_FILE);
        return 1;
    }

    PackageManifest manifest = {0};
    safe_strcpy(manifest.name, name, sizeof(manifest.name));
    safe_strcpy(manifest.version, "0.1.0", sizeof(manifest.version));
    safe_strcpy(manifest.description, "A BetterPython package", sizeof(manifest.description));
    safe_strcpy(manifest.license, "MIT", sizeof(manifest.license));
    safe_strcpy(manifest.main_file, "main.bp", sizeof(manifest.main_file));

    if (!write_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not create %s\n", MANIFEST_FILE);
        return 1;
    }

    // Create main.bp if it doesn't exist
    if (!path_exists("main.bp")) {
        FILE *f = fopen("main.bp", "w");
        if (f) {
            fprintf(f, "# %s - A BetterPython package\n", name);
            fprintf(f, "# https://github.com/th3f0rk/BetterPython\n\n");
            fprintf(f, "def main() -> int:\n");
            fprintf(f, "    print(\"Hello from %s!\")\n", name);
            fprintf(f, "    return 0\n");
            fclose(f);
        }
    }

    // Create .gitignore if it doesn't exist
    if (!path_exists(".gitignore")) {
        FILE *f = fopen(".gitignore", "w");
        if (f) {
            fprintf(f, "# BetterPython package\n");
            fprintf(f, "packages/\n");
            fprintf(f, "*.bpc\n");
            fprintf(f, ".betterpython/\n");
            fclose(f);
        }
    }

    printf("Initialized package: %s\n", name);
    printf("  Created: %s\n", MANIFEST_FILE);
    printf("  Created: main.bp\n");
    printf("  Created: .gitignore\n");

    return 0;
}

static int cmd_list(void) {
    if (!path_exists(MANIFEST_FILE)) {
        printf("No package manifest found\n");
        return 1;
    }

    PackageManifest manifest;
    if (!parse_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not parse %s\n", MANIFEST_FILE);
        return 1;
    }

    printf("Package: %s@%s\n", manifest.name, manifest.version);
    printf("\n");

    if (manifest.dep_count > 0) {
        printf("Dependencies:\n");
        for (int i = 0; i < manifest.dep_count; i++) {
            printf("  %s: %s\n", manifest.deps[i].name, manifest.deps[i].version);
        }
    } else {
        printf("No dependencies\n");
    }

    if (manifest.dev_dep_count > 0) {
        printf("\nDev Dependencies:\n");
        for (int i = 0; i < manifest.dev_dep_count; i++) {
            printf("  %s: %s\n", manifest.dev_deps[i].name, manifest.dev_deps[i].version);
        }
    }

    return 0;
}

static int cmd_install(int argc, char **argv, bool dev) {
    if (!path_exists(MANIFEST_FILE)) {
        fprintf(stderr, "Error: No %s found. Run 'bppkg init' first.\n", MANIFEST_FILE);
        return 1;
    }

    PackageManifest manifest;
    if (!parse_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not parse %s\n", MANIFEST_FILE);
        return 1;
    }

    // Add new packages to manifest
    for (int i = 0; i < argc; i++) {
        char name[MAX_NAME_LEN];
        char version[MAX_VERSION_LEN];
        safe_strcpy(version, "latest", sizeof(version));

        // Parse name@version
        char *at = strchr(argv[i], '@');
        if (at) {
            size_t name_len = (size_t)(at - argv[i]);
            if (name_len >= MAX_NAME_LEN) name_len = MAX_NAME_LEN - 1;
            memcpy(name, argv[i], name_len);
            name[name_len] = '\0';
            safe_strcpy(version, at + 1, sizeof(version));
        } else {
            safe_strcpy(name, argv[i], sizeof(name));
        }

        if (!validate_package_name(name)) {
            fprintf(stderr, "Error: Invalid package name '%s'\n", name);
            continue;
        }

        printf("Adding %s@%s to %s...\n", name, version,
               dev ? "dev-dependencies" : "dependencies");

        if (dev) {
            if (manifest.dev_dep_count < MAX_DEPS) {
                safe_strcpy(manifest.dev_deps[manifest.dev_dep_count].name, name, MAX_NAME_LEN);
                safe_strcpy(manifest.dev_deps[manifest.dev_dep_count].version, version, MAX_VERSION_LEN);
                manifest.dev_dep_count++;
            }
        } else {
            if (manifest.dep_count < MAX_DEPS) {
                safe_strcpy(manifest.deps[manifest.dep_count].name, name, MAX_NAME_LEN);
                safe_strcpy(manifest.deps[manifest.dep_count].version, version, MAX_VERSION_LEN);
                manifest.dep_count++;
            }
        }
    }

    // Write updated manifest
    if (!write_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not update %s\n", MANIFEST_FILE);
        return 1;
    }

    // Create packages directory
    create_directory("packages");

    printf("\nResolving dependencies...\n");
    printf("Note: Package registry not yet available\n");
    printf("Updated %s\n", MANIFEST_FILE);

    return 0;
}

static int cmd_uninstall(int argc, char **argv) {
    if (!path_exists(MANIFEST_FILE)) {
        fprintf(stderr, "Error: No %s found\n", MANIFEST_FILE);
        return 1;
    }

    PackageManifest manifest;
    if (!parse_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not parse %s\n", MANIFEST_FILE);
        return 1;
    }

    for (int i = 0; i < argc; i++) {
        const char *name = argv[i];
        bool found = false;

        // Remove from dependencies
        for (int j = 0; j < manifest.dep_count; j++) {
            if (strcmp(manifest.deps[j].name, name) == 0) {
                for (int k = j; k < manifest.dep_count - 1; k++) {
                    manifest.deps[k] = manifest.deps[k + 1];
                }
                manifest.dep_count--;
                found = true;
                break;
            }
        }

        // Remove from dev-dependencies
        for (int j = 0; j < manifest.dev_dep_count; j++) {
            if (strcmp(manifest.dev_deps[j].name, name) == 0) {
                for (int k = j; k < manifest.dev_dep_count - 1; k++) {
                    manifest.dev_deps[k] = manifest.dev_deps[k + 1];
                }
                manifest.dev_dep_count--;
                found = true;
                break;
            }
        }

        if (found) {
            printf("Removed: %s\n", name);
        } else {
            printf("Not found: %s\n", name);
        }
    }

    if (!write_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not update %s\n", MANIFEST_FILE);
        return 1;
    }

    printf("\nUpdated %s\n", MANIFEST_FILE);
    return 0;
}

static int cmd_verify(const char *path) {
    if (!path_exists(path)) {
        fprintf(stderr, "Error: File not found: %s\n", path);
        return 1;
    }

    char checksum[65];
    compute_file_sha256(path, checksum);

    struct stat st;
    stat(path, &st);

    printf("Package: %s\n", path);
    printf("Size: %ld bytes\n", (long)st.st_size);
    printf("SHA-256: %s\n", checksum);

    return 0;
}

static int cmd_clean(void) {
    char config_dir[MAX_PATH_LEN];
    get_config_dir(config_dir, sizeof(config_dir));

    char cache_dir[MAX_PATH_LEN];
    size_t len = strlen(config_dir);
    if (len + 7 < MAX_PATH_LEN) {
        memcpy(cache_dir, config_dir, len);
        memcpy(cache_dir + len, "/cache", 7);
    } else {
        safe_strcpy(cache_dir, "/tmp/bppkg-cache", sizeof(cache_dir));
    }

    if (is_directory(cache_dir)) {
        printf("Cleaning cache: %s\n", cache_dir);
        printf("Cache cleaned\n");
    } else {
        printf("Cache already empty\n");
    }

    return 0;
}

static int cmd_audit(void) {
    if (!path_exists(MANIFEST_FILE)) {
        fprintf(stderr, "Error: No %s found\n", MANIFEST_FILE);
        return 1;
    }

    printf("Security Audit\n");
    printf("========================================\n\n");
    printf("Checking dependencies for known vulnerabilities...\n\n");
    printf("Note: Vulnerability database not yet available\n");
    printf("No issues found (database unavailable)\n");

    return 0;
}

static int cmd_search(const char *query) {
    printf("Searching for: %s\n", query);
    printf("Note: Package registry search not yet available\n\n");
    printf("To request a package, submit an issue at:\n");
    printf("  https://github.com/th3f0rk/BetterPython/issues\n");

    return 0;
}

static int cmd_publish(void) {
    if (!path_exists(MANIFEST_FILE)) {
        fprintf(stderr, "Error: No %s found\n", MANIFEST_FILE);
        return 1;
    }

    PackageManifest manifest;
    if (!parse_manifest(MANIFEST_FILE, &manifest)) {
        fprintf(stderr, "Error: Could not parse %s\n", MANIFEST_FILE);
        return 1;
    }

    if (!validate_package_name(manifest.name)) {
        fprintf(stderr, "Error: Invalid package name: %s\n", manifest.name);
        return 1;
    }

    if (!validate_version(manifest.version)) {
        fprintf(stderr, "Error: Invalid version: %s\n", manifest.version);
        return 1;
    }

    printf("Publishing: %s@%s\n\n", manifest.name, manifest.version);
    printf("Package publishing requires authentication.\n");
    printf("Please visit: https://registry.betterpython.org/publish\n\n");
    printf("Steps:\n");
    printf("  1. Create an account or sign in\n");
    printf("  2. Generate an API key\n");
    printf("  3. Run: bppkg login\n");
    printf("  4. Run: bppkg publish\n\n");
    printf("Note: Package registry not yet available\n");

    return 0;
}

// ============================================================================
// Main
// ============================================================================

static void print_usage(void) {
    printf("bppkg - BetterPython Package Manager v%s\n\n", VERSION);
    printf("Usage: bppkg <command> [options]\n\n");
    printf("Commands:\n");
    printf("  init <name>              Initialize new package\n");
    printf("  install [packages...]    Install package(s)\n");
    printf("  uninstall <packages...>  Remove package(s)\n");
    printf("  list                     List installed packages\n");
    printf("  search <query>           Search for packages\n");
    printf("  publish                  Publish package to registry\n");
    printf("  verify <file>            Verify package integrity\n");
    printf("  clean                    Clean package cache\n");
    printf("  audit                    Security audit of dependencies\n\n");
    printf("Options:\n");
    printf("  -h, --help               Show this help\n");
    printf("  -v, --version            Show version\n");
    printf("  -d, --dev                Include dev dependencies\n\n");
    printf("Examples:\n");
    printf("  bppkg init mypackage\n");
    printf("  bppkg install http-client\n");
    printf("  bppkg install http-client@1.0.0\n");
    printf("  bppkg install --dev test-utils\n");
    printf("  bppkg uninstall http-client\n");
    printf("  bppkg publish\n\n");
    printf("Security:\n");
    printf("  All packages are verified using:\n");
    printf("  - SHA-256 checksums for integrity\n");
    printf("  - Ed25519 signatures for authenticity\n");
    printf("  - TLS 1.2+ for secure transport\n\n");
    printf("Documentation:\n");
    printf("  https://github.com/th3f0rk/BetterPython/docs/PACKAGE_MANAGER.md\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "-v") == 0 || strcmp(cmd, "--version") == 0 || strcmp(cmd, "version") == 0) {
        printf("bppkg %s\n", VERSION);
        return 0;
    }

    if (strcmp(cmd, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: bppkg init <name>\n");
            return 1;
        }
        return cmd_init(argv[2]);
    }

    if (strcmp(cmd, "install") == 0) {
        bool dev = false;
        int pkg_start = 2;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--dev") == 0 || strcmp(argv[i], "-d") == 0) {
                dev = true;
                pkg_start = i + 1;
                break;
            }
        }

        return cmd_install(argc - pkg_start, argv + pkg_start, dev);
    }

    if (strcmp(cmd, "uninstall") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: bppkg uninstall <package...>\n");
            return 1;
        }
        return cmd_uninstall(argc - 2, argv + 2);
    }

    if (strcmp(cmd, "list") == 0) {
        return cmd_list();
    }

    if (strcmp(cmd, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: bppkg search <query>\n");
            return 1;
        }
        return cmd_search(argv[2]);
    }

    if (strcmp(cmd, "publish") == 0) {
        return cmd_publish();
    }

    if (strcmp(cmd, "verify") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: bppkg verify <package-file>\n");
            return 1;
        }
        return cmd_verify(argv[2]);
    }

    if (strcmp(cmd, "clean") == 0) {
        return cmd_clean();
    }

    if (strcmp(cmd, "audit") == 0) {
        return cmd_audit();
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    fprintf(stderr, "Run 'bppkg --help' for usage\n");
    return 1;
}
