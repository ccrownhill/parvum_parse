#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "input.h"
#include "parse_types.h"
#include "util_types.h"

static void gmap_free_val(const char *_1, void *val, void *_2);
static void gmap_print_el(const char *key, void *val, void *_);
static void conn_print(const char *key, void *val, void *_);

static void set_lhs_first_rule_map(const char *key, void *val, void *maps);
static void *set_own_index(const char *key, void *val, void *prev_out);
static void insert_rule_list(const char *key, void *val, void *maps);

static void print_arr_literal(FILE *out, int *arr, int size);
static void print_enum_entry(const char *key, void *val, void *arg);
static void print_red_op_switch_el(const char *key, void *val, void *outf);

void rule_map_key_gen(char *rkey, ProdRule *rule)
{
  size_t pos = snprintf(rkey, RULE_LEN, "%s ->", rule->sym);
  for (int i = 0; i < rule->num_symbols; i++) {
    pos += snprintf(rkey + pos, TOK_LEN, " %s", rule->sym_l[i]);
  }
}

char *terminals[] = {"NONE", "T_PLUS", "T_MINUS", "T_TIMES", "T_LBRACKET", "T_RBRACKET", "T_NUMBER"};
//char *terminals[] = {"NONE", "T_LBRACKET", "T_RBRACKET"};

int terminal_check_el(const char *key, void *val, void *check)
{
  char *ch = (char *)check;
  return (strcmp(key, ch) == 0);
}

int is_terminal(const char *name, HashMap *tok_map)
{
  return HashMap_any(tok_map, terminal_check_el, (void *)name);
}

/******************************************************************************/
/* Parse table                                                                */
/******************************************************************************/

void key_gen(char *buf, int state_no, const char *token_type)
{
  snprintf(buf, FORMAT_BUFLEN, "%d %s", state_no, token_type);
}

void print_tok_map_el(const char *key, void *val, void *arg)
{
  fprintf(stderr, "%s -> %d\n", key, *((int *)val));
}

PTable PTable_construct(const char *root, CC *cc,
    HashMap *gmap, HashMap *tok_map, RuleMaps rmaps)
{
  LR1El *set_iter;
  int act;
  PTable out;
  GotoParams gparams;
  char buf[FORMAT_BUFLEN];
  int rule_num, terminal_num;
  char rule_key[RULE_LEN];

  out.num_rows = cc->state_no + 1;
  out.num_terminals = tok_map->size;
  out.num_non_terminals = gmap->size;

  out.action_t = calloc(out.num_rows, sizeof(int *));
  out.goto_t = calloc(out.num_rows, sizeof(int *));
  // fill all fields with INT_MIN by default to signify an invalid operation
  // if INT_MIN is encountered in a field
  for (int i = 0; i < out.num_rows; i++) {
    out.action_t[i] = calloc(out.num_terminals, sizeof(int));
    for (int j = 0; j < out.num_terminals; j++) {
      out.action_t[i][j] = INT_MIN;
    }

    out.goto_t[i] = calloc(out.num_non_terminals, sizeof(int));
    for (int j = 0; j < out.num_non_terminals; j++) {
      out.goto_t[i][j] = INT_MIN;
    }
  }

  for (; cc != NULL; cc = cc->next) {
    for (set_iter = cc->cc_set; set_iter != NULL; set_iter = set_iter->next) {

      if (strcmp(set_iter->rule->sym, root) == 0 &&
          set_iter->pos == set_iter->rule->num_symbols &&
          set_iter->lookahead == 0) { // accept
        out.action_t[cc->state_no][0] = INT_MAX;

      } else if (set_iter->pos == set_iter->rule->num_symbols) {

        rule_map_key_gen(rule_key, set_iter->rule);

        rule_num = si_map_get(rmaps.total_map, rule_key);
        // add 1 to rule num so that it can never be 0 and hence reductions
        // can be distinguished from shifts which have entries >= 0
        out.action_t[cc->state_no][set_iter->lookahead] = -(rule_num + 1);

      } else if (is_terminal(set_iter->rule->sym_l[set_iter->pos], tok_map)) {
        CC **cc_next =
          (CC **)HashMap_get(
            cc->goto_map,
            set_iter->rule->sym_l[set_iter->pos],
            NULL
          );

        
        terminal_num = si_map_get(tok_map, set_iter->rule->sym_l[set_iter->pos]);
        out.action_t[cc->state_no][terminal_num] = (*cc_next)->state_no;
      }
    }

    gparams.cc = cc;
    gparams.tok_map = tok_map;
    gparams.goto_t = out.goto_t;
    gparams.lhs_map = rmaps.lhs_map;
    HashMap_iter(gmap, add_to_goto, (void *)&gparams);
  }

  return out;
}


