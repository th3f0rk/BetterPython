#!/usr/bin/env python3
"""
BetterPython Language Server Protocol Implementation
Provides IDE features: autocomplete, diagnostics, hover, go-to-definition
"""

import json
import sys
import os
import re
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
from pathlib import Path

# LSP Protocol Types
@dataclass
class Position:
    line: int
    character: int

@dataclass
class Range:
    start: Position
    end: Position

@dataclass
class Location:
    uri: str
    range: Range

@dataclass
class Diagnostic:
    range: Range
    severity: int  # 1=Error, 2=Warning, 3=Info, 4=Hint
    message: str
    source: str = "betterpython"

@dataclass
class CompletionItem:
    label: str
    kind: int  # 1=Text, 3=Function, 5=Field, 6=Variable, etc.
    detail: str
    documentation: str

# Built-in function signatures
BUILTINS = {
    # I/O
    "print": {"params": ["..."], "return": "void", "doc": "Print values to stdout"},
    "read_line": {"params": [], "return": "str", "doc": "Read line from stdin"},
    
    # String operations
    "len": {"params": ["s: str"], "return": "int", "doc": "Get string length"},
    "substr": {"params": ["s: str", "start: int", "length: int"], "return": "str", "doc": "Extract substring"},
    "str_upper": {"params": ["s: str"], "return": "str", "doc": "Convert to uppercase"},
    "str_lower": {"params": ["s: str"], "return": "str", "doc": "Convert to lowercase"},
    "str_trim": {"params": ["s: str"], "return": "str", "doc": "Remove whitespace"},
    "str_reverse": {"params": ["s: str"], "return": "str", "doc": "Reverse string"},
    "str_repeat": {"params": ["s: str", "count: int"], "return": "str", "doc": "Repeat string"},
    "str_pad_left": {"params": ["s: str", "width: int", "pad: str"], "return": "str", "doc": "Pad left"},
    "str_pad_right": {"params": ["s: str", "width: int", "pad: str"], "return": "str", "doc": "Pad right"},
    "str_contains": {"params": ["haystack: str", "needle: str"], "return": "bool", "doc": "Check substring"},
    "str_count": {"params": ["haystack: str", "needle: str"], "return": "int", "doc": "Count occurrences"},
    "str_find": {"params": ["haystack: str", "needle: str"], "return": "int", "doc": "Find substring"},
    "str_replace": {"params": ["s: str", "old: str", "new: str"], "return": "str", "doc": "Replace substring"},
    "str_char_at": {"params": ["s: str", "index: int"], "return": "str", "doc": "Get character"},
    "starts_with": {"params": ["s: str", "prefix: str"], "return": "bool", "doc": "Check prefix"},
    "ends_with": {"params": ["s: str", "suffix: str"], "return": "bool", "doc": "Check suffix"},
    "to_str": {"params": ["value: any"], "return": "str", "doc": "Convert to string"},
    
    # Character ops
    "chr": {"params": ["code: int"], "return": "str", "doc": "ASCII to character"},
    "ord": {"params": ["c: str"], "return": "int", "doc": "Character to ASCII"},
    
    # Math
    "abs": {"params": ["n: int"], "return": "int", "doc": "Absolute value"},
    "min": {"params": ["a: int", "b: int"], "return": "int", "doc": "Minimum"},
    "max": {"params": ["a: int", "b: int"], "return": "int", "doc": "Maximum"},
    "pow": {"params": ["base: int", "exp: int"], "return": "int", "doc": "Power"},
    "sqrt": {"params": ["n: int"], "return": "int", "doc": "Square root"},
    "clamp": {"params": ["value: int", "min: int", "max: int"], "return": "int", "doc": "Clamp value"},
    "sign": {"params": ["n: int"], "return": "int", "doc": "Sign of number"},
    
    # Random
    "rand": {"params": [], "return": "int", "doc": "Random integer 0-32767"},
    "rand_range": {"params": ["min: int", "max: int"], "return": "int", "doc": "Random in range"},
    "rand_seed": {"params": ["seed: int"], "return": "void", "doc": "Seed RNG"},
    
    # Security
    "hash_sha256": {"params": ["data: str"], "return": "str", "doc": "SHA-256 hash"},
    "hash_md5": {"params": ["data: str"], "return": "str", "doc": "MD5 hash"},
    "secure_compare": {"params": ["a: str", "b: str"], "return": "bool", "doc": "Constant-time compare"},
    "random_bytes": {"params": ["length: int"], "return": "str", "doc": "Crypto random bytes"},
    
    # Encoding
    "base64_encode": {"params": ["data: str"], "return": "str", "doc": "Base64 encode"},
    "base64_decode": {"params": ["data: str"], "return": "str", "doc": "Base64 decode"},
    "int_to_hex": {"params": ["n: int"], "return": "str", "doc": "Integer to hex"},
    "hex_to_int": {"params": ["hex: str"], "return": "int", "doc": "Hex to integer"},
    
    # Validation
    "is_digit": {"params": ["s: str"], "return": "bool", "doc": "Check if all digits"},
    "is_alpha": {"params": ["s: str"], "return": "bool", "doc": "Check if all alpha"},
    "is_alnum": {"params": ["s: str"], "return": "bool", "doc": "Check if alphanumeric"},
    "is_space": {"params": ["s: str"], "return": "bool", "doc": "Check if whitespace"},
    
    # File I/O
    "file_read": {"params": ["path: str"], "return": "str", "doc": "Read file"},
    "file_write": {"params": ["path: str", "data: str"], "return": "bool", "doc": "Write file"},
    "file_append": {"params": ["path: str", "data: str"], "return": "bool", "doc": "Append to file"},
    "file_exists": {"params": ["path: str"], "return": "bool", "doc": "Check file exists"},
    "file_delete": {"params": ["path: str"], "return": "bool", "doc": "Delete file"},
    "file_size": {"params": ["path: str"], "return": "int", "doc": "Get file size"},
    "file_copy": {"params": ["src: str", "dst: str"], "return": "bool", "doc": "Copy file"},
    
    # System
    "clock_ms": {"params": [], "return": "int", "doc": "Current time in ms"},
    "sleep": {"params": ["ms: int"], "return": "void", "doc": "Sleep milliseconds"},
    "getenv": {"params": ["name: str"], "return": "str", "doc": "Get environment variable"},
    "exit": {"params": ["code: int"], "return": "void", "doc": "Exit program"},
}

