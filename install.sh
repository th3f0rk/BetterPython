#!/bin/bash
set -e

# BetterPython Complete Installation Script
# Installs compiler, VM, LSP, TreeSitter, and Neovim integration

INSTALL_DIR="${INSTALL_DIR:-$HOME/.local}"
NEOVIM_CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/nvim"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    local missing=()
    
    command -v gcc >/dev/null 2>&1 || missing+=("gcc")
    command -v make >/dev/null 2>&1 || missing+=("make")
    command -v python3 >/dev/null 2>&1 || missing+=("python3")
    
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Install with: sudo apt-get install ${missing[*]}"
        exit 1
    fi
    
    log_success "All dependencies found"
}

install_compiler() {
    log_info "Building BetterPython compiler and VM..."
    
    cd "$SCRIPT_DIR"
    make clean
    make
    
    mkdir -p "$INSTALL_DIR/bin"
    cp bin/bpcc "$INSTALL_DIR/bin/"
    cp bin/bpvm "$INSTALL_DIR/bin/"
    cp bin/betterpython "$INSTALL_DIR/bin/"
    
    log_success "Compiler and VM installed to $INSTALL_DIR/bin"
}

install_tools() {
    log_info "Installing formatter, linter, and package manager..."
    
    mkdir -p "$INSTALL_DIR/bin"
    
    # Make tools executable
    chmod +x "$SCRIPT_DIR/formatter/bpfmt.py"
    chmod +x "$SCRIPT_DIR/linter/bplint.py"
    chmod +x "$SCRIPT_DIR/package-manager/bppkg.py"
    
    # Create wrapper scripts
    cat > "$INSTALL_DIR/bin/bpfmt" << 'WRAPPER'
#!/bin/bash
exec python3 "$(dirname "$(readlink -f "$0")")/../share/betterpython/formatter/bpfmt.py" "$@"
WRAPPER
    
    cat > "$INSTALL_DIR/bin/bplint" << 'WRAPPER'
#!/bin/bash
exec python3 "$(dirname "$(readlink -f "$0")")/../share/betterpython/linter/bplint.py" "$@"
WRAPPER
    
    cat > "$INSTALL_DIR/bin/bppkg" << 'WRAPPER'
#!/bin/bash
exec python3 "$(dirname "$(readlink -f "$0")")/../share/betterpython/package-manager/bppkg.py" "$@"
WRAPPER
    
    chmod +x "$INSTALL_DIR/bin/bpfmt"
    chmod +x "$INSTALL_DIR/bin/bplint"
    chmod +x "$INSTALL_DIR/bin/bppkg"
    
    # Copy Python scripts
    mkdir -p "$INSTALL_DIR/share/betterpython"
    cp -r "$SCRIPT_DIR/formatter" "$INSTALL_DIR/share/betterpython/"
    cp -r "$SCRIPT_DIR/linter" "$INSTALL_DIR/share/betterpython/"
    cp -r "$SCRIPT_DIR/package-manager" "$INSTALL_DIR/share/betterpython/"
    
    log_success "Tools installed"
}

install_lsp() {
    log_info "Installing LSP server..."
    
    mkdir -p "$INSTALL_DIR/share/betterpython/lsp-server"
    cp "$SCRIPT_DIR/lsp-server/betterpython_lsp.py" "$INSTALL_DIR/share/betterpython/lsp-server/"
    chmod +x "$INSTALL_DIR/share/betterpython/lsp-server/betterpython_lsp.py"
    
    log_success "LSP server installed"
}

install_treesitter() {
    log_info "Installing TreeSitter parser..."
    
    if ! command -v npm >/dev/null 2>&1; then
        log_warn "npm not found, skipping TreeSitter installation"
        log_info "Install Node.js to enable TreeSitter support"
        return
    fi
    
    cd "$SCRIPT_DIR/tree-sitter-betterpython"
    
    if [ ! -d "node_modules" ]; then
        npm install
    fi
    
    # Generate parser
    npx tree-sitter generate
    
    log_success "TreeSitter parser built"
}

