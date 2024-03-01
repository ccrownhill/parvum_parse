#include <stdio.h>
#include <stdlib.h> // for exit
#include <string.h>

#include "parser.h"
#include "parse_types.h"
#include "util_types.h"

#define FNAME_LEN 50
#define LINE_LEN 300


char parse_stack_decl[] =
"typedef struct _ParseStack {\n"
"	void *val;\n"
"	int state_no;\n"
"	struct _ParseStack *next;\n"
"} ParseStack;\n"
"\n"
"\n"
"\n"
"static ParseStack *ParseStack_push(ParseStack *stack, void *val, int state_no);\n"
"static ParseStack *ParseStack_pop(ParseStack *stack, void **out);\n"
"static void ParseStack_free(ParseStack *stack);\n"
"\n"
"\n"
"static ParseStack *ParseStack_push(ParseStack *stack, void *val, int state_no)\n"
"{\n"
"  ParseStack *new = malloc(sizeof(ParseStack));\n"
"  new->val = val;\n"
"  new->state_no = state_no;\n"
"  new->next = stack;\n"
"  return new;\n"
"}\n"
"\n"
"static ParseStack *ParseStack_pop(ParseStack *stack, void **out)\n"
"{\n"
"  if (stack == NULL)\n"
"    return NULL;\n"
"  ParseStack *new = stack->next;\n"
"  *out = stack->val;\n"
"  free(stack);\n"
"  return new;\n"
"}\n"
"\n"
"static void ParseStack_free(ParseStack *stack)\n"
"{\n"
"  void *dummy;\n"
"  while ((stack = ParseStack_pop(stack, &dummy)) != NULL);\n"
"}\n"
"\n";


char pparse_fn_before_switch[] =
"void *pparse()\n"
"{\n"
"  ParseStack *pstack = ParseStack_push(NULL, (void *)0, 0);\n"
"  int tt = yylex();\n"
"  int act;\n"
"  void *ppval;\n"
"  void **stack_vals;\n"
"  int lhs_num;\n"
"\n"
"  while (1) {\n"
"    act = action_t[pstack->state_no][tt];\n"
"    if (act == INT_MIN) {\n"
"      fprintf(stderr, \"parsing error\\n\");\n"
"      exit(1);\n"
"    } else if (act < 0) {\n"
"      int rule_no = (-act) - 1;\n"
"      stack_vals = calloc(rule_num_elts[rule_no], sizeof(void *));\n"
"      for (int i = 0; i < rule_num_elts[rule_no]; i++) {\n"
"        pstack = ParseStack_pop(pstack, stack_vals + (rule_num_elts[rule_no] - i - 1));\n"
"      }\n"
"\n";

char pparse_fn_after_switch[] =
"      free(stack_vals);\n"
"      lhs_num = -1;\n"
"      for (int i = 0; i < num_non_terminals; i++) {\n"
"        if (rule_no <= lhs_first_rules[i]) {\n"
"          lhs_num = i;\n"
"          break;\n"
"        }\n"
"      }\n"
"\n"
"      if (lhs_num == -1) {\n"
"        fprintf(stderr, \"could not find lhs symbol number in lhs_first_rules\\n\");\n"
"        exit(1);\n"
"      }\n"
"\n"
"      // find number of rule using lhs_from_rule_map\n"
"      pstack = ParseStack_push(pstack, ppval, goto_t[pstack->state_no][lhs_num]);\n"
"    } else if (act == INT_MAX && tt == 0) {\n"
"      break;\n"
"    } else if (act >= 0) {\n"
"      pstack = ParseStack_push(pstack, ppval_in, act);\n"
"      tt = yylex();\n"
"    }\n"
"  }\n"
"  ParseStack_free(pstack);\n"
"  return ppval;\n"
"}\n";

char includes[] =
"#include <stdio.h>\n"
"#include <limits.h>\n"
"#include <stdlib.h>\n";


int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: parser grammar_file [parse_file]\n");
    exit(1);
  }

  FILE *grammar_f;
  if (!(grammar_f = fopen(argv[1], "r"))) {
    error("can't open file '%s'", argv[1]);
  }

  char outc_file_name[FNAME_LEN], outh_file_name[FNAME_LEN];
  strncpy(outc_file_name, argv[1], FNAME_LEN);
  strncat(outc_file_name, "g.c", FNAME_LEN);
  FILE *outc;
  if (!(outc = fopen(outc_file_name, "w"))) {
    error("can't open file '%s' for writing", outc_file_name);
  }
  FILE *outh;
  strncpy(outh_file_name, argv[1], FNAME_LEN);
  strncat(outh_file_name, "g.h", FNAME_LEN);
  if (!(outh = fopen(outh_file_name, "w"))) {
    error("can't open file '%s' for writing", outh_file_name);
  }
  char *tmp = outh_file_name;
  char *outh_basename = outh_file_name;
  while ((tmp = strstr(tmp, "/")) != NULL) {
    tmp++;
    outh_basename = tmp;
  }

  Declarations *decl = Declarations_parse(grammar_f);

  // fill header file
  char header_def[FNAME_LEN];
  header_upper_case_underscored(outh_basename, header_def);
  fprintf(outh, "#ifndef %s\n", header_def);
  fprintf(outh, "#define %s\n", header_def);

  print_tok_enum_from_tok_map(outh, decl->tok_map);
  fprintf(outh, "int yylex(void);\n");
  fprintf(outh, "void yyerror(const char *);\n");
  fprintf(outh, "extern void *ppval_in;\n");
  fprintf(outh, "#endif\n");

  // fill source file
  fprintf(outc, "%s\n", includes);
  fprintf(outc, "#include \"%s\"\n", outh_basename);
  fprintf(outc, "%s\n", parse_stack_decl);


  HashMap *gmap = gmap_generate(grammar_f, decl->root);
  strncpy(decl->root, "%root", TOK_LEN);

  HashMap *fmap = fmap_generate(gmap, decl->root, decl->tok_map);

  CC *cc = CC_construct(gmap, fmap, decl->root);

  RuleMaps rmaps = RuleMaps_construct(gmap);

  PTable ptable = PTable_construct(decl->root, cc, gmap,
      decl->tok_map, rmaps);

  print_1d_arr_def(outc, rmaps.lhs_first_rules, rmaps.lhs_map->size, "lhs_first_rules");
  print_1d_arr_def(outc, rmaps.rule_num_elts, rmaps.total_map->size, "rule_num_elts");
  PTable_print(outc, ptable);

  fprintf(outc, "void *ppval_in;\n");
  fprintf(outc, "int num_terminals = %d;\n", ptable.num_terminals);
  fprintf(outc, "int num_non_terminals = %d;\n", ptable.num_non_terminals);

  fprintf(outc, "%s\n", pparse_fn_before_switch);
  print_red_op_switch(outc, gmap);

  fprintf(outc, "%s\n", pparse_fn_after_switch);

  CC_deconstruct(cc);
  PTable_free(ptable);
  fmap_free(fmap);
  gmap_free(gmap);
  Declarations_free(decl);
  RuleMaps_free(rmaps);

  // dump C code at end of grammar file
  char buf[LINE_LEN];
  while (fgets(buf, LINE_LEN, grammar_f) != NULL) {
    fprintf(outc, "%s", buf);
  }

  fclose(grammar_f);
  fclose(outc);
  fclose(outh);
}