KEYWORDS = ["def", "let", "if", "elif", "else", "while", "return", "and", "or", "not", "true", "false"]
TYPES = ["int", "str", "bool", "void"]

class BetterPythonParser:
    """Simple parser for extracting symbols and diagnostics"""
    
    def __init__(self, source: str):
        self.source = source
        self.lines = source.split('\n')
        self.functions = {}
        self.variables = {}
        self.diagnostics = []
        
    def parse(self):
        """Parse source and extract symbols"""
        for line_num, line in enumerate(self.lines):
            # Function definitions
            func_match = re.match(r'def\s+(\w+)\s*\((.*?)\)\s*->\s*(\w+)\s*:', line)
            if func_match:
                func_name = func_match.group(1)
                params = func_match.group(2)
                return_type = func_match.group(3)
                
                self.functions[func_name] = {
                    "line": line_num,
                    "params": params,
                    "return": return_type
                }
                
            # Variable declarations
            var_match = re.match(r'\s*let\s+(\w+)\s*:\s*(\w+)', line)
            if var_match:
                var_name = var_match.group(1)
                var_type = var_match.group(2)
                
                self.variables[var_name] = {
                    "line": line_num,
                    "type": var_type
                }
                
            # Basic syntax checks
            if ':' in line and 'def' in line and '->' not in line:
                self.diagnostics.append(Diagnostic(
                    range=Range(Position(line_num, 0), Position(line_num, len(line))),
                    severity=1,
                    message="Function definition missing return type annotation"
                ))
                
            # Undefined function calls (simple check)
            call_match = re.search(r'(\w+)\s*\(', line)
            if call_match:
                func_name = call_match.group(1)
                if func_name not in BUILTINS and func_name not in self.functions and func_name not in KEYWORDS:
                    if 'def' not in line:  # Not a definition
                        self.diagnostics.append(Diagnostic(
                            range=Range(Position(line_num, call_match.start()), Position(line_num, call_match.end())),
                            severity=2,
                            message=f"Function '{func_name}' may not be defined"
                        ))

