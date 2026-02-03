#!/usr/bin/env python3
"""
BetterPython Language Server Protocol Implementation
Full-featured LSP with diagnostics, completion, hover, definition, references
"""

import json
import sys
import os
import re
import logging
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass, field
from pathlib import Path
from enum import IntEnum

# Configure logging
logging.basicConfig(
    filename='/tmp/betterpython_lsp.log',
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

class DiagnosticSeverity(IntEnum):
    ERROR = 1
    WARNING = 2
    INFORMATION = 3
    HINT = 4

class CompletionItemKind(IntEnum):
    TEXT = 1
    METHOD = 2
    FUNCTION = 3
    CONSTRUCTOR = 4
    FIELD = 5
    VARIABLE = 6
    CLASS = 7
    INTERFACE = 8
    MODULE = 9
    PROPERTY = 10
    UNIT = 11
    VALUE = 12
    ENUM = 13
    KEYWORD = 14
    SNIPPET = 15
    COLOR = 16
    FILE = 17
    REFERENCE = 18

@dataclass
class Position:
    line: int
    character: int
    
    def to_dict(self):
        return {"line": self.line, "character": self.character}

@dataclass
class Range:
    start: Position
    end: Position
    
    def to_dict(self):
        return {"start": self.start.to_dict(), "end": self.end.to_dict()}

@dataclass
class Diagnostic:
    range: Range
    severity: DiagnosticSeverity
    message: str
    source: str = "betterpython"
    code: Optional[str] = None
    
    def to_dict(self):
        result = {
            "range": self.range.to_dict(),
            "severity": int(self.severity),
            "message": self.message,
            "source": self.source,
        }
        if self.code:
            result["code"] = self.code
        return result

@dataclass
class Symbol:
    name: str
    kind: str  # function, variable, parameter
    line: int
    character: int
    type_annotation: Optional[str] = None
    return_type: Optional[str] = None
    parameters: List[Tuple[str, str]] = field(default_factory=list)
    doc: Optional[str] = None

# Complete built-in function database
BUILTINS = {
    # I/O
    "print": Symbol("print", "function", 0, 0, return_type="void", 
                   parameters=[("...", "any")],
                   doc="Print values to stdout separated by spaces"),
    "read_line": Symbol("read_line", "function", 0, 0, return_type="str",
                       doc="Read line from stdin, removing newline"),
    
    # String operations
    "len": Symbol("len", "function", 0, 0, return_type="int",
                 parameters=[("s", "str")],
                 doc="Get length of string"),
    "substr": Symbol("substr", "function", 0, 0, return_type="str",
                    parameters=[("s", "str"), ("start", "int"), ("length", "int")],
                    doc="Extract substring from string"),
    "str_upper": Symbol("str_upper", "function", 0, 0, return_type="str",
                       parameters=[("s", "str")],
                       doc="Convert string to uppercase"),
    "str_lower": Symbol("str_lower", "function", 0, 0, return_type="str",
                       parameters=[("s", "str")],
                       doc="Convert string to lowercase"),
    "str_trim": Symbol("str_trim", "function", 0, 0, return_type="str",
                      parameters=[("s", "str")],
                      doc="Remove leading and trailing whitespace"),
    "str_reverse": Symbol("str_reverse", "function", 0, 0, return_type="str",
                         parameters=[("s", "str")],
                         doc="Reverse string"),
    "str_repeat": Symbol("str_repeat", "function", 0, 0, return_type="str",
                        parameters=[("s", "str"), ("count", "int")],
                        doc="Repeat string count times"),
    "str_pad_left": Symbol("str_pad_left", "function", 0, 0, return_type="str",
                          parameters=[("s", "str"), ("width", "int"), ("pad", "str")],
                          doc="Pad string on left to width using pad character(s)"),
    "str_pad_right": Symbol("str_pad_right", "function", 0, 0, return_type="str",
                           parameters=[("s", "str"), ("width", "int"), ("pad", "str")],
                           doc="Pad string on right to width using pad character(s)"),
    "str_contains": Symbol("str_contains", "function", 0, 0, return_type="bool",
                          parameters=[("haystack", "str"), ("needle", "str")],
                          doc="Check if haystack contains needle"),
    "str_count": Symbol("str_count", "function", 0, 0, return_type="int",
                       parameters=[("haystack", "str"), ("needle", "str")],
                       doc="Count non-overlapping occurrences of needle"),
    "str_find": Symbol("str_find", "function", 0, 0, return_type="int",
                      parameters=[("haystack", "str"), ("needle", "str")],
                      doc="Find first occurrence, return index or -1"),
    "str_index_of": Symbol("str_index_of", "function", 0, 0, return_type="int",
                          parameters=[("haystack", "str"), ("needle", "str")],
                          doc="Alias for str_find"),
    "str_replace": Symbol("str_replace", "function", 0, 0, return_type="str",
                         parameters=[("s", "str"), ("old", "str"), ("new", "str")],
                         doc="Replace first occurrence of old with new"),
    "str_char_at": Symbol("str_char_at", "function", 0, 0, return_type="str",
                         parameters=[("s", "str"), ("index", "int")],
                         doc="Get character at index"),
    "starts_with": Symbol("starts_with", "function", 0, 0, return_type="bool",
                         parameters=[("s", "str"), ("prefix", "str")],
                         doc="Check if string starts with prefix"),
    "ends_with": Symbol("ends_with", "function", 0, 0, return_type="bool",
                       parameters=[("s", "str"), ("suffix", "str")],
                       doc="Check if string ends with suffix"),
    "to_str": Symbol("to_str", "function", 0, 0, return_type="str",
                    parameters=[("value", "any")],
                    doc="Convert value to string"),
    
    # Character operations
    "chr": Symbol("chr", "function", 0, 0, return_type="str",
                 parameters=[("code", "int")],
                 doc="Convert ASCII code to character (0-127)"),
    "ord": Symbol("ord", "function", 0, 0, return_type="int",
                 parameters=[("c", "str")],
                 doc="Convert first character to ASCII code"),
    
    # Math operations
    "abs": Symbol("abs", "function", 0, 0, return_type="int",
                 parameters=[("n", "int")],
                 doc="Absolute value"),
    "min": Symbol("min", "function", 0, 0, return_type="int",
                 parameters=[("a", "int"), ("b", "int")],
                 doc="Minimum of two values"),
    "max": Symbol("max", "function", 0, 0, return_type="int",
                 parameters=[("a", "int"), ("b", "int")],
                 doc="Maximum of two values"),
    "pow": Symbol("pow", "function", 0, 0, return_type="int",
                 parameters=[("base", "int"), ("exp", "int")],
                 doc="Raise base to power exp"),
    "sqrt": Symbol("sqrt", "function", 0, 0, return_type="int",
                  parameters=[("n", "int")],
                  doc="Integer square root"),
    "floor": Symbol("floor", "function", 0, 0, return_type="int",
                   parameters=[("n", "int")],
                   doc="Floor function (identity for integers)"),
    "ceil": Symbol("ceil", "function", 0, 0, return_type="int",
                  parameters=[("n", "int")],
                  doc="Ceiling function (identity for integers)"),
    "round": Symbol("round", "function", 0, 0, return_type="int",
                   parameters=[("n", "int")],
                   doc="Rounding function (identity for integers)"),
    "clamp": Symbol("clamp", "function", 0, 0, return_type="int",
                   parameters=[("value", "int"), ("min", "int"), ("max", "int")],
                   doc="Clamp value between min and max"),
    "sign": Symbol("sign", "function", 0, 0, return_type="int",
                  parameters=[("n", "int")],
                  doc="Return -1, 0, or 1 based on sign"),
    
    # Random
    "rand": Symbol("rand", "function", 0, 0, return_type="int",
                  doc="Generate random integer 0-32767"),
    "rand_range": Symbol("rand_range", "function", 0, 0, return_type="int",
                        parameters=[("min", "int"), ("max", "int")],
                        doc="Generate random integer in [min, max)"),
    "rand_seed": Symbol("rand_seed", "function", 0, 0, return_type="void",
                       parameters=[("seed", "int")],
                       doc="Seed random number generator"),
    
    # Security
    "hash_sha256": Symbol("hash_sha256", "function", 0, 0, return_type="str",
                         parameters=[("data", "str")],
                         doc="Compute SHA-256 hash (64 hex characters)"),
    "hash_md5": Symbol("hash_md5", "function", 0, 0, return_type="str",
                      parameters=[("data", "str")],
                      doc="Compute MD5 hash (32 hex characters) - insecure"),
    "secure_compare": Symbol("secure_compare", "function", 0, 0, return_type="bool",
                            parameters=[("a", "str"), ("b", "str")],
                            doc="Constant-time string comparison for security"),
    "random_bytes": Symbol("random_bytes", "function", 0, 0, return_type="str",
                          parameters=[("length", "int")],
                          doc="Generate cryptographically secure random bytes"),
    
    # Encoding
    "base64_encode": Symbol("base64_encode", "function", 0, 0, return_type="str",
                           parameters=[("data", "str")],
                           doc="Encode string to base64"),
    "base64_decode": Symbol("base64_decode", "function", 0, 0, return_type="str",
                           parameters=[("data", "str")],
                           doc="Decode base64 to string"),
    "int_to_hex": Symbol("int_to_hex", "function", 0, 0, return_type="str",
                        parameters=[("n", "int")],
                        doc="Convert integer to hexadecimal string"),
    "hex_to_int": Symbol("hex_to_int", "function", 0, 0, return_type="int",
                        parameters=[("hex", "str")],
                        doc="Convert hexadecimal string to integer"),
    
    # Validation
    "is_digit": Symbol("is_digit", "function", 0, 0, return_type="bool",
                      parameters=[("s", "str")],
                      doc="Check if all characters are digits"),
    "is_alpha": Symbol("is_alpha", "function", 0, 0, return_type="bool",
                      parameters=[("s", "str")],
                      doc="Check if all characters are alphabetic"),
    "is_alnum": Symbol("is_alnum", "function", 0, 0, return_type="bool",
                      parameters=[("s", "str")],
                      doc="Check if all characters are alphanumeric"),
    "is_space": Symbol("is_space", "function", 0, 0, return_type="bool",
                      parameters=[("s", "str")],
                      doc="Check if all characters are whitespace"),
    
    # File I/O
    "file_read": Symbol("file_read", "function", 0, 0, return_type="str",
                       parameters=[("path", "str")],
                       doc="Read entire file as string"),
    "file_write": Symbol("file_write", "function", 0, 0, return_type="bool",
                        parameters=[("path", "str"), ("data", "str")],
                        doc="Write string to file (overwrites)"),
    "file_append": Symbol("file_append", "function", 0, 0, return_type="bool",
                         parameters=[("path", "str"), ("data", "str")],
                         doc="Append string to file"),
    "file_exists": Symbol("file_exists", "function", 0, 0, return_type="bool",
                         parameters=[("path", "str")],
                         doc="Check if file exists"),
    "file_delete": Symbol("file_delete", "function", 0, 0, return_type="bool",
                         parameters=[("path", "str")],
                         doc="Delete file"),
    "file_size": Symbol("file_size", "function", 0, 0, return_type="int",
                       parameters=[("path", "str")],
                       doc="Get file size in bytes (-1 if not found)"),
    "file_copy": Symbol("file_copy", "function", 0, 0, return_type="bool",
                       parameters=[("src", "str"), ("dst", "str")],
                       doc="Copy file from src to dst"),
    
    # System
    "clock_ms": Symbol("clock_ms", "function", 0, 0, return_type="int",
                      doc="Get current time in milliseconds since epoch"),
    "sleep": Symbol("sleep", "function", 0, 0, return_type="void",
                   parameters=[("ms", "int")],
                   doc="Sleep for specified milliseconds"),
    "getenv": Symbol("getenv", "function", 0, 0, return_type="str",
                    parameters=[("name", "str")],
                    doc="Get environment variable (empty if not found)"),
    "exit": Symbol("exit", "function", 0, 0, return_type="void",
                  parameters=[("code", "int")],
                  doc="Exit program with status code"),
}

KEYWORDS = ["def", "let", "if", "elif", "else", "while", "return", "and", "or", "not", "true", "false"]
TYPES = ["int", "str", "bool", "void"]

class BetterPythonAnalyzer:
    """Analyze BetterPython source code"""
    
    def __init__(self, source: str, uri: str):
        self.source = source
        self.uri = uri
        self.lines = source.split('\n')
        self.functions = {}
        self.variables = {}
        self.diagnostics = []
        
    def analyze(self):
        """Perform full analysis"""
        self.parse_symbols()
        self.check_semantics()
        return self.diagnostics
        
    def parse_symbols(self):
        """Extract function and variable definitions"""
        for line_num, line in enumerate(self.lines):
            # Function definitions
            func_match = re.match(r'def\s+(\w+)\s*\((.*?)\)\s*->\s*(\w+)\s*:', line)
            if func_match:
                func_name = func_match.group(1)
                params_str = func_match.group(2)
                return_type = func_match.group(3)
                
                # Parse parameters
                params = []
                if params_str.strip():
                    for param in params_str.split(','):
                        param = param.strip()
                        if ':' in param:
                            name, typ = param.split(':', 1)
                            params.append((name.strip(), typ.strip()))
                
                self.functions[func_name] = Symbol(
                    name=func_name,
                    kind="function",
                    line=line_num,
                    character=line.index(func_name),
                    return_type=return_type,
                    parameters=params
                )
                
            # Variable declarations
            var_match = re.match(r'\s*let\s+(\w+)\s*:\s*(\w+)', line)
            if var_match:
                var_name = var_match.group(1)
                var_type = var_match.group(2)
                
                self.variables[var_name] = Symbol(
                    name=var_name,
                    kind="variable",
                    line=line_num,
                    character=line.index(var_name),
                    type_annotation=var_type
                )
                
    def check_semantics(self):
        """Perform semantic checks"""
        for line_num, line in enumerate(self.lines):
            stripped = line.strip()
            
            # Skip empty lines and comments
            if not stripped or stripped.startswith('#'):
                continue
                
            # Check function definition syntax
            if stripped.startswith('def '):
                if '->' not in stripped:
                    self.add_diagnostic(
                        line_num, 0, len(line),
                        DiagnosticSeverity.ERROR,
                        "Function definition missing return type annotation (-> type)",
                        "E001"
                    )
                elif ':' not in stripped:
                    self.add_diagnostic(
                        line_num, 0, len(line),
                        DiagnosticSeverity.ERROR,
                        "Function definition missing colon",
                        "E002"
                    )
                    
            # Check variable declaration syntax
            if 'let ' in stripped:
                var_match = re.search(r'let\s+\w+', stripped)
                if var_match and ':' not in stripped:
                    col = var_match.start()
                    self.add_diagnostic(
                        line_num, col, col + len(var_match.group()),
                        DiagnosticSeverity.ERROR,
                        "Variable declaration missing type annotation",
                        "E003"
                    )
                    
            # Check for undefined function calls
            call_matches = re.finditer(r'\b(\w+)\s*\(', stripped)
            for match in call_matches:
                func_name = match.group(1)
                if (func_name not in BUILTINS and 
                    func_name not in self.functions and 
                    func_name not in KEYWORDS):
                    col = match.start()
                    self.add_diagnostic(
                        line_num, col, col + len(func_name),
                        DiagnosticSeverity.WARNING,
                        f"Function '{func_name}' may not be defined",
                        "W001"
                    )
                    
            # Check for undefined variables
            var_matches = re.finditer(r'\b(\w+)\b', stripped)
            for match in var_matches:
                var_name = match.group(1)
                if (var_name not in self.variables and
                    var_name not in self.functions and
                    var_name not in BUILTINS and
                    var_name not in KEYWORDS and
                    var_name not in TYPES and
                    not var_name.isdigit()):
                    # Check if it's in a let statement
                    if not re.match(r'\s*let\s+' + var_name, stripped):
                        col = match.start()
                        self.add_diagnostic(
                            line_num, col, col + len(var_name),
                            DiagnosticSeverity.HINT,
                            f"Variable '{var_name}' may not be defined",
                            "H001"
                        )
                        
            # Check type annotations
            if return_type := self.has_invalid_type(stripped):
                self.add_diagnostic(
                    line_num, 0, len(line),
                    DiagnosticSeverity.ERROR,
                    f"Invalid type '{return_type}'. Must be int, str, bool, or void",
                    "E004"
                )
                
    def has_invalid_type(self, line: str) -> Optional[str]:
        """Check for invalid type annotations"""
        type_matches = re.finditer(r':\s*(\w+)', line)
        for match in type_matches:
            typ = match.group(1)
            if typ not in TYPES and typ not in KEYWORDS:
                return typ
        return None
        
    def add_diagnostic(self, line: int, start_char: int, end_char: int,
                      severity: DiagnosticSeverity, message: str, code: str):
        """Add diagnostic message"""
        diag = Diagnostic(
            range=Range(
                start=Position(line, start_char),
                end=Position(line, end_char)
            ),
            severity=severity,
            message=message,
            code=code
        )
        self.diagnostics.append(diag)

class LanguageServer:
    """LSP server implementation"""
    
    def __init__(self):
        self.documents = {}
        self.analyzers = {}
        logging.info("BetterPython LSP Server started")
        
    def handle_message(self, msg: Dict) -> Optional[Dict]:
        """Handle incoming LSP message"""
        method = msg.get("method")
        params = msg.get("params", {})
        msg_id = msg.get("id")
        
        logging.debug(f"Received: {method}")
        
        try:
            if method == "initialize":
                return self.initialize(msg_id, params)
            elif method == "initialized":
                logging.info("Client initialized")
                return None
            elif method == "textDocument/didOpen":
                self.did_open(params)
            elif method == "textDocument/didChange":
                self.did_change(params)
            elif method == "textDocument/didClose":
                self.did_close(params)
            elif method == "textDocument/completion":
                return self.completion(msg_id, params)
            elif method == "textDocument/hover":
                return self.hover(msg_id, params)
            elif method == "textDocument/definition":
                return self.definition(msg_id, params)
            elif method == "textDocument/references":
                return self.references(msg_id, params)
            elif method == "textDocument/documentSymbol":
                return self.document_symbol(msg_id, params)
            elif method == "textDocument/formatting":
                return self.formatting(msg_id, params)
            elif method == "shutdown":
                return {"jsonrpc": "2.0", "id": msg_id, "result": None}
            elif method == "exit":
                sys.exit(0)
                
        except Exception as e:
            logging.error(f"Error handling {method}: {e}", exc_info=True)
            if msg_id:
                return {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {
                        "code": -32603,
                        "message": str(e)
                    }
                }
        
        return None
        
    def initialize(self, msg_id: int, params: Dict) -> Dict:
        """Handle initialize request"""
        logging.info(f"Initialize from client: {params.get('clientInfo')}")
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "capabilities": {
                    "textDocumentSync": {
                        "openClose": True,
                        "change": 1,  # Full
                        "save": {"includeText": False}
                    },
                    "completionProvider": {
                        "triggerCharacters": [".", "(", ":"]
                    },
                    "hoverProvider": True,
                    "definitionProvider": True,
                    "referencesProvider": True,
                    "documentSymbolProvider": True,
                    "documentFormattingProvider": True,
                },
                "serverInfo": {
                    "name": "betterpython-lsp",
                    "version": "1.0.0"
                }
            }
        }
        
    def did_open(self, params: Dict):
        """Handle document open"""
        doc = params["textDocument"]
        uri = doc["uri"]
        text = doc["text"]
        
        self.documents[uri] = text
        self.analyzers[uri] = BetterPythonAnalyzer(text, uri)
        
        diagnostics = self.analyzers[uri].analyze()
        self.publish_diagnostics(uri, diagnostics)
        
    def did_change(self, params: Dict):
        """Handle document change"""
        doc = params["textDocument"]
        uri = doc["uri"]
        changes = params["contentChanges"]
        
        if changes:
            text = changes[0]["text"]
            self.documents[uri] = text
            self.analyzers[uri] = BetterPythonAnalyzer(text, uri)
            
            diagnostics = self.analyzers[uri].analyze()
            self.publish_diagnostics(uri, diagnostics)
            
    def did_close(self, params: Dict):
        """Handle document close"""
        uri = params["textDocument"]["uri"]
        if uri in self.documents:
            del self.documents[uri]
        if uri in self.analyzers:
            del self.analyzers[uri]
            
    def completion(self, msg_id: int, params: Dict) -> Dict:
        """Provide completion items"""
        uri = params["textDocument"]["uri"]
        pos = params["position"]
        
        items = []
        
        # Add built-in functions
        for name, symbol in BUILTINS.items():
            params_str = ", ".join(f"{p[0]}: {p[1]}" for p in symbol.parameters)
            items.append({
                "label": name,
                "kind": CompletionItemKind.FUNCTION,
                "detail": f"{name}({params_str}) -> {symbol.return_type}",
                "documentation": {
                    "kind": "markdown",
                    "value": symbol.doc or ""
                },
                "insertText": f"{name}($1)",
                "insertTextFormat": 2  # Snippet
            })
            
        # Add user-defined functions
        if uri in self.analyzers:
            for name, symbol in self.analyzers[uri].functions.items():
                params_str = ", ".join(f"{p[0]}: {p[1]}" for p in symbol.parameters)
                items.append({
                    "label": name,
                    "kind": CompletionItemKind.FUNCTION,
                    "detail": f"{name}({params_str}) -> {symbol.return_type}",
                    "insertText": f"{name}($1)",
                    "insertTextFormat": 2
                })
                
        # Add variables
        if uri in self.analyzers:
            for name, symbol in self.analyzers[uri].variables.items():
                items.append({
                    "label": name,
                    "kind": CompletionItemKind.VARIABLE,
                    "detail": f"{name}: {symbol.type_annotation}",
                })
                
        # Add keywords
        for kw in KEYWORDS:
            items.append({
                "label": kw,
                "kind": CompletionItemKind.KEYWORD,
                "detail": f"keyword",
            })
            
        # Add types
        for typ in TYPES:
            items.append({
                "label": typ,
                "kind": CompletionItemKind.CLASS,
                "detail": "type",
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
        word_match = re.search(r'\b\w+\b', line[pos["character"]:])
        if word_match:
            word = word_match.group()
            
            # Check built-ins
            if word in BUILTINS:
                symbol = BUILTINS[word]
                params_str = ", ".join(f"{p[0]}: {p[1]}" for p in symbol.parameters)
                hover_text = f"```betterpython\n{word}({params_str}) -> {symbol.return_type}\n```\n\n{symbol.doc}"
                
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
                
            # Check user functions
            if uri in self.analyzers:
                if word in self.analyzers[uri].functions:
                    symbol = self.analyzers[uri].functions[word]
                    params_str = ", ".join(f"{p[0]}: {p[1]}" for p in symbol.parameters)
                    hover_text = f"```betterpython\ndef {word}({params_str}) -> {symbol.return_type}\n```"
                    
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
                    
                # Check variables
                if word in self.analyzers[uri].variables:
                    symbol = self.analyzers[uri].variables[word]
                    hover_text = f"```betterpython\n{word}: {symbol.type_annotation}\n```"
                    
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
        
        if uri not in self.documents or uri not in self.analyzers:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
            
        line = self.documents[uri].split('\n')[pos["line"]]
        
        # Find word at position
        word_match = re.search(r'\b\w+\b', line[pos["character"]:])
        if word_match:
            word = word_match.group()
            
            # Check functions
            if word in self.analyzers[uri].functions:
                symbol = self.analyzers[uri].functions[word]
                return {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "uri": uri,
                        "range": {
                            "start": {"line": symbol.line, "character": symbol.character},
                            "end": {"line": symbol.line, "character": symbol.character + len(word)}
                        }
                    }
                }
                
            # Check variables
            if word in self.analyzers[uri].variables:
                symbol = self.analyzers[uri].variables[word]
                return {
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "uri": uri,
                        "range": {
                            "start": {"line": symbol.line, "character": symbol.character},
                            "end": {"line": symbol.line, "character": symbol.character + len(word)}
                        }
                    }
                }
                
        return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
    def references(self, msg_id: int, params: Dict) -> Dict:
        """Find references"""
        uri = params["textDocument"]["uri"]
        pos = params["position"]
        
        if uri not in self.documents:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
            
        lines = self.documents[uri].split('\n')
        line = lines[pos["line"]]
        
        # Find word at position
        word_match = re.search(r'\b\w+\b', line[pos["character"]:])
        if not word_match:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
            
        word = word_match.group()
        references = []
        
        # Find all occurrences
        for line_num, line_text in enumerate(lines):
            for match in re.finditer(r'\b' + re.escape(word) + r'\b', line_text):
                references.append({
                    "uri": uri,
                    "range": {
                        "start": {"line": line_num, "character": match.start()},
                        "end": {"line": line_num, "character": match.end()}
                    }
                })
                
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": references
        }
        
    def document_symbol(self, msg_id: int, params: Dict) -> Dict:
        """Provide document symbols"""
        uri = params["textDocument"]["uri"]
        
        if uri not in self.analyzers:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
            
        symbols = []
        analyzer = self.analyzers[uri]
        
        # Add functions
        for name, symbol in analyzer.functions.items():
            symbols.append({
                "name": name,
                "kind": 12,  # Function
                "range": {
                    "start": {"line": symbol.line, "character": 0},
                    "end": {"line": symbol.line, "character": 100}
                },
                "selectionRange": {
                    "start": {"line": symbol.line, "character": symbol.character},
                    "end": {"line": symbol.line, "character": symbol.character + len(name)}
                }
            })
            
        # Add variables
        for name, symbol in analyzer.variables.items():
            symbols.append({
                "name": name,
                "kind": 13,  # Variable
                "range": {
                    "start": {"line": symbol.line, "character": 0},
                    "end": {"line": symbol.line, "character": 100}
                },
                "selectionRange": {
                    "start": {"line": symbol.line, "character": symbol.character},
                    "end": {"line": symbol.line, "character": symbol.character + len(name)}
                }
            })
            
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": symbols
        }
        
    def formatting(self, msg_id: int, params: Dict) -> Dict:
        """Format document"""
        # Placeholder - would integrate with formatter
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": []
        }
        
    def publish_diagnostics(self, uri: str, diagnostics: List[Diagnostic]):
        """Publish diagnostics to client"""
        notification = {
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": uri,
                "diagnostics": [d.to_dict() for d in diagnostics]
            }
        }
        
        self.send_message(notification)
        
    def send_message(self, msg: Dict):
        """Send message to client"""
        content = json.dumps(msg)
        response = f"Content-Length: {len(content)}\r\n\r\n{content}"
        sys.stdout.buffer.write(response.encode('utf-8'))
        sys.stdout.buffer.flush()
        
    def run(self):
        """Main server loop"""
        logging.info("LSP server running")
        
        while True:
            try:
                # Read headers
                headers = {}
                while True:
                    line = sys.stdin.buffer.readline()
                    if not line:
                        logging.info("Client disconnected")
                        return
                        
                    line = line.decode('utf-8').strip()
                    if not line:
                        break
                        
                    if ': ' in line:
                        key, value = line.split(': ', 1)
                        headers[key] = value
                    
                # Read content
                content_length = int(headers.get('Content-Length', 0))
                if content_length == 0:
                    continue
                    
                content = sys.stdin.buffer.read(content_length).decode('utf-8')
                
                # Parse and handle message
                msg = json.loads(content)
                response = self.handle_message(msg)
                
                if response:
                    self.send_message(response)
                    
            except KeyboardInterrupt:
                logging.info("Server interrupted")
                break
            except Exception as e:
                logging.error(f"Error in main loop: {e}", exc_info=True)

if __name__ == "__main__":
    server = LanguageServer()
    server.run()