void add_to_goto(const char *key, void *val, void *params)
{
  GotoParams *p = (GotoParams *)params;
  CC **next_cc;
  int non_terminal_num;

  if (!(is_terminal(key, p->tok_map))) {
    if ((next_cc = (CC **)HashMap_get(p->cc->goto_map, key, NULL)) == NULL) {
      return;
    }

    non_terminal_num = si_map_get(p->lhs_map, key);
    p->goto_t[p->cc->state_no][non_terminal_num] = (*next_cc)->state_no;
  }
}

void *state_list_reduce(const char *key, void *_, void *list)
{
  char *state_loc = strstr(key, " ");
  *state_loc = '\0';
  int state_no = atoi(key);
  *state_loc = ' ';
  if (List_contains(list, (void *)state_no, NULL))
    return list;
  else
    return List_insert(list, (void *)state_no);
}

void *non_terminals_list_reduce(const char *key, void *_, void *list)
{
  char *nt_loc = strstr(key, " ") + 1;
  if (List_contains(list, (void *)nt_loc, str_equal))
    return list;
  else
    return List_insert(list, (void *)strdup(nt_loc));
}

static void print_arr_literal(FILE *out, int *arr, int size)
{
  fprintf(out, "{");
  for (int i = 0; i < size; i++) {
    fprintf(out, "%d", arr[i]);
    if (i < size - 1) {
      fprintf(out, ",");
    }
  }
  fprintf(out, "}");
}

void print_1d_arr_def(FILE *out, int *arr, int size, const char *name)
{
  fprintf(out, "int %s[%d] = \n", name, size);
  print_arr_literal(out, arr, size);
  fprintf(out, ";\n");
}

void print_2d_arr_def(FILE *out, int **arr, int rows, int cols, const char *name)
{
  fprintf(out, "int %s[%d][%d] = {\n", name, rows, cols);
  for (int i = 0; i < rows; i++) {
    print_arr_literal(out, arr[i], cols);
    if (i < rows - 1) {
      fprintf(out, ",");
    }
    fprintf(out, "\n");
  }
  fprintf(out, "};\n");

}

void PTable_print(FILE *out, PTable table)
{
  print_2d_arr_def(out, table.action_t, table.num_rows, table.num_terminals, "action_t");
  print_2d_arr_def(out, table.goto_t, table.num_rows, table.num_non_terminals, "goto_t");
}

void PTable_free(PTable table)
{
  for (int i = 0; i < table.num_rows; i++) {
    free(table.action_t[i]);
    free(table.goto_t[i]);
  }
  free(table.action_t);
  free(table.goto_t);
}


/******************************************************************************/
/* Conversion utility types                                                   */
/* types that are used to convert string elements in tables to integers for   */
/* generating an integer only parse table that is easy to put into the output */
/* C file                                                                     */
/******************************************************************************/

Declarations *Declarations_parse(FILE *in)
{
  char token[TOK_LEN];
  Declarations *out = malloc(sizeof(Declarations));
  out->tok_map = si_map_init();

  int tok_mode = 0;
  int count = 0; // start counting from one so we don't interfere with 0 for
                  // reduce actions
  int root_defined = 0;
  si_map_set(out->tok_map, "NONE", count++);
  while ((get_token(in, token) != EOF) && (strcmp(token, "%%") != 0)) {
    if (strcmp(token, "%token") == 0) {
      tok_mode = 1;
    } else if (strcmp(token, "%start") == 0) {
      if (get_token(in, token) == EOF || strcmp(token, "%%") == 0) {
        error("expecting name of root of grammar");
      }
      strncpy(out->root, token, TOK_LEN);
      root_defined = 1;
      tok_mode = 0;
    } else if (token[0] != '%' && tok_mode == 1) {
      si_map_set(out->tok_map, token, count++);
    } else {
      error("invalid grammar file");
    }
  }

  if (root_defined == 0) {
    error("forgot to specify root with '%%start root'");
  }

  return out;
}