class LanguageServer:
    """LSP server for BetterPython"""
    
    def __init__(self):
        self.documents = {}
        self.parsers = {}
        
    def handle_message(self, msg: Dict) -> Optional[Dict]:
        """Handle LSP message"""
        method = msg.get("method")
        params = msg.get("params", {})
        msg_id = msg.get("id")
        
        if method == "initialize":
            return self.initialize(msg_id)
        elif method == "textDocument/didOpen":
            self.did_open(params)
        elif method == "textDocument/didChange":
            self.did_change(params)
        elif method == "textDocument/completion":
            return self.completion(msg_id, params)
        elif method == "textDocument/hover":
            return self.hover(msg_id, params)
        elif method == "textDocument/definition":
            return self.definition(msg_id, params)
            
        return None
        
    def initialize(self, msg_id: int) -> Dict:
        """Initialize response"""
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "capabilities": {
                    "textDocumentSync": 1,
                    "completionProvider": {
                        "triggerCharacters": [".", "("]
                    },
                    "hoverProvider": True,
                    "definitionProvider": True,
                }
            }
        }
        
    def did_open(self, params: Dict):
        """Handle document open"""
        doc = params["textDocument"]
        uri = doc["uri"]
        text = doc["text"]
        
        self.documents[uri] = text
        self.parsers[uri] = BetterPythonParser(text)
        self.parsers[uri].parse()
        
        self.publish_diagnostics(uri)
        
    def did_change(self, params: Dict):
        """Handle document change"""
        doc = params["textDocument"]
        uri = doc["uri"]
        changes = params["contentChanges"]
        
        if changes:
            self.documents[uri] = changes[0]["text"]
            self.parsers[uri] = BetterPythonParser(changes[0]["text"])
            self.parsers[uri].parse()
            
            self.publish_diagnostics(uri)
            
    def completion(self, msg_id: int, params: Dict) -> Dict:
        """Provide completions"""
        items = []
        
        # Add built-in functions
        for name, info in BUILTINS.items():
            params_str = ", ".join(info["params"])
            items.append({
                "label": name,
                "kind": 3,  # Function
                "detail": f"{name}({params_str}) -> {info['return']}",
                "documentation": info["doc"]
            })
            
        # Add keywords
        for kw in KEYWORDS:
            items.append({
                "label": kw,
                "kind": 14,  # Keyword
                "detail": f"keyword: {kw}"
            })
            
        # Add types
        for typ in TYPES:
            items.append({
                "label": typ,
                "kind": 7,  # Class (type)
                "detail": f"type: {typ}"
            })
            
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": items
        }
        
    def hover(self, msg_id: int, params: Dict) -> Dict:
        """Provide hover information"""
        uri = params["textDocument"]["uri"]
        pos = params["position"]
        
        if uri not in self.documents:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
            
        line = self.documents[uri].split('\n')[pos["line"]]
        
        # Find word at position
        words = re.findall(r'\w+', line)
        for word in words:
            if word in BUILTINS:
                info = BUILTINS[word]
                params_str = ", ".join(info["params"])
                hover_text = f"```betterpython\n{word}({params_str}) -> {info['return']}\n```\n\n{info['doc']}"
                
                return {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "contents": {
                            "kind": "markdown",
                            "value": hover_text
                        }
                    }
                }
                
        return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
    def definition(self, msg_id: int, params: Dict) -> Dict:
        """Go to definition"""
        uri = params["textDocument"]["uri"]
        pos = params["position"]
        
        if uri not in self.parsers:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
            
        parser = self.parsers[uri]
        line = self.documents[uri].split('\n')[pos["line"]]
        
        # Find function call
        words = re.findall(r'\w+', line)
        for word in words:
            if word in parser.functions:
                func_info = parser.functions[word]
                return {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "uri": uri,
                        "range": {
                            "start": {"line": func_info["line"], "character": 0},
                            "end": {"line": func_info["line"], "character": 100}
                        }
                    }
                }
                
        return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
    def publish_diagnostics(self, uri: str):
        """Publish diagnostics"""
        if uri not in self.parsers:
            return
            
        diagnostics = []
        for diag in self.parsers[uri].diagnostics:
            diagnostics.append({
                "range": {
                    "start": {"line": diag.range.start.line, "character": diag.range.start.character},
                    "end": {"line": diag.range.end.line, "character": diag.range.end.character}
                },
                "severity": diag.severity,
                "message": diag.message,
                "source": diag.source
            })
            
        notification = {
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": uri,
                "diagnostics": diagnostics
            }
        }
        
        self.send_message(notification)
        
    def send_message(self, msg: Dict):
        """Send LSP message"""
        content = json.dumps(msg)
        response = f"Content-Length: {len(content)}\r\n\r\n{content}"
        sys.stdout.buffer.write(response.encode('utf-8'))
        sys.stdout.buffer.flush()
        
    def run(self):
        """Main server loop"""
        while True:
            # Read header
            headers = {}
            while True:
                line = sys.stdin.buffer.readline().decode('utf-8')
                if line == '\r\n':
                    break
                key, value = line.strip().split(': ', 1)
                headers[key] = value
                
            # Read content
            content_length = int(headers.get('Content-Length', 0))
            content = sys.stdin.buffer.read(content_length).decode('utf-8')
            
            if not content:
                break
                
            # Parse and handle message
            msg = json.loads(content)
            response = self.handle_message(msg)
            
            if response:
                self.send_message(response)

if __name__ == "__main__":
    server = LanguageServer()
    server.run()
