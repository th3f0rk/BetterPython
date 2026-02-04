/**
 * TreeSitter Grammar for BetterPython
 * Complete syntax tree definition with all language constructs
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

  rules: {
    // Top-level structure
    source_file: $ => repeat($._definition),

    _definition: $ => choice(
      $.function_definition,
      $._statement,
    ),

    // Function definition with explicit return type
    function_definition: $ => seq(
      'def',
      field('name', $.identifier),
      field('parameters', $.parameter_list),
      '->',
      field('return_type', $.type),
      ':',
      field('body', $.block),
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

    type: $ => choice(
      'int',
      'str',
      'bool',
      'void',
    ),

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
      $.if_statement,
      $.while_statement,
      $.return_statement,
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

    while_statement: $ => seq(
      'while',
      field('condition', $._expression),
      ':',
      field('body', $.block),
    ),

    return_statement: $ => seq(
      'return',
      optional(field('value', $._expression)),
    ),

    expression_statement: $ => $._expression,

    // Expressions with precedence
    _expression: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $.call_expression,
      $.parenthesized_expression,
      $.identifier,
      $.integer,
      $.string,
      $.boolean,
    ),

    binary_expression: $ => {
      const table = [
        ['or', 10],
        ['and', 11],
        [choice('==', '!='), 12],
        [choice('<', '<=', '>', '>='), 13],
        [choice('+', '-'), 14],
        [choice('*', '/', '%'), 15],
      ];

      return choice(...table.map(([operator, precedence]) =>
        prec.left(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    unary_expression: $ => prec(16, seq(
      field('operator', choice('not', '-')),
      field('operand', $._expression),
    )),

    call_expression: $ => prec(17, seq(
      field('function', $.identifier),
      field('arguments', $.argument_list),
    )),

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

    // Literals
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    integer: $ => /-?[0-9]+/,

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
        /[\\'"nrt]/,
        /[0-7]{1,3}/,
        /x[0-9a-fA-F]{2}/,
      ),
    )),

    boolean: $ => choice('true', 'false'),

    comment: $ => token(seq('#', /.*/)),
  },
});