void Declarations_free(Declarations *decl)
{
  HashMap_deconstruct(decl->tok_map);
  free(decl);
}

static void print_enum_entry(const char *key, void *val, void *arg)
{
  FILE *of = (FILE *)arg;
  fprintf(of, "%s = %d,\n", key, *((int *)val));
}

void print_tok_enum_from_tok_map(FILE *out, HashMap *tok_map)
{
  fprintf(out, "enum type {\n");
  HashMap_iter(tok_map, print_enum_entry, (void *)out);
  fprintf(out, "};\n");
}

static void insert_rule_list(const char *key, void *val, void *maps)
{
  char rule_key[RULE_LEN];
  ProdRule *iter = *((ProdRule **)val);
  int pos = 0;
  RuleMaps *rmaps = (RuleMaps *)maps;

  si_map_set(rmaps->lhs_map, key, (int)iter->sym_no);
  for (; iter != NULL; iter = iter->next) {
    rule_map_key_gen(rule_key, iter);
    si_map_set(rmaps->total_map, rule_key, iter->rule_no);
    rmaps->lhs_first_rules[iter->sym_no] = iter->rule_no;
  }
}

static void set_rule_num_elts(const char *key, void *val, void *maps)
{
  char rule_key[RULE_LEN];
  ProdRule *iter = *((ProdRule **)val);
  RuleMaps *rmaps = (RuleMaps *)maps;
  int rnum;

  for (; iter != NULL; iter = iter->next) {
    rule_map_key_gen(rule_key, iter);
    rnum = si_map_get(rmaps->total_map, rule_key);
    rmaps->rule_num_elts[rnum] = iter->num_symbols;
  }
}

// map every rule to a unique number
RuleMaps RuleMaps_construct(HashMap *gmap)
{
  RuleMaps out;
  out.total_map = si_map_init();
  out.lhs_map = si_map_init();

  out.lhs_first_rules = calloc(gmap->size, sizeof(int));

  HashMap_iter(gmap, insert_rule_list, (void *)&out);

  out.rule_num_elts = calloc(out.total_map->size, sizeof(int));

  HashMap_iter(gmap, set_rule_num_elts, (void *)&out);

  return out;
}

void RuleMaps_free(RuleMaps maps)
{
  HashMap_deconstruct(maps.total_map);
  HashMap_deconstruct(maps.lhs_map);
  free(maps.lhs_first_rules);
  free(maps.rule_num_elts);
}



/******************************************************************************/
/* Grammar map                                                                */
/******************************************************************************/

static void gmap_print_el(const char *key, void *val, void *_)
{
  printf("Non-terminal: '%s'\n", key);
  ProdRule_print_list(*((ProdRule **)val));
}

void gmap_print(HashMap *gmap)
{
  HashMap_iter(gmap, gmap_print_el, NULL);
}

void ProdRule_print(ProdRule *rule)
{
  int i;
  printf("[%s <- ", rule->sym);
  for (i = 0; i < rule->num_symbols; i++) {
    printf(" '%s' ", rule->sym_l[i]);
  }
  printf(", %d] ", rule->rule_no);
  printf("%s", rule->red_op);
}

void ProdRule_print_list(ProdRule *list)
{
  ProdRule *rule_iter;

  for (rule_iter = list; rule_iter != NULL; rule_iter = rule_iter->next) {
    ProdRule_print(rule_iter);
    printf("\n");
  }
}

ProdRule *ProdRule_append(ProdRule *list, ProdRule *rule)
{
  ProdRule *last;

  APPEND(list, last, rule);
}

ProdRule *ProdRule_generate(const char *sym, int rule_no, int sym_no)
{
  ProdRule *out = malloc(sizeof(ProdRule));
  out->rule_no = rule_no;
  out->sym_no = sym_no;
  out->sym = strdup(sym);
  memset(out->red_op, '\0', RED_OP_LEN);
  out->num_symbols = 0;
  out->next = NULL;
  return out;
}


