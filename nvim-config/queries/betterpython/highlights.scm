; Highlights for Neovim TreeSitter

[
  "def"
  "let"
  "if"
  "elif"
  "else"
  "while"
  "return"
] @keyword

[
  "and"
  "or"
  "not"
] @keyword.operator

(type) @type.builtin
(boolean) @boolean

(function_definition
  name: (identifier) @function)

(call_expression
  function: (identifier) @function.call)

(parameter
  name: (identifier) @parameter)

(variable_declaration
  name: (identifier) @variable)

(identifier) @variable

(integer) @number
(string) @string
(escape_sequence) @string.escape

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

"->" @punctuation.special

[
  "("
  ")"
  ":"
  ","
] @punctuation.delimiter

(comment) @comment
(ERROR) @error