install_neovim_config() {
    log_info "Installing Neovim configuration..."
    
    if ! command -v nvim >/dev/null 2>&1; then
        log_warn "Neovim not found, skipping Neovim configuration"
        return
    fi
    
    # Create Neovim data directory for BetterPython
    local nvim_data="$HOME/.local/share/nvim/betterpython"
    mkdir -p "$nvim_data"
    
    # Copy LSP server
    cp -r "$SCRIPT_DIR/lsp-server" "$nvim_data/"
    
    # Copy TreeSitter queries
    mkdir -p "$NEOVIM_CONFIG/queries/betterpython"
    cp "$SCRIPT_DIR/nvim-config/queries/betterpython/highlights.scm" \
       "$NEOVIM_CONFIG/queries/betterpython/"
    
    # Copy Lua configuration
    mkdir -p "$NEOVIM_CONFIG/lua"
    cp "$SCRIPT_DIR/nvim-config/betterpython.lua" \
       "$NEOVIM_CONFIG/lua/"
    
    log_success "Neovim configuration installed"
    log_info "Add to your init.lua: require('betterpython').setup()"
}

setup_path() {
    log_info "Setting up PATH..."
    
    local shell_rc=""
    if [ -n "$BASH_VERSION" ]; then
        shell_rc="$HOME/.bashrc"
    elif [ -n "$ZSH_VERSION" ]; then
        shell_rc="$HOME/.zshrc"
    fi
    
    if [ -n "$shell_rc" ]; then
        if ! grep -q "BetterPython" "$shell_rc"; then
            echo "" >> "$shell_rc"
            echo "# BetterPython" >> "$shell_rc"
            echo "export PATH=\"$INSTALL_DIR/bin:\$PATH\"" >> "$shell_rc"
            log_success "Added $INSTALL_DIR/bin to PATH in $shell_rc"
            log_warn "Run: source $shell_rc"
        else
            log_info "PATH already configured"
        fi
    fi
}

run_tests() {
    log_info "Running tests..."
    
    export PATH="$INSTALL_DIR/bin:$PATH"
    
    # Test compiler
    if ! command -v bpcc >/dev/null 2>&1; then
        log_error "bpcc not in PATH"
        return 1
    fi
    
    # Create test file
    local test_file="/tmp/test_betterpython.bp"
    cat > "$test_file" << 'TEST'
def main() -> int:
    print("BetterPython installation test")
    return 0
TEST
    
    # Compile and run
    if bpcc "$test_file" -o /tmp/test.bpc && bpvm /tmp/test.bpc; then
        log_success "Compiler and VM working"
    else
        log_error "Compiler or VM test failed"
        return 1
    fi
    
    # Test formatter
    if command -v bpfmt >/dev/null 2>&1; then
        log_success "Formatter installed"
    else
        log_warn "Formatter not found"
    fi
    
    # Test linter
    if command -v bplint >/dev/null 2>&1; then
        log_success "Linter installed"
    else
        log_warn "Linter not found"
    fi
    
    # Cleanup
    rm -f "$test_file" /tmp/test.bpc
}

print_summary() {
    echo ""
    echo "========================================"
    echo "  BetterPython Installation Complete"
    echo "========================================"
    echo ""
    echo "Installed to: $INSTALL_DIR"
    echo ""
    echo "Commands:"
    echo "  betterpython <file.bp>  - Compile and run"
    echo "  bpcc <file.bp>          - Compile to bytecode"
    echo "  bpvm <file.bpc>         - Run bytecode"
    echo "  bpfmt <file.bp>         - Format code"
    echo "  bplint <file.bp>        - Lint code"
    echo "  bppkg <command>         - Package manager"
    echo ""
    echo "Neovim:"
    echo "  Add to init.lua: require('betterpython').setup()"
    echo "  :BPRun                  - Run current file (F5)"
    echo "  :BPCompile              - Compile current file"
    echo "  :BPFormat               - Format current file"
    echo ""
    echo "LSP Features:"
    echo "  - Autocomplete (Ctrl+Space)"
    echo "  - Hover documentation (K)"
    echo "  - Go to definition (gd)"
    echo "  - Find references (gr)"
    echo "  - Diagnostics ([d, ]d)"
    echo ""
    echo "Examples: $SCRIPT_DIR/examples/"
    echo "Documentation: $SCRIPT_DIR/docs/"
    echo ""
}

main() {
    echo "BetterPython Installation Script"
    echo "================================="
    echo ""
    
    check_dependencies
    install_compiler
    install_tools
    install_lsp
    install_treesitter
    install_neovim_config
    setup_path
    run_tests
    print_summary
    
    log_success "Installation complete!"
}

main "$@"
