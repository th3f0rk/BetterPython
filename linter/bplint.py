#!/usr/bin/env python3
"""
BetterPython Linter
Static analysis tool for detecting code issues
"""

import sys
import re
from typing import List, Tuple

class Issue:
    def __init__(self, line: int, severity: str, message: str, code: str):
        self.line = line
        self.severity = severity  # error, warning, info
        self.message = message
        self.code = code  # E001, W001, etc.
        
    def __str__(self):
        return f"{self.line}: {self.severity.upper()} [{self.code}] {self.message}"

class BetterPythonLinter:
    """Lint BetterPython source code"""
    
    def __init__(self):
        self.issues = []
        
    def lint(self, source: str) -> List[Issue]:
        """Perform linting"""
        lines = source.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            self.check_line(line_num, line, lines)
            
        return self.issues
        
    def check_line(self, line_num: int, line: str, all_lines: List[str]):
        """Check single line"""
        stripped = line.strip()
        
        # E001: Line too long
        if len(line) > 100:
            self.issues.append(Issue(
                line_num, 'warning', 
                f'Line too long ({len(line)} > 100 characters)', 
                'E001'
            ))
            
        # E002: Trailing whitespace
        if line != line.rstrip():
            self.issues.append(Issue(
                line_num, 'info',
                'Trailing whitespace',
                'E002'
            ))
            
        # E003: Missing type annotation
        if 'let' in stripped and ':' not in stripped:
            self.issues.append(Issue(
                line_num, 'error',
                'Variable declaration missing type annotation',
                'E003'
            ))
            
        # E004: Function missing return type
        if stripped.startswith('def ') and '->' not in stripped:
            self.issues.append(Issue(
                line_num, 'error',
                'Function missing return type annotation',
                'E004'
            ))
            
        # W001: Unused variable (simple check)
        var_match = re.match(r'\s*let\s+(\w+)', line)
        if var_match:
            var_name = var_match.group(1)
            # Check if variable is used later
            used = False
            for future_line in all_lines[line_num:]:
                if var_name in future_line and 'let' not in future_line:
                    used = True
                    break
            if not used:
                self.issues.append(Issue(
                    line_num, 'warning',
                    f"Variable '{var_name}' declared but never used",
                    'W001'
                ))
                
        # W002: Multiple statements on one line (except colons)
        if ';' in stripped:
            self.issues.append(Issue(
                line_num, 'warning',
                'Multiple statements on one line',
                'W002'
            ))
            
        # I001: Use of deprecated function
        deprecated = ['file_write']  # Example
        for func in deprecated:
            if re.search(rf'\b{func}\s*\(', line):
                self.issues.append(Issue(
                    line_num, 'info',
                    f"Function '{func}' is deprecated",
                    'I001'
                ))

def main():
    if len(sys.argv) != 2:
        print("Usage: bplint <file.bp>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    try:
        with open(filepath, 'r') as f:
            source = f.read()
            
        linter = BetterPythonLinter()
        issues = linter.lint(source)
        
        if not issues:
            print(f"{filepath}: No issues found")
            sys.exit(0)
            
        for issue in issues:
            print(f"{filepath}:{issue}")
            
        errors = sum(1 for i in issues if i.severity == 'error')
        warnings = sum(1 for i in issues if i.severity == 'warning')
        infos = sum(1 for i in issues if i.severity == 'info')
        
        print(f"\nFound {errors} errors, {warnings} warnings, {infos} info messages")
        sys.exit(1 if errors > 0 else 0)
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
