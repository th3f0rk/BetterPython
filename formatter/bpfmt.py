#!/usr/bin/env python3
"""
BetterPython Code Formatter
Automatically formats .bp files according to style guidelines
"""

import sys
import re
from typing import List

class BetterPythonFormatter:
    """Format BetterPython source code"""
    
    def __init__(self):
        self.indent_level = 0
        self.indent_str = "    "  # 4 spaces
        
    def format(self, source: str) -> str:
        """Format source code"""
        lines = source.split('\n')
        formatted = []
        
        for line in lines:
            stripped = line.strip()
            
            if not stripped or stripped.startswith('#'):
                formatted.append(stripped)
                continue
                
            # Decrease indent for certain keywords
            if self.should_dedent(stripped):
                self.indent_level = max(0, self.indent_level - 1)
                
            # Format the line
            formatted_line = self.format_line(stripped)
            indented = self.indent_str * self.indent_level + formatted_line
            formatted.append(indented)
            
            # Increase indent after colons
            if stripped.endswith(':'):
                self.indent_level += 1
                
        return '\n'.join(formatted)
        
    def should_dedent(self, line: str) -> bool:
        """Check if line should decrease indent"""
        return line.startswith(('elif', 'else', 'return'))
        
    def format_line(self, line: str) -> str:
        """Format single line"""
        # Add spaces around operators
        line = re.sub(r'([=+\-*/%<>!])=', r' \1= ', line)
        line = re.sub(r'([^=!<>])=([^=])', r'\1 = \2', line)
        line = re.sub(r'([+\-*/%])', r' \1 ', line)
        line = re.sub(r'(<|>|<=|>=|==|!=)', r' \1 ', line)
        
        # Clean up multiple spaces
        line = re.sub(r' +', ' ', line)
        
        # Fix spacing around colons and commas
        line = re.sub(r'\s*:\s*', ': ', line)
        line = re.sub(r'\s*,\s*', ', ', line)
        line = re.sub(r'\s*\(\s*', '(', line)
        line = re.sub(r'\s*\)', ')', line)
        
        # Special case for function return types
        line = re.sub(r'\) : ', ') -> ', line)
        line = re.sub(r'- > ', '-> ', line)
        
        return line.strip()

def main():
    if len(sys.argv) != 2:
        print("Usage: bpfmt <file.bp>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    try:
        with open(filepath, 'r') as f:
            source = f.read()
            
        formatter = BetterPythonFormatter()
        formatted = formatter.format(source)
        
        with open(filepath, 'w') as f:
            f.write(formatted)
            
        print(f"Formatted {filepath}")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