ProdRule *ProdRule_generate_list(FILE *g_file, const char *sym, int *rule_no, int sym_no)
{
  char token[TOK_LEN];
  char red_op[RED_OP_LEN], red_op_token[TOK_LEN], *redp, *prev_redp, old;
  ProdRule *tmp_rule;
  int num_read;
  
  ProdRule *prod_l;

  prod_l = NULL;
  tmp_rule = ProdRule_generate(sym, (*rule_no)++, sym_no);

  while (get_token(g_file, token) != EOF && token[strlen(token)-1] != ':' &&
      strcmp(token, "%%") != 0) {
    if (token[0] == '|') {
      continue;
    }

    if (token[0] != '{') {
      if (tmp_rule->num_symbols < MAX_TERMS_PER_RULE) {
        tmp_rule->sym_l[tmp_rule->num_symbols++] = strdup(token);
      } else {
        error("Grammar parsing: exceeded maximum number of symbols"
            "per production rule. "
            "Can't be over '%d'", MAX_TERMS_PER_RULE);
      }
    } else {
      num_read = read_until(g_file, red_op, RED_OP_LEN, '}');
      if (num_read == EOF || num_read > RED_OP_LEN) {
        error("could not read reduction operation for rule %d\n",
            tmp_rule->sym_no);
      }

      redp = red_op;
      prev_redp = red_op;
      strcpy(tmp_rule->red_op, "{");
      while ((redp = strstr(redp, "$")) != NULL) {
        *redp = '\0';
        if (redp[1] == '$') {
          redp += 2;
          strcpy(red_op_token, "ppval");
        } else {
          strcpy(red_op_token, "stack_vals[");
          while (isdigit(*(++redp))) {
            old = redp[1];
            redp[1] = '\0';
            strcat(red_op_token, redp);
            redp[1] = old;
          }
          strcat(red_op_token, "]");
        }
        strcat(tmp_rule->red_op, prev_redp);
        strcat(tmp_rule->red_op, red_op_token);
        prev_redp = redp;
      }
      strcat(tmp_rule->red_op, prev_redp);
      strcat(tmp_rule->red_op, "}");

      prod_l = ProdRule_append(prod_l, tmp_rule);
      tmp_rule = ProdRule_generate(sym, (*rule_no)++, sym_no);
    }
  }

  ProdRule_free(tmp_rule);
  (*rule_no)--;

  if (token[strlen(token)-1] == ':')
    unget_token(g_file, token);

  return prod_l;
}

static void print_red_op_switch_el(const char *key, void *val, void *outf)
{
  FILE *out = (FILE *)outf;
  ProdRule *iter = *((ProdRule **)val);

  for (; iter != NULL; iter = iter->next) {
    fprintf(out, "case %d:\n", iter->rule_no);
    fprintf(out, "\t%s\n", iter->red_op);
    fprintf(out, "\tbreak;\n");
  }
}

void print_red_op_switch(FILE *out, HashMap *gmap)
{
  fprintf(out, "switch (rule_no) {\n");
  HashMap_iter(gmap, print_red_op_switch_el, (void *)out);
  fprintf(out, "}\n");
}



HashMap *gmap_generate(FILE *g_file, const char *root)
{
  char token[TOK_LEN];
  ProdRule *prod_l;
  HashMap *gram_map;

  gram_map = HashMap_construct(sizeof(ProdRule *));

  int rule_no = 0, sym_no = 0;
  prod_l = ProdRule_generate("%root", rule_no++, sym_no++);
  prod_l->num_symbols = 1;
  prod_l->sym_l[0] = strdup(root);
  HashMap_set(gram_map, "%root", (void *)&prod_l);

  while ((get_token(g_file, token) != EOF) && (strcmp(token, "%%") != 0)) {
    if (token[strlen(token)-1] == ':') {
      token[strlen(token)-1] = '\0';
      prod_l = ProdRule_generate_list(g_file, token, &rule_no, sym_no++);
      HashMap_set(gram_map, token, (void *)&prod_l);
    } else {
      error("syntax: non-terminals must be defined with a"
          "':' without spaces separating it from the non-terminal name");
    }
  }
  return gram_map;
}

void ProdRule_free(ProdRule *rule)
{
  int i;
  for (i = 0; i < rule->num_symbols; i++) {
    free(rule->sym_l[i]);
  }
  free(rule->sym);
  free(rule);
}

