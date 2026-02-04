; Locals queries for scope analysis

; Function definitions create scope
(function_definition
  name: (identifier) @local.definition.function) @local.scope

; Parameters are local definitions
(parameter
  name: (identifier) @local.definition.parameter)

; Variable declarations are local definitions
(variable_declaration
  name: (identifier) @local.definition.variable)

; Identifiers are references
(identifier) @local.reference
