%token T_PLUS T_MINUS T_TIMES
%token T_LBRACKET T_RBRACKET
%token T_NUMBER

%start expr

%%

expr: expr T_PLUS term { $$ = (void *)((long)$0 + (long)$2); }
    | expr T_MINUS term { $$ = (void *)((long)$0 - (long)$2); }
    | term { $$ = $0; }

term: term T_TIMES factor { $$ = (void *)((long)$0 * (long)$2); }
    | factor { $$ = $0; }

factor: T_LBRACKET expr T_RBRACKET { $$ = $1; }
      | T_NUMBER { $$ = $0; }

%%