void ProdRule_free_list(ProdRule *prod_l)
{
  ProdRule *next;

  for (; prod_l != NULL; prod_l = next) {
    next = prod_l->next;
    ProdRule_free(prod_l);
  }
}

static void gmap_free_val(const char *_1, void *val, void *_2)
{
  ProdRule_free_list(*((ProdRule **)val));
}

void gmap_free(HashMap *gmap)
{
  HashMap_iter(gmap, gmap_free_val, NULL);
  HashMap_deconstruct(gmap);
}


/******************************************************************************/
/* First map																																	*/
/******************************************************************************/


FirstSetEl *fmap_get(HashMap *fmap, const char *name)
{
  FirstSetEl **out;
  if ((out = HashMap_get(fmap, name, NULL)) == NULL) {
    return NULL;
  }
  return *out;
}

void fmap_insert_el(const char *key, void *val, void *map)
{
  FirstSetEl *fset_el;
  fset_el = fset_insert(NULL, *((int *)val));
  HashMap_set((HashMap *)map, key, (void *)&fset_el);
}

HashMap *fmap_generate(HashMap *gmap, const char *root, HashMap *tok_map)
{
  HashMap *out;
  FirstSetEl *fset_el;

  out = HashMap_construct(sizeof(FirstSetEl *));

  si_map_set(out, "NONE", 0);
  HashMap_iter(tok_map, fmap_insert_el, out);

  fmap_generate_here(out, root, gmap);
  return out;
}

void fmap_generate_here(HashMap *fmap, const char *name, HashMap *gmap)
{
  ProdRule *prod_l, *rule_iter;
  FirstSetEl *fset, *set_iter, **ret;

  if ((HashMap_get(fmap, name, NULL)) == NULL) {
    // generate new first map element with empty items list
    fset = NULL;
    // assume name exists in map because we generate first sets only for
    // entries from grammar map
    prod_l = *((ProdRule **)HashMap_get(gmap, name, NULL));
    for (rule_iter = prod_l; rule_iter != NULL; rule_iter = rule_iter->next) {
      if (strcmp(name, rule_iter->sym_l[0]) == 0)
        continue;
      fmap_generate_here(fmap, rule_iter->sym_l[0], gmap);
      ret = (FirstSetEl **)HashMap_get(fmap, rule_iter->sym_l[0], NULL);

      for (set_iter = *ret; set_iter != NULL; set_iter = set_iter->next) {
        fset = fset_insert(fset, set_iter->token_type);
      }
    }
    HashMap_set(fmap, name, (void *)&fset);
  }
}

void fmap_print_el(const char *name, void *val, void *_)
{
  FirstSetEl *fset = *((FirstSetEl **)val);

  printf("FIRST(%s): {", (char *)name);
  for (; fset != NULL; fset = fset->next) {
    printf("%d,", fset->token_type); 
  }
  printf("}\n");
}

void fmap_print(HashMap *fmap)
{
  HashMap_iter(fmap, fmap_print_el, NULL);
}

void fmap_free(HashMap *fmap)
{
  HashMap_iter(fmap, fset_free, NULL);
  HashMap_deconstruct(fmap);
}

/******************************************************************************/
/* First set                                                                  */
/******************************************************************************/


FirstSetEl *fset_insert(FirstSetEl *start, int t)
{
  FirstSetEl *new, *iter, *prev;

  prev = NULL;
  for (iter = start; iter != NULL; iter = iter->next) {
    if (t < iter->token_type) {
      new = malloc(sizeof(FirstSetEl));
      new->token_type = t;
      new->next = iter;
      if (prev != NULL) {
        prev->next = new;
        return start;
      } else {
        return new;
      }
    } else if (t == iter->token_type) { // don't add items that exist in set
                                        // already
      return start;
    }
    prev = iter;
  }

  // if it is not smaller than any other element insert it at the end
  new = malloc(sizeof(FirstSetEl)); // rewrite malloc because it might be
                                    // avoided in case when t is in set already
                                    // this way no unnecessary memory allocations
                                    // will be performed

  new->token_type = t;
  new->next = NULL;
  if (prev == NULL) {
    return new;
  } else {
    prev->next = new;
    return start;
  }
}



void fset_free(const char *_1, void *val, void *_2)
{
  FirstSetEl *next, *iter;
  for (iter = *((FirstSetEl **)val); iter != NULL; iter = next) {
    next = iter->next;
    free(iter);
  }
}

