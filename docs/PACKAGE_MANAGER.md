# BetterPython Package Manager (bppkg)

A secure, production-ready package manager for BetterPython with industry-standard security practices.

## Overview

bppkg is designed with security as the top priority. All packages are verified using:
- **SHA-256 checksums** for integrity verification
- **Ed25519 signatures** for authenticity verification
- **TLS 1.2+** for secure transport
- **Path traversal protection** for safe file operations
- **Lockfile** for reproducible builds

## Quick Start

### Initialize a New Package

```bash
bppkg init mypackage
```

This creates:
- `bpkg.toml` - Package manifest
- `main.bp` - Entry point
- `.gitignore` - Git ignore file

### Install Dependencies

```bash
# Install a package
bppkg install http-client

# Install specific version
bppkg install http-client@1.0.0

# Install dev dependency
bppkg install --dev test-utils

# Install all dependencies from manifest
bppkg install
```

### List Packages

```bash
bppkg list
```

### Uninstall Packages

```bash
bppkg uninstall http-client
```

## Package Manifest (bpkg.toml)

```toml
[package]
name = "mypackage"
version = "0.1.0"
description = "A BetterPython package"
author = "Your Name"
license = "MIT"
repository = "https://github.com/user/mypackage"
homepage = "https://mypackage.example.com"
keywords = ["utility", "tools"]
main = "main.bp"
min_bp_version = "1.0.0"

[dependencies]
http-client = "^1.0.0"
json-utils = ">=2.0.0"

[dev-dependencies]
test-utils = "*"

[scripts]
build = "bpcc main.bp -o main.bpc"
test = "betterpython tests/test.bp"
```

## Version Specification

| Spec | Meaning |
|------|---------|
| `1.0.0` | Exact version |
| `^1.0.0` | Compatible (same major version) |
| `~1.0.0` | Approximately equivalent (same major.minor) |
| `>=1.0.0` | Greater than or equal |
| `<=1.0.0` | Less than or equal |
| `>1.0.0` | Greater than |
| `<1.0.0` | Less than |
| `*` or `latest` | Any version |

## Lockfile (bpkg.lock)

The lockfile ensures reproducible builds by recording exact versions and checksums of all installed packages.

```json
{
  "version": 1,
  "generated": "2026-02-05T12:00:00Z",
  "packages": {
    "http-client": {
      "name": "http-client",
      "version": "1.2.3",
      "checksum": "abc123...",
      "checksum_algorithm": "sha256",
      "source": "https://registry.betterpython.org/packages/http-client-1.2.3.tar.gz",
      "dependencies": {}
    }
  }
}
```

## Security Features

### Package Signing

All packages in the registry are signed with Ed25519 signatures. When you install a package, bppkg:

1. Downloads the package
2. Verifies the SHA-256 checksum
3. Verifies the Ed25519 signature against trusted keys
4. Extracts the package only if both verifications pass

### Key Management

```bash
# Generate a signing keypair
bppkg keygen my-key

# Trust a public key
bppkg trust author-key /path/to/author.pub

# Verify a package manually
bppkg verify package.tar.gz
```

### Security Audit

```bash
bppkg audit
```

Checks dependencies for known vulnerabilities (when vulnerability database is available).

### TLS Configuration

bppkg enforces:
- TLS 1.2 minimum version
- Certificate verification
- Hostname verification
- No IP addresses in URLs (SSRF protection)

### Input Validation

All inputs are validated:
- Package names: alphanumeric with `_` and `-`, max 128 chars
- Versions: semantic versioning (semver)
- Paths: checked for directory traversal attacks

## Publishing Packages

### Prerequisites

1. Create an account at https://registry.betterpython.org
2. Generate an API key
3. Run `bppkg login`

### Publish

```bash
bppkg publish
```

This will:
1. Validate the manifest
2. Create a tarball of your package
3. Sign the package with your key
4. Upload to the registry

### Package Requirements

- Valid `bpkg.toml` manifest
- Valid semver version
- All files referenced in manifest must exist
- No sensitive files (`.env`, credentials, etc.)

## CLI Reference

### Commands

| Command | Description |
|---------|-------------|
| `init <name>` | Initialize new package |
| `install [packages...]` | Install package(s) |
| `uninstall <packages...>` | Remove package(s) |
| `list` | List installed packages |
| `search <query>` | Search for packages |
| `publish` | Publish package to registry |
| `verify <file>` | Verify package integrity |
| `clean` | Clean package cache |
| `audit` | Security audit of dependencies |

### Key Management

| Command | Description |
|---------|-------------|
| `keygen <key-id>` | Generate signing keypair |
| `trust <key-id> <pubkey>` | Add trusted public key |

### Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help |
| `-v, --version` | Show version |
| `-d, --dev` | Include dev dependencies |

## Directory Structure

```
~/.betterpython/
├── packages/           # Cached packages
├── keys/              # Signing keys
│   ├── my-key.key     # Private key (0600 permissions)
│   └── my-key.pub     # Public key
├── cache/             # Download cache
├── config.json        # Configuration
└── trusted_keys.json  # Trusted public keys
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `BPPKG_REGISTRY` | Override default registry URL |
| `BPPKG_NO_VERIFY` | Disable signature verification (NOT RECOMMENDED) |

## Troubleshooting

### "Package signed by untrusted key"

The package was signed with a key you haven't trusted. Either:
1. Trust the key: `bppkg trust author-key /path/to/key.pub`
2. Contact the package author for their public key

### "Checksum mismatch"

The downloaded package doesn't match its expected checksum. This could indicate:
- Network corruption during download
- Tampered package
- Registry issue

Try clearing the cache and reinstalling: `bppkg clean && bppkg install`

### "SSL error"

Certificate verification failed. This could indicate:
- Man-in-the-middle attack
- Outdated CA certificates
- Network issues

Ensure your system has up-to-date CA certificates.

### "Network error"

Cannot connect to the registry. Check:
- Internet connection
- Firewall settings
- Registry status

## Security Best Practices

1. **Never disable signature verification** in production
2. **Keep keys secure** - never commit private keys to version control
3. **Use specific versions** instead of wildcards in production
4. **Regularly audit** dependencies with `bppkg audit`
5. **Review lockfile** changes before committing
6. **Trust keys carefully** - verify key ownership before trusting

## Contributing

To contribute to the package registry or bppkg:
- https://github.com/th3f0rk/BetterPython/issues

## Version History

### v2.0.0 (Current)
- Ed25519 signature verification
- SHA-256 checksums
- TLS 1.2+ enforcement
- Lockfile support
- Path traversal protection
- Input validation
- Security audit command

### v1.0.0
- Initial release
- Basic install/uninstall
