/**
 * TreeSitter Grammar for BetterPython v1.1
 * Complete syntax tree definition with all language constructs including:
 * - Functions with type annotations
 * - Classes with inheritance
 * - Extern (FFI) declarations
 * - Structs and enums
 * - Exception handling
 * - Control flow (for, while, if, break, continue)
 * - Complex types (arrays, maps, tuples, pointers)
 */

module.exports = grammar({
  name: 'betterpython',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  externals: $ => [
    $._indent,
    $._dedent,
    $._newline,
  ],

  word: $ => $.identifier,

  conflicts: $ => [
    [$.primary_expression, $.parameter],
    [$.type, $.primary_expression],
  ],

  rules: {
    // Top-level structure
    source_file: $ => repeat($._definition),

    _definition: $ => choice(
      $.function_definition,
      $.class_definition,
      $.struct_definition,
      $.enum_definition,
      $.extern_declaration,
      $.import_statement,
      $.export_statement,
      $._statement,
    ),

    // Import/Export statements
    import_statement: $ => seq(
      'import',
      field('module', $.identifier),
      optional(seq(
        '(',
        $.identifier,
        repeat(seq(',', $.identifier)),
        optional(','),
        ')'
      )),
      optional(seq('as', field('alias', $.identifier))),
    ),

    export_statement: $ => seq(
      'export',
      choice(
        $.function_definition,
        $.class_definition,
        $.struct_definition,
      ),
    ),

    // Function definition with explicit return type
    function_definition: $ => seq(
      'def',
      field('name', $.identifier),
      optional($.generic_parameters),
      field('parameters', $.parameter_list),
      '->',
      field('return_type', $.type),
      ':',
      field('body', $.block),
    ),

    // Generic parameters for generic functions
    generic_parameters: $ => seq(
      '<',
      $.identifier,
      repeat(seq(',', $.identifier)),
      '>',
    ),

    parameter_list: $ => seq(
      '(',
      optional(seq(
        $.parameter,
        repeat(seq(',', $.parameter)),
        optional(','),
      )),
      ')',
    ),

    parameter: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    // Class definition with optional inheritance
    class_definition: $ => seq(
      'class',
      field('name', $.identifier),
      optional(seq('(', field('parent', $.identifier), ')')),
      ':',
      field('body', $.class_body),
    ),

    class_body: $ => seq(
      $._indent,
      repeat1(choice(
        $.field_declaration,
        $.method_definition,
      )),
      $._dedent,
    ),

    field_declaration: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    method_definition: $ => seq(
      'def',
      field('name', $.identifier),
      field('parameters', $.parameter_list),
      optional(seq('->', field('return_type', $.type))),
      ':',
      field('body', $.block),
    ),

    // Struct definition
    struct_definition: $ => seq(
      'struct',
      field('name', $.identifier),
      ':',
      field('body', $.struct_body),
    ),

    struct_body: $ => seq(
      $._indent,
      repeat1($.field_declaration),
      $._dedent,
    ),

    // Enum definition
    enum_definition: $ => seq(
      'enum',
      field('name', $.identifier),
      ':',
      field('body', $.enum_body),
    ),

    enum_body: $ => seq(
      $._indent,
      repeat1($.enum_member),
      $._dedent,
    ),

    enum_member: $ => seq(
      field('name', $.identifier),
      optional(seq('=', field('value', $.integer))),
    ),

    // Extern (FFI) declaration
    extern_declaration: $ => seq(
      'extern',
      'fn',
      field('name', $.identifier),
      field('parameters', $.parameter_list),
      '->',
      field('return_type', $.type),
      'from',
      field('library', $.string),
      optional(seq('as', field('c_name', $.string))),
    ),

    // Types with full support for complex types
    type: $ => choice(
      $.primitive_type,
      $.array_type,
      $.map_type,
      $.tuple_type,
      $.pointer_type,
      $.identifier,  // User-defined types (structs, classes, enums)
    ),

    primitive_type: $ => choice(
      'int',
      'float',
      'str',
      'bool',
      'void',
      'i8',
      'i16',
      'i32',
      'i64',
      'u8',
      'u16',
      'u32',
      'u64',
    ),

    array_type: $ => seq('[', $.type, ']'),

    map_type: $ => seq('{', $.type, ':', $.type, '}'),

    tuple_type: $ => seq(
      '(',
      $.type,
      repeat1(seq(',', $.type)),
      optional(','),
      ')',
    ),

    pointer_type: $ => seq('ptr', optional(seq('<', $.type, '>'))),

    // Block with indentation
    block: $ => seq(
      $._indent,
      repeat1($._statement),
      $._dedent,
    ),

    // Statements
    _statement: $ => choice(
      $.variable_declaration,
      $.assignment_statement,
      $.index_assignment,
      $.field_assignment,
      $.if_statement,
      $.for_statement,
      $.while_statement,
      $.break_statement,
      $.continue_statement,
      $.return_statement,
      $.try_statement,
      $.throw_statement,
      $.expression_statement,
    ),

    variable_declaration: $ => seq(
      'let',
      field('name', $.identifier),
      ':',
      field('type', $.type),
      '=',
      field('value', $._expression),
    ),

    assignment_statement: $ => seq(
      field('left', $.identifier),
      '=',
      field('right', $._expression),
    ),

    index_assignment: $ => seq(
      field('container', $._expression),
      '[',
      field('index', $._expression),
      ']',
      '=',
      field('value', $._expression),
    ),

    field_assignment: $ => seq(
      field('object', $._expression),
      '.',
      field('field', $.identifier),
      '=',
      field('value', $._expression),
    ),

    if_statement: $ => seq(
      'if',
      field('condition', $._expression),
      ':',
      field('consequence', $.block),
      repeat($.elif_clause),
      optional($.else_clause),
    ),

    elif_clause: $ => seq(
      'elif',
      field('condition', $._expression),
      ':',
      field('consequence', $.block),
    ),

    else_clause: $ => seq(
      'else',
      ':',
      field('alternative', $.block),
    ),

    for_statement: $ => seq(
      'for',
      field('iterator', $.identifier),
      'in',
      choice(
        seq('range', '(', field('start', $._expression), ',', field('end', $._expression), ')'),
        field('iterable', $._expression),
      ),
      ':',
      field('body', $.block),
    ),

    while_statement: $ => seq(
      'while',
      field('condition', $._expression),
      ':',
      field('body', $.block),
    ),

    break_statement: $ => 'break',

    continue_statement: $ => 'continue',

    return_statement: $ => seq(
      'return',
      optional(field('value', $._expression)),
    ),

    try_statement: $ => seq(
      'try',
      ':',
      field('try_body', $.block),
      'catch',
      optional(field('exception_var', $.identifier)),
      ':',
      field('catch_body', $.block),
      optional(seq(
        'finally',
        ':',
        field('finally_body', $.block),
      )),
    ),

    throw_statement: $ => seq(
      'throw',
      field('value', $._expression),
    ),

    expression_statement: $ => $._expression,

    // Expressions with precedence
    _expression: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $.call_expression,
      $.method_call,
      $.index_expression,
      $.field_access,
      $.new_expression,
      $.super_expression,
      $.lambda_expression,
      $.parenthesized_expression,
      $.array_literal,
      $.map_literal,
      $.struct_literal,
      $.tuple_expression,
      $.fstring,
      $.primary_expression,
    ),

    primary_expression: $ => choice(
      $.identifier,
      $.integer,
      $.float,
      $.string,
      $.boolean,
      'self',
    ),

    binary_expression: $ => {
      const table = [
        ['or', 10],
        ['and', 11],
        [choice('==', '!='), 12],
        [choice('<', '<=', '>', '>='), 13],
        [choice('+', '-'), 14],
        [choice('*', '/', '%'), 15],
        [choice('&', '|', '^'), 16],
        [choice('<<', '>>'), 17],
      ];

      return choice(...table.map(([operator, precedence]) =>
        prec.left(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    unary_expression: $ => prec(18, seq(
      field('operator', choice('not', '-', '~')),
      field('operand', $._expression),
    )),

    call_expression: $ => prec(19, seq(
      field('function', $.identifier),
      optional($.type_arguments),
      field('arguments', $.argument_list),
    )),

    type_arguments: $ => seq(
      '<',
      $.type,
      repeat(seq(',', $.type)),
      '>',
    ),

    method_call: $ => prec(19, seq(
      field('object', $._expression),
      '.',
      field('method', $.identifier),
      field('arguments', $.argument_list),
    )),

    index_expression: $ => prec(19, seq(
      field('container', $._expression),
      '[',
      field('index', $._expression),
      ']',
    )),

    field_access: $ => prec(19, seq(
      field('object', $._expression),
      '.',
      field('field', $.identifier),
    )),

    new_expression: $ => prec(20, seq(
      'new',
      field('class', $.identifier),
      field('arguments', $.argument_list),
    )),

    super_expression: $ => choice(
      seq('super', '(', ')'),
      seq('super', '.', field('method', $.identifier), $.argument_list),
    ),

    lambda_expression: $ => seq(
      'lambda',
      $.parameter_list,
      '->',
      $.type,
      ':',
      $._expression,
    ),

    argument_list: $ => seq(
      '(',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ')',
    ),

    parenthesized_expression: $ => seq(
      '(',
      $._expression,
      ')',
    ),

    array_literal: $ => seq(
      '[',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ']',
    ),

    map_literal: $ => seq(
      '{',
      optional(seq(
        $.map_entry,
        repeat(seq(',', $.map_entry)),
        optional(','),
      )),
      '}',
    ),

    map_entry: $ => seq(
      field('key', $._expression),
      ':',
      field('value', $._expression),
    ),

    struct_literal: $ => seq(
      field('type', $.identifier),
      '{',
      optional(seq(
        $.struct_field_init,
        repeat(seq(',', $.struct_field_init)),
        optional(','),
      )),
      '}',
    ),

    struct_field_init: $ => seq(
      field('name', $.identifier),
      ':',
      field('value', $._expression),
    ),

    tuple_expression: $ => seq(
      '(',
      $._expression,
      ',',
      $._expression,
      repeat(seq(',', $._expression)),
      optional(','),
      ')',
    ),

    fstring: $ => seq(
      'f"',
      repeat(choice(
        $.fstring_content,
        $.fstring_interpolation,
        $.escape_sequence,
      )),
      '"',
    ),

    fstring_content: $ => token.immediate(prec(1, /[^"\\{]+/)),

    fstring_interpolation: $ => seq(
      '{',
      $._expression,
      '}',
    ),

    // Literals
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    integer: $ => choice(
      /[0-9]+/,
      /0x[0-9a-fA-F]+/,
      /0b[01]+/,
      /0o[0-7]+/,
    ),

    float: $ => /[0-9]+\.[0-9]*([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+|\.[0-9]+([eE][+-]?[0-9]+)?/,

    string: $ => seq(
      '"',
      repeat(choice(
        $.string_content,
        $.escape_sequence,
      )),
      '"',
    ),

    string_content: $ => token.immediate(prec(1, /[^"\\]+/)),

    escape_sequence: $ => token.immediate(seq(
      '\\',
      choice(
        /[\\'"nrt0]/,
        /[0-7]{1,3}/,
        /x[0-9a-fA-F]{2}/,
        /u[0-9a-fA-F]{4}/,
      ),
    )),

    boolean: $ => choice('true', 'false'),

    comment: $ => token(seq('#', /.*/)),
  },
});
