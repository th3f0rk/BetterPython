# INSTALL

## Requirements

- A C compiler (clang or gcc)
- make
- POSIX environment (Linux/macOS). Windows is possible via MSYS2/WSL, but not documented here.

## Build

From the repo root:

```bash
make
```

This produces:

- `bin/bpcc`
- `bin/bpvm`
- `bin/betterpython`

## Run an example

```bash
./bin/bpcc examples/hello.bp -o /tmp/hello.bpc
./bin/bpvm /tmp/hello.bpc
```

Or:

```bash
./bin/betterpython examples/hello.bp
```

## Install (optional)

```bash
sudo make install
```

Defaults to `/usr/local/bin`. Override with:

```bash
make PREFIX=$HOME/.local install
```

## Usage

### Compile

```bash
bpcc input.bp -o output.bpc
```

### Run

```bash
bpvm output.bpc
```

### Compile + run

```bash
betterpython input.bp
```

## Debug tips

- Build with assertions and debug symbols:

```bash
make clean
make CFLAGS='-O0 -g -Wall -Wextra -Werror'
```