/******************************************************************************/
/* Canonical Collection Set																										*/
/******************************************************************************/

int cc_set_contains(LR1El *set, const char *sym, char *sym_l[],
                    int pos, int lookahead, int num_rule_items)
{
  LR1El *iter;

  for (iter = set; iter != NULL; iter = iter->next) {
    if (strcmp(iter->rule->sym, sym) == 0 &&
        string_set_equal(iter->rule->sym_l,
          sym_l, iter->rule->num_symbols, num_rule_items) &&
        iter->pos == pos && iter->lookahead == lookahead)
      return 1;
  }

  return 0;
}

int cc_set_is_subset(LR1El *set, LR1El *potential_subset)
{
  LR1El *iter;
  for (iter = potential_subset; iter != NULL; iter = iter->next) {
    if (!(cc_set_contains(set, iter->rule->sym, iter->rule->sym_l, iter->pos,
            iter->lookahead, iter->rule->num_symbols))) {
      return 0;
    }
  }
  return 1;
}

int cc_set_equal(LR1El *a, LR1El *b)
{
  return (cc_set_is_subset(a, b) && cc_set_is_subset(b, a));
}

LR1El *cc_set_append(LR1El *set, const char *sym, char *sym_l[],
                    int pos, int lookahead, int num_rule_items)
{
  if (cc_set_contains(set, sym, sym_l,
        pos, lookahead, num_rule_items)) {
    return set;
  }
  LR1El *new = malloc(sizeof(LR1El));
  new->rule = ProdRule_generate(sym, 0, 0); // don't care about rule numbers
                                            // here since they have already
                                            // been extracted to other maps
  new->rule->num_symbols = num_rule_items;
  string_set_copy(sym_l, new->rule->sym_l, num_rule_items);
  new->pos = pos;
  new->lookahead = lookahead;
  new->next = NULL;

  LR1El *last;

  APPEND(set, last, new);
}

void cc_set_free(LR1El *set)
{
  LR1El *next;
  int i;

  for (; set != NULL; set = next) {
    next = set->next;
    ProdRule_free(set->rule);
    free(set);
  }
}

void cc_set_print(LR1El *set)
{
  int i;

  printf("{\n");
  for (; set != NULL; set = set->next) {
    printf("[%s -> ", set->rule->sym);
    for (i = 0; i < set->rule->num_symbols; i++) {
      if (set->pos == i)
        printf(" o");
      printf(" %s", set->rule->sym_l[i]);
    }
    if (set->pos == i)
      printf(" o");
    printf(", %d]\n", set->lookahead);

  }
  printf("}\n");
}



// compute complete set of LR1 elements by trying to expand all of the
// LR1 elements in set at the parsing position to get further LR1 elements
LR1El *closure_set(LR1El *set, HashMap *fmap, HashMap *gmap)
{
  LR1El *iter;
  ProdRule *rule, **gmap_get_out;
  FirstSetEl *found;
  int lookahead, i;
  int counter = 0;


  for (iter = set; iter != NULL; iter = iter->next) {
    if (iter->pos < iter->rule->num_symbols &&
        (gmap_get_out =
         (ProdRule **)HashMap_get(gmap, iter->rule->sym_l[iter->pos], NULL)) != NULL) {
      rule = *gmap_get_out;
      for (; rule != NULL; rule = rule->next) {
        // go over all FIRST elements of
        if (iter->pos + 1 < iter->rule->num_symbols &&
            (found = fmap_get(fmap, iter->rule->sym_l[iter->pos + 1])) != NULL) {
          for (; found != NULL; found = found->next) {
            // insert into set
            // note that this function will not allow duplicates to be inserted
            set = cc_set_append(set, iter->rule->sym_l[iter->pos], rule->sym_l,
                  0, found->token_type, rule->num_symbols);
          }
        } else {
          set = cc_set_append(set, iter->rule->sym_l[iter->pos], rule->sym_l,
                0, iter->lookahead, rule->num_symbols);
        }
      }
    }
  }

  return set;
}

