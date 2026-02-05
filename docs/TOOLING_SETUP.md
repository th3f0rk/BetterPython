# BetterPython Tooling Setup Guide

This guide covers how to set up development tooling for BetterPython in various environments, including editor integration, syntax highlighting, and language server support.

## Table of Contents

1. [Quick Installation](#quick-installation)
2. [Manual Installation](#manual-installation)
3. [Editor Setup](#editor-setup)
   - [Visual Studio Code](#visual-studio-code)
   - [Neovim](#neovim)
   - [Vim](#vim)
   - [Emacs](#emacs)
   - [Sublime Text](#sublime-text)
4. [Tree-sitter Grammar](#tree-sitter-grammar)
5. [LSP Server](#lsp-server)
6. [Formatter & Linter](#formatter--linter)
7. [Troubleshooting](#troubleshooting)

---

## Quick Installation

The easiest way to install BetterPython with all tooling is:

```bash
# Clone the repository
git clone https://github.com/th3f0rk/BetterPython.git
cd BetterPython

# Install everything
./install.sh

# Or with all tooling
./install.sh --all
```

This installs:
- `bpcc` - BetterPython compiler
- `bpvm` - BetterPython virtual machine
- `bprepl` - Interactive REPL
- `betterpython` - All-in-one wrapper script
- LSP server
- Tree-sitter grammar
- Formatter and linter

---

## Manual Installation

### Building from Source

```bash
# Prerequisites
# Ubuntu/Debian:
sudo apt-get install build-essential

# macOS:
xcode-select --install

# Build
cd BetterPython
make clean && make

# Binaries are in bin/
ls bin/
# bpcc  bpvm  bprepl  betterpython
```

### Install to System Path

```bash
# Option 1: User installation
mkdir -p ~/.local/bin
cp bin/* ~/.local/bin/
echo 'export PATH="$PATH:$HOME/.local/bin"' >> ~/.bashrc
source ~/.bashrc

# Option 2: System-wide installation (requires sudo)
sudo cp bin/* /usr/local/bin/
```

### Verify Installation

```bash
bpcc --version
bpvm --version
bprepl --help
```

---

## Editor Setup

### Visual Studio Code

1. **Install Prerequisites**
   - Install VS Code
   - Install Python extension (for LSP)

2. **Install Generic LSP Client**
   - Search for "Generic LSP Client" or similar in extensions
   - Or use the "vscode-languageclient" library

3. **Configure LSP** (`settings.json`):
   ```json
   {
     "lsp.serverPath": "betterpython-lsp",
     "lsp.args": [],
     "lsp.fileTypes": [".bp"]
   }
   ```

4. **Syntax Highlighting**
   Create `.vscode/betterpython.tmLanguage.json`:
   ```json
   {
     "name": "BetterPython",
     "scopeName": "source.bp",
     "fileTypes": ["bp"],
     "patterns": [
       {
         "name": "keyword.control.bp",
         "match": "\\b(if|elif|else|for|while|break|continue|return|def|let|class|new|super|extern|try|catch|finally|throw|import|export|struct|enum)\\b"
       },
       {
         "name": "storage.type.bp",
         "match": "\\b(int|float|bool|str|void|ptr)\\b"
       },
       {
         "name": "constant.language.bp",
         "match": "\\b(true|false)\\b"
       },
       {
         "name": "constant.numeric.bp",
         "match": "\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b"
       },
       {
         "name": "string.quoted.double.bp",
         "begin": "\"",
         "end": "\"",
         "patterns": [
           {"name": "constant.character.escape.bp", "match": "\\\\."}
         ]
       },
       {
         "name": "comment.line.bp",
         "match": "#.*$"
       }
     ]
   }
   ```

### Neovim

1. **Using Lazy.nvim (Recommended)**

   Add to your `init.lua`:
   ```lua
   {
     "th3f0rk/BetterPython",
     ft = "betterpython",
     config = function()
       require('betterpython').setup()
     end
   }
   ```

2. **Manual Setup**

   Create `~/.config/nvim/lua/betterpython.lua`:
   ```lua
   local M = {}

   function M.setup()
     -- File type detection
     vim.filetype.add({
       extension = {
         bp = "betterpython"
       }
     })

     -- LSP configuration
     local lspconfig = require('lspconfig')
     local configs = require('lspconfig.configs')

     if not configs.betterpython then
       configs.betterpython = {
         default_config = {
           cmd = { "betterpython-lsp" },
           filetypes = { "betterpython" },
           root_dir = function(fname)
             return lspconfig.util.find_git_ancestor(fname) or vim.fn.getcwd()
           end,
           settings = {},
         },
       }
     end

     lspconfig.betterpython.setup({
       on_attach = function(client, bufnr)
         -- Enable completion
         vim.api.nvim_buf_set_option(bufnr, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

         -- Keybindings
         local opts = { buffer = bufnr, noremap = true, silent = true }
         vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
         vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
         vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
         vim.keymap.set('n', '<leader>rn', vim.lsp.buf.rename, opts)
       end
     })

     -- Tree-sitter (if available)
     pcall(function()
       require('nvim-treesitter.parsers').get_parser_configs().betterpython = {
         install_info = {
           url = "~/.local/share/nvim/betterpython/tree-sitter-betterpython",
           files = { "src/parser.c" },
         },
         filetype = "betterpython",
       }
     end)

     -- Commands
     vim.api.nvim_create_user_command('BPRun', function()
       local file = vim.fn.expand('%:p')
       vim.cmd('!' .. 'betterpython ' .. file)
     end, {})

     vim.api.nvim_create_user_command('BPCompile', function()
       local file = vim.fn.expand('%:p')
       local out = vim.fn.expand('%:p:r') .. '.bpc'
       vim.cmd('!' .. 'bpcc ' .. file .. ' -o ' .. out)
     end, {})

     vim.api.nvim_create_user_command('BPFormat', function()
       local file = vim.fn.expand('%:p')
       vim.cmd('!' .. 'bpfmt ' .. file)
       vim.cmd('e!')
     end, {})

     -- Keybindings
     vim.keymap.set('n', '<F5>', ':BPRun<CR>', { buffer = true, silent = true })
   end

   return M
   ```

   Add to `init.lua`:
   ```lua
   require('betterpython').setup()
   ```

3. **Tree-sitter Queries**

   Create `~/.config/nvim/queries/betterpython/highlights.scm`:
   ```scheme
   ; Keywords
   ["if" "elif" "else" "for" "while" "break" "continue" "return"] @keyword.control
   ["def" "class" "struct" "enum" "let"] @keyword
   ["import" "export" "extern" "from"] @keyword.import
   ["try" "catch" "finally" "throw"] @keyword.exception
   ["new" "super" "self"] @keyword.operator
   ["in" "and" "or" "not"] @keyword.operator

   ; Types
   ["int" "float" "bool" "str" "void" "ptr"] @type.builtin

   ; Literals
   (integer) @number
   (float) @number.float
   (string) @string
   (boolean) @boolean
   (comment) @comment

   ; Functions
   (function_definition name: (identifier) @function)
   (call_expression function: (identifier) @function.call)

   ; Variables
   (identifier) @variable
   ```

### Vim

1. **Syntax Highlighting**

   Create `~/.vim/syntax/betterpython.vim`:
   ```vim
   " BetterPython syntax file

   if exists("b:current_syntax")
     finish
   endif

   " Keywords
   syn keyword bpKeyword if elif else for while break continue return
   syn keyword bpKeyword def let class new super extern
   syn keyword bpKeyword try catch finally throw
   syn keyword bpKeyword import export from struct enum

   " Types
   syn keyword bpType int float bool str void ptr

   " Boolean literals
   syn keyword bpBoolean true false

   " Numbers
   syn match bpNumber "\<\d\+\>"
   syn match bpFloat "\<\d\+\.\d*\([eE][+-]\?\d\+\)\?\>"

   " Strings
   syn region bpString start='"' end='"' contains=bpEscape
   syn match bpEscape "\\." contained

   " Comments
   syn match bpComment "#.*$"

   " Function definition
   syn match bpFunction "\<def\s\+\zs\w\+"

   " Highlighting
   hi def link bpKeyword Keyword
   hi def link bpType Type
   hi def link bpBoolean Boolean
   hi def link bpNumber Number
   hi def link bpFloat Float
   hi def link bpString String
   hi def link bpEscape SpecialChar
   hi def link bpComment Comment
   hi def link bpFunction Function

   let b:current_syntax = "betterpython"
   ```

2. **Filetype Detection**

   Create `~/.vim/ftdetect/betterpython.vim`:
   ```vim
   au BufRead,BufNewFile *.bp set filetype=betterpython
   ```

### Emacs

1. **Major Mode**

   Create `~/.emacs.d/betterpython-mode.el`:
   ```elisp
   ;;; betterpython-mode.el --- Major mode for BetterPython

   (defvar betterpython-mode-syntax-table
     (let ((st (make-syntax-table)))
       (modify-syntax-entry ?# "<" st)
       (modify-syntax-entry ?\n ">" st)
       (modify-syntax-entry ?\" "\"" st)
       st))

   (defvar betterpython-keywords
     '("if" "elif" "else" "for" "while" "break" "continue" "return"
       "def" "let" "class" "new" "super" "extern"
       "try" "catch" "finally" "throw"
       "import" "export" "from" "struct" "enum"))

   (defvar betterpython-types
     '("int" "float" "bool" "str" "void" "ptr"))

   (defvar betterpython-constants
     '("true" "false"))

   (defvar betterpython-font-lock-keywords
     `((,(regexp-opt betterpython-keywords 'words) . font-lock-keyword-face)
       (,(regexp-opt betterpython-types 'words) . font-lock-type-face)
       (,(regexp-opt betterpython-constants 'words) . font-lock-constant-face)
       ("\\<\\([0-9]+\\(\\.[0-9]*\\)?\\([eE][+-]?[0-9]+\\)?\\)\\>" . font-lock-constant-face)
       ("\\<def\\s-+\\([a-zA-Z_][a-zA-Z0-9_]*\\)" 1 font-lock-function-name-face)))

   (define-derived-mode betterpython-mode prog-mode "BetterPython"
     "Major mode for editing BetterPython code."
     :syntax-table betterpython-mode-syntax-table
     (setq-local font-lock-defaults '(betterpython-font-lock-keywords))
     (setq-local comment-start "# ")
     (setq-local comment-end "")
     (setq-local indent-tabs-mode nil)
     (setq-local tab-width 4))

   (add-to-list 'auto-mode-alist '("\\.bp\\'" . betterpython-mode))

   (provide 'betterpython-mode)
   ```

2. **Load in init.el**
   ```elisp
   (load "~/.emacs.d/betterpython-mode.el")
   ```

3. **LSP Mode Integration**
   ```elisp
   (with-eval-after-load 'lsp-mode
     (add-to-list 'lsp-language-id-configuration '(betterpython-mode . "betterpython"))
     (lsp-register-client
      (make-lsp-client
       :new-connection (lsp-stdio-connection '("betterpython-lsp"))
       :major-modes '(betterpython-mode)
       :server-id 'betterpython-lsp)))
   ```

### Sublime Text

1. **Syntax Definition**

   Create `BetterPython.sublime-syntax` in `~/.config/sublime-text/Packages/User/`:
   ```yaml
   %YAML 1.2
   ---
   name: BetterPython
   file_extensions: [bp]
   scope: source.betterpython

   contexts:
     main:
       - match: '#.*$'
         scope: comment.line.betterpython

       - match: '\b(if|elif|else|for|while|break|continue|return|def|let|class|new|super|extern|try|catch|finally|throw|import|export|struct|enum)\b'
         scope: keyword.control.betterpython

       - match: '\b(int|float|bool|str|void|ptr)\b'
         scope: storage.type.betterpython

       - match: '\b(true|false)\b'
         scope: constant.language.betterpython

       - match: '\b[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?\b'
         scope: constant.numeric.betterpython

       - match: '"'
         scope: punctuation.definition.string.begin.betterpython
         push: string

     string:
       - meta_scope: string.quoted.double.betterpython
       - match: '\\.'
         scope: constant.character.escape.betterpython
       - match: '"'
         scope: punctuation.definition.string.end.betterpython
         pop: true
   ```

---

## Tree-sitter Grammar

The BetterPython Tree-sitter grammar provides fast, incremental parsing for syntax highlighting and code analysis.

### Building the Parser

```bash
cd tree-sitter-betterpython

# Install dependencies
npm install

# Generate parser
npx tree-sitter generate

# Test grammar (optional)
npx tree-sitter test
```

### Using with Neovim

```lua
-- In your Neovim config
require('nvim-treesitter.parsers').get_parser_configs().betterpython = {
  install_info = {
    url = "/path/to/tree-sitter-betterpython",
    files = { "src/parser.c" },
  },
  filetype = "betterpython",
}

-- Install the parser
:TSInstall betterpython
```

### Grammar Features

The grammar supports:
- Function definitions with type annotations
- Class definitions with inheritance
- Extern (FFI) declarations
- Struct and enum definitions
- Control flow statements
- Exception handling (try/catch/finally)
- Type expressions (arrays, maps, tuples)
- String interpolation (f-strings)
- Comments

---

## LSP Server

The BetterPython LSP server provides:
- Autocompletion
- Hover documentation
- Go to definition
- Find references
- Diagnostics (errors/warnings)
- Symbol outline
- Formatting

### Installation

```bash
# Install Python dependencies
pip3 install pygls lsprotocol

# Install LSP server
cp lsp-server/*.py ~/.local/share/betterpython/lsp/

# Create wrapper script
cat > ~/.local/bin/betterpython-lsp << 'EOF'
#!/bin/bash
python3 ~/.local/share/betterpython/lsp/server.py "$@"
EOF
chmod +x ~/.local/bin/betterpython-lsp
```

### Starting the Server

```bash
# Start server (for testing)
betterpython-lsp

# With logging
betterpython-lsp --log-file /tmp/bp-lsp.log
```

### Capabilities

| Feature | Status |
|---------|--------|
| Completion | ✅ Full |
| Hover | ✅ Full |
| Go to Definition | ✅ Full |
| Find References | ✅ Full |
| Document Symbols | ✅ Full |
| Diagnostics | ✅ Full |
| Formatting | ✅ Full |
| Rename | 🔄 Partial |
| Code Actions | 🔄 Partial |

---

## Formatter & Linter

### Formatter (bpfmt)

Automatically formats BetterPython code:

```bash
# Format a file (in-place)
bpfmt file.bp

# Format and output to stdout
bpfmt --stdout file.bp

# Check formatting (for CI)
bpfmt --check file.bp
```

### Linter (bplint)

Checks code for potential issues:

```bash
# Lint a file
bplint file.bp

# Output as JSON
bplint --json file.bp

# Specific checks only
bplint --checks=unused,type-mismatch file.bp
```

### Lint Checks

| Check | Description |
|-------|-------------|
| `unused` | Unused variables and imports |
| `type-mismatch` | Type mismatches |
| `undefined` | Undefined references |
| `unreachable` | Unreachable code |
| `shadow` | Variable shadowing |
| `format` | Formatting issues |

---

## Troubleshooting

### Common Issues

**1. Command not found**
```bash
# Check PATH
echo $PATH | tr ':' '\n' | grep -E '(local|betterpython)'

# Add to PATH
export PATH="$PATH:$HOME/.local/bin"
```

**2. LSP not connecting**
```bash
# Test LSP server
betterpython-lsp --version

# Check Python dependencies
python3 -c "import pygls; print(pygls.__version__)"
```

**3. Tree-sitter parser not working**
```bash
# Regenerate parser
cd tree-sitter-betterpython
npx tree-sitter generate

# Check for errors
npx tree-sitter test
```

**4. Syntax highlighting not working**
- Verify file extension is `.bp`
- Check filetype detection in your editor
- Try restarting the editor

### Getting Help

- GitHub Issues: https://github.com/th3f0rk/BetterPython/issues
- Documentation: See `docs/` directory
- Examples: See `examples/` directory

---

## Quick Reference

### Commands

| Command | Description |
|---------|-------------|
| `bpcc file.bp -o out.bpc` | Compile to bytecode |
| `bpvm file.bpc` | Run bytecode |
| `betterpython file.bp` | Compile and run |
| `bprepl` | Start REPL |
| `bpfmt file.bp` | Format code |
| `bplint file.bp` | Lint code |
| `betterpython-lsp` | Start LSP server |

### File Extensions

| Extension | Description |
|-----------|-------------|
| `.bp` | BetterPython source code |
| `.bpc` | BetterPython bytecode |

### Editor Keybindings (Suggested)

| Key | Action |
|-----|--------|
| `F5` | Run current file |
| `K` | Show hover documentation |
| `gd` | Go to definition |
| `gr` | Find references |
| `Ctrl+Space` | Trigger completion |
