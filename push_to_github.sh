#!/bin/bash
set -e

# Script to push BetterPython to GitHub
# Repository: https://github.com/th3f0rk/BetterPython.git

REPO_URL="https://github.com/th3f0rk/BetterPython.git"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "BetterPython GitHub Push Script"
echo "================================"
echo ""

cd "$SCRIPT_DIR"

# Check if git is installed
if ! command -v git >/dev/null 2>&1; then
    echo "Error: git is not installed"
    exit 1
fi

# Initialize git if not already
if [ ! -d ".git" ]; then
    echo "Initializing git repository..."
    git init
    git remote add origin "$REPO_URL"
fi

# Create .gitignore
cat > .gitignore << 'GITIGNORE'
# Build artifacts
*.o
*.bpc
bin/
build/
node_modules/
out/

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Logs
*.log
/tmp/

# Package manager
.betterpython/

# Python
__pycache__/
*.pyc
*.pyo

# TreeSitter generated
tree-sitter-betterpython/src/parser.c
tree-sitter-betterpython/src/tree_sitter/
tree-sitter-betterpython/binding.cc

# VSCode extension
vscode-betterpython/*.vsix
vscode-betterpython/out/
GITIGNORE

# Stage all files
echo "Staging files..."
git add -A

# Create commit
echo "Creating commit..."
git commit -m "Complete BetterPython language implementation

- Full compiler and VM
- 68 built-in functions with comprehensive stdlib
- Complete LSP server with diagnostics, completion, hover
- TreeSitter parser for syntax highlighting
- Neovim and VSCode integration
- Code formatter and linter
- Package manager
- Installation scripts
- Comprehensive documentation

Ready for educational use and development"

# Push to GitHub
echo ""
echo "Ready to push to: $REPO_URL"
echo "Press Enter to continue or Ctrl+C to cancel..."
read

echo "Pushing to GitHub..."
git branch -M main
git push -u origin main --force

echo ""
echo "✓ Successfully pushed to GitHub!"
echo ""
echo "Repository: https://github.com/th3f0rk/BetterPython"
echo ""
