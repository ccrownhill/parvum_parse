%option noyywrap

%{
#include "grammar_math.gg.h"
%}


%%
[+] { ppval_in = NULL; return T_PLUS; }

[-] { ppval_in = NULL; return T_MINUS; }

[*] { ppval_in = NULL; return T_TIMES; }

[(] { ppval_in = NULL; return T_LBRACKET; }

[)] { ppval_in = NULL; return T_RBRACKET; }

[0-9]+([.][0-9]*)? { ppval_in = (void *)atoi(yytext); return T_NUMBER; }


.      /* ignore any other characters */

%%

/* Error handler. This will get called if none of the rules match. */
void yyerror (char const *s)
{
  fprintf (stderr, "Flex Error: %s\n", s); /* s is the text that wasn't matched */
  exit(1);
}
