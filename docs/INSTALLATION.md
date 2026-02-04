# BetterPython Installation Guide

## System Requirements

### Minimum Requirements

- Operating System: Linux, macOS, or Unix-like system
- C Compiler: GCC 4.9+ or Clang 3.5+
- C Standard: C11 support required
- Make: GNU Make or compatible
- RAM: 64 MB minimum
- Disk: 10 MB for source, 2 MB for binaries

### Recommended Requirements

- Operating System: Ubuntu 20.04+, macOS 10.15+
- C Compiler: GCC 9.0+ or Clang 10.0+
- RAM: 256 MB for comfortable development
- Disk: 50 MB for full installation with examples

## Dependencies

### Required

- C11 compliant compiler
- POSIX-compliant system libraries
- Math library (libm)
- Standard C library

### Optional

- None (BetterPython is self-contained)

## Building from Source

### Quick Build

```bash
cd betterpython
make
```

This builds:
- bin/bpcc (compiler)
- bin/bpvm (virtual machine)
- bin/betterpython (wrapper script)

### Build Steps Explained

1. Compiler compiles all .c files in src/
2. Linker creates bpcc and bpvm executables
3. Wrapper script copied to bin/

### Build Configuration

Edit Makefile to customize:

```makefile
CC = gcc              # Change compiler
CFLAGS = -O2 -Wall    # Modify compilation flags
PREFIX = /usr/local   # Change install directory
```

### Compiler Flags

Default flags:
- -O2: Optimization level 2
- -Wall: All warnings
- -Wextra: Extra warnings
- -Werror: Treat warnings as errors
- -std=c11: C11 standard compliance

### Clean Build

Remove all build artifacts:

```bash
make clean
```

### Rebuild Everything

```bash
make clean
make
```

## Installation

### System-Wide Installation

Install to /usr/local (requires root):

```bash
sudo make install
```

This installs:
- /usr/local/bin/bpcc
- /usr/local/bin/bpvm
- /usr/local/bin/betterpython

### Custom Installation Directory

Install to custom location:

```bash
make install PREFIX=$HOME/.local
```

### User Installation

For single-user installation without root:

```bash
mkdir -p $HOME/bin
cp bin/* $HOME/bin/
export PATH=$HOME/bin:$PATH
```

Add to .bashrc or .zshrc for persistence:

```bash
echo 'export PATH=$HOME/bin:$PATH' >> ~/.bashrc
```

## Verification

### Test Installation

```bash
betterpython --version
```

Expected output:
```
BetterPython compiler and VM
```

### Run Test Program

```bash
echo 'def main() -> int: print("Hello") return 0' > test.bp
betterpython test.bp
```

Expected output:
```
Hello
```

### Run Test Suite

```bash
cd betterpython
./bin/betterpython tests/test_security.bp
./bin/betterpython tests/test_strings.bp
```

All tests should pass.

## Usage

### Compile and Run

Single command (recommended):

```bash
betterpython program.bp
```

### Compile Only

```bash
bpcc program.bp -o program.bpc
```

### Run Bytecode

```bash
bpvm program.bpc
```

### Multiple Files

BetterPython currently supports single-file programs only.
Workaround for multiple files:

```bash
cat module1.bp module2.bp main.bp > combined.bp
betterpython combined.bp
```

## Troubleshooting

### Compilation Errors

#### "command not found: make"

Install build tools:

Ubuntu/Debian:
```bash
sudo apt-get install build-essential
```

macOS:
```bash
xcode-select --install
```

#### "cc: command not found"

Install C compiler:

Ubuntu/Debian:
```bash
sudo apt-get install gcc
```

macOS:
```bash
xcode-select --install
```

#### "undefined reference to pow"

Math library not linked. Edit Makefile and ensure -lm flag is present.

#### "implicit declaration of function"

Missing header or wrong C standard. Ensure -std=c11 flag is set.

### Runtime Errors

#### "permission denied"

