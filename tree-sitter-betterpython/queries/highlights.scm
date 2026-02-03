; Syntax highlighting queries for BetterPython

; Keywords
[
  "def"
  "let"
  "if"
  "elif"
  "else"
  "while"
  "return"
] @keyword

; Boolean operators and literals
[
  "and"
  "or"
  "not"
] @keyword.operator

(boolean) @constant.builtin.boolean

; Types
(type) @type.builtin

; Function definitions
(function_definition
  name: (identifier) @function)

; Function calls
(call_expression
  function: (identifier) @function.call)

; Parameters
(parameter
  name: (identifier) @variable.parameter
  type: (type) @type.builtin)

; Variable declarations
(variable_declaration
  name: (identifier) @variable
  type: (type) @type.builtin)

; Literals
(integer) @number
(string) @string
(escape_sequence) @string.escape

; Operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "!="
  "<"
  "<="
  ">"
  ">="
  "="
] @operator

; Special operators
"->" @punctuation.special

; Delimiters
[
  "("
  ")"
  ":"
  ","
] @punctuation.delimiter

; Comments
(comment) @comment

; Error nodes for invalid syntax
(ERROR) @error
