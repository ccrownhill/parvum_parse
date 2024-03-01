#ifndef PARSER_H
#define PARSER_H

typedef enum type {
  NONE = 0,
  T_PLUS = 1,
  T_MINUS = 2,
  T_TIMES = 3,
  T_LBRACKET = 4,
  T_RBRACKET = 5,
  T_NUMBER = 6
} TokType;

// typedef enum type {
//   NONE = 0,
//   T_LBRACKET = 1,
//   T_RBRACKET = 2
// } TokType;

extern char *terminals[];

extern int yylex();

#endif