Make binaries executable:

```bash
chmod +x bin/*
```

#### "cannot open /dev/urandom"

System missing /dev/urandom. Security functions will fail.
Workaround: Use rand() instead of random_bytes().

#### "segmentation fault"

Possible causes:
- Stack overflow (very deep recursion)
- Bytecode corruption
- VM bug

Report with minimal reproduction case.

### Performance Issues

#### Slow Compilation

Normal for large files. Compilation is O(n) in source size.
Typical: 10000 lines compile in <100ms.

#### Slow Execution

VM is interpreter, not JIT. Expected performance:
- Simple loops: ~10M iterations/second
- String operations: ~1M operations/second
- File I/O: Limited by disk speed

## Uninstallation

### Remove System Installation

```bash
sudo rm /usr/local/bin/bpcc
sudo rm /usr/local/bin/bpvm
sudo rm /usr/local/bin/betterpython
```

### Remove User Installation

```bash
rm $HOME/bin/bpcc
rm $HOME/bin/bpvm
rm $HOME/bin/betterpython
```

### Remove Source

```bash
rm -rf betterpython/
```

## Platform-Specific Notes

### Linux

Works on all major distributions:
- Ubuntu/Debian
- Fedora/RHEL
- Arch Linux
- Alpine Linux

### macOS

Tested on:
- macOS 10.15 (Catalina)
- macOS 11 (Big Sur)
- macOS 12 (Monterey)
- macOS 13+ (Ventura and later)

### BSD

Should work on:
- FreeBSD
- OpenBSD
- NetBSD

May require minor Makefile adjustments.

### Windows

Not officially supported. Possible options:
- WSL (Windows Subsystem for Linux) - Recommended
- Cygwin
- MinGW

## Docker Installation

Create Dockerfile:

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y gcc make
COPY betterpython /opt/betterpython
WORKDIR /opt/betterpython
RUN make
ENV PATH="/opt/betterpython/bin:${PATH}"
CMD ["betterpython"]
```

Build and run:

```bash
docker build -t betterpython .
docker run -it betterpython
```

## Development Setup

### For Contributing

```bash
git clone <repository>
cd betterpython
make
./bin/betterpython examples/showcase.bp
```

### Editor Setup

#### VSCode

Create .vscode/tasks.json:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "run",
      "type": "shell",
      "command": "./bin/betterpython ${file}",
      "group": {
        "kind": "build",
        "isDefault": true
      }
    }
  ]
}
```

#### Vim

Add to .vimrc:

```vim
autocmd BufRead,BufNewFile *.bp set filetype=python
nnoremap <F5> :!betterpython %<CR>
```

#### Emacs

Add to .emacs:

```elisp
(add-to-list 'auto-mode-alist '("\\.bp\\'" . python-mode))
(global-set-key (kbd "<f5>") 
  '(lambda () (interactive) 
     (compile (concat "betterpython " buffer-file-name))))
```

## Performance Tuning

### Compilation Speed

For faster compilation:

```makefile
CFLAGS = -O0  # Disable optimization during development
```

For faster binaries:

```makefile
CFLAGS = -O3 -march=native -flto
```

### Binary Size

For smaller binaries:

```bash
strip bin/bpcc bin/bpvm
```

Typical sizes:
- Before strip: ~100 KB
- After strip: ~56 KB

## Updating

### Update to Latest Version

```bash
cd betterpython
git pull
make clean
make
sudo make install
```

### Version Check

Check installed version:

```bash
betterpython --version
```

## Getting Help

### Documentation

- README.md: Project overview
- docs/ARCHITECTURE.md: System design
- docs/FUNCTION_REFERENCE.md: All functions
- docs/SECURITY_API.md: Security functions
- docs/STRING_API.md: String functions

### Examples

See examples/ directory for working programs.

### Issues

Report bugs with:
- OS and version
- Compiler and version
- Error message
- Minimal reproduction code

---

End of Installation Guide
