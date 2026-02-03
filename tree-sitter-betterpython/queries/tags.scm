; Tags queries for code navigation

; Function definitions
(function_definition
  name: (identifier) @name) @definition.function

; Variable declarations
(variable_declaration
  name: (identifier) @name) @definition.variable

; Function calls
(call_expression
  function: (identifier) @name) @reference.call