LR1El *goto_set(LR1El *set, const char *sym, HashMap *fmap, HashMap *gmap)
{
  LR1El *out = NULL;

  for (; set != NULL; set = set->next) {
    // if parsing position has already reached end of rule can't go anywhere
    if (set->pos + 1 <= set->rule->num_symbols &&
        // check if goto_set symbol follows current parsing position
        strcmp(set->rule->sym_l[set->pos], sym) == 0) {
      out = cc_set_append(out, set->rule->sym, set->rule->sym_l, set->pos + 1, set->lookahead,
          set->rule->num_symbols);
    }
  }

  return closure_set(out, fmap, gmap);
}


/******************************************************************************/
/* Canonical Collection of Sets																								*/
/******************************************************************************/


CC *CC_insert(CC *cc, int state_no, LR1El *cc_set)
{
  CC *out = malloc(sizeof(CC));
  out->state_no = state_no;
  out->cc_set = cc_set;

  out->goto_map = HashMap_construct(sizeof(CC *));

  out->next = cc;
  return out;
}

CC *CC_find(CC *cc, LR1El *cc_set)
{
  CC *iter;
  for (iter = cc; iter != NULL; iter = iter->next) {
    if (cc_set_equal(iter->cc_set, cc_set)) {
      return iter;
    }
  }
  return NULL;
}

void CC_deconstruct(CC *root)
{
  CC *next;
  for (; root != NULL; root = next) {
    next = root->next;
    HashMap_deconstruct(root->goto_map);
    cc_set_free(root->cc_set);
    free(root);
  }
}

CC *CC_construct(HashMap *gmap, HashMap *fmap, const char *root)
{
  CC *out, *workset, *goto_target;
  CCStack *workstack;  
  LR1El *cc_set, *iter_set;
  ProdRule *rule, **gmap_get_out;
  int state_no;

  out = NULL;
  workstack = NULL;
  cc_set = NULL; // empty set
  state_no = 0;
  workset = malloc(sizeof(CC));

  // get first item in grammar map
  if ((gmap_get_out = (ProdRule **)HashMap_get(gmap, root, NULL)) != NULL) {
    rule = *gmap_get_out;
    for (; rule != NULL; rule = rule->next) {
      cc_set = cc_set_append(cc_set, root, rule->sym_l, 0, NONE, rule->num_symbols);
    }
  }
  cc_set = closure_set(cc_set, fmap, gmap);
  out = CC_insert(out, state_no, cc_set);

  workstack = CCStack_push(workstack, out);
  while (workstack != NULL) {
    workstack = CCStack_pop(workstack, workset);
    for (iter_set = workset->cc_set; iter_set != NULL; iter_set = iter_set->next) {
      if (iter_set->pos < iter_set->rule->num_symbols) {
        cc_set = goto_set(workset->cc_set, iter_set->rule->sym_l[iter_set->pos], fmap, gmap);
        if ((goto_target = CC_find(out, cc_set)) == NULL) {
          out = CC_insert(out, ++state_no, cc_set);
          workstack = CCStack_push(workstack, out);
          goto_target = out;
        } else {
          cc_set_free(cc_set);
        }
        HashMap_set(workset->goto_map, iter_set->rule->sym_l[iter_set->pos],
            (void *)&goto_target);
      }
    }
  }

  free(workset);
  return out;
}

static void conn_print(const char *key, void *val, void *_)
{
  CC *conn_item = *((CC **) val);
  printf("if '%s' -> %d\n", key, conn_item->state_no);
}

void CC_print(CC *cc)
{
  for (; cc != NULL; cc = cc->next) {
    printf("State %d: ", cc->state_no);
    cc_set_print(cc->cc_set);
    printf("Connected to these states over these connections: [\n");
    HashMap_iter(cc->goto_map, conn_print, NULL);
    printf("]\n\n");
  }
}



CCStack *CCStack_push(CCStack *stack, CC *cc)
{
  CCStack *new = malloc(sizeof(CCStack));
  new->cc = cc;
  new->next = stack;
  return new;
}

CCStack *CCStack_pop(CCStack *stack, CC *out)
{
  memcpy(out, stack->cc, sizeof(CC));
  CCStack *new_first = stack->next;
  free(stack);
  return new_first;
}

void CCStack_free(CCStack *stack)
{
  CCStack *next;
  for (; stack != NULL; stack = next) {
    next = stack->next;
    free(stack);
  }
}
