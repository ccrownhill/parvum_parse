#ifndef PARSE_TYPES_H
#define PARSE_TYPES_H

#include <limits.h>

#include "parser.h"
#include "util_types.h"

#define APPEND(LIST, LAST, NEW) ({\
  if (LIST != NULL) {\
    for (LAST = LIST; LAST->next != NULL; LAST = LAST->next);\
    LAST->next = NEW;\
    return LIST;\
  } else {\
    return NEW;\
  }\
})

#define TOK_LEN 50
#define RULE_LEN 200
#define RED_OP_LEN 1000
#define MAX_TERMS_PER_RULE 10

typedef struct _ProdRule {
  int rule_no;
  int sym_no; // number of non-terminal symbol sym on LHS of production rule
  char *sym;
  int num_symbols;
  char *sym_l[MAX_TERMS_PER_RULE];
  char red_op[RED_OP_LEN];
  struct _ProdRule *next;
} ProdRule;


typedef struct _FirstSetEl {
  int token_type;
  struct _FirstSetEl *next;
} FirstSetEl;


// LR1 element to make up linked list representing a set in the
// canonical collection of sets
typedef struct _LR1El {
  ProdRule *rule;
  int pos;
  int lookahead;
  struct _LR1El *next;
} LR1El;


typedef struct _CC {
  int state_no;
  LR1El *cc_set;
  HashMap *goto_map;
  struct _CC *next;
} CC;

typedef struct _CCStack {
  CC *cc;
  struct _CCStack *next;
} CCStack;


// 
// typedef enum _ActionType {SHIFT, REDUCE, ACCEPT} ActionType;
// 
// typedef union _ActionInstr {
//   int state_no;
//   ProdRule *red_rule;
// } ActionInstr;
// 
// typedef struct _Action {
//   ActionType act_type;
//   ActionInstr act_instr;
// } Action;


typedef struct _PTable {
  int **action_t;
  int **goto_t;
  int num_rows;
  int num_terminals;
  int num_non_terminals;
  int root;
} PTable;


typedef struct _GotoParams {
  CC *cc;
  HashMap *tok_map;
  int **goto_t;
  HashMap *lhs_map;
} GotoParams;



typedef struct _Declarations {
  HashMap *tok_map;
  char root[TOK_LEN];
} Declarations;

typedef struct _RuleMaps {
  HashMap *total_map;
  HashMap *lhs_map;
  int *lhs_first_rules;
  int *rule_num_elts;
} RuleMaps;




int is_terminal(const char *name, HashMap *tok_map);


void key_gen(char *buf, int state_no, const char *token_type);
PTable PTable_construct(const char *root, CC *cc, HashMap *gmap,
    HashMap *tok_map, RuleMaps rmaps);
void add_to_goto(const char *key, void *val, void *params);
void act_table_free_el(const char *key, void *val, void *_);
void *state_list_reduce(const char *key, void *_, void *list);
void *non_terminals_list_reduce(const char *key, void *_, void *list);
void PTable_print(FILE *out, PTable table);
void PTable_free(PTable table);

void print_1d_arr_def(FILE *out, int *arr, int size, const char *name);
void print_2d_arr_def(FILE *out, int **arr, int rows, int cols, const char *name);

Declarations *Declarations_parse(FILE *in);
void Declarations_free(Declarations *decl);
void print_tok_enum_from_tok_map(FILE *out, HashMap *tok_map);

void rule_map_key_gen(char *key, ProdRule *rule);
RuleMaps RuleMaps_construct(HashMap *gmap);
void RuleMaps_free(RuleMaps maps);

ProdRule *ProdRule_generate(const char *sym, int rule_no, int sym_no);
ProdRule *ProdRule_append(ProdRule *list, ProdRule *rule);
ProdRule *ProdRule_generate_list(FILE *g_file, const char *sym, int *rule_no, int sym_no);
HashMap *gmap_generate(FILE *g_file, const char *root);
void ProdRule_print(ProdRule *rule);
void ProdRule_print_list(ProdRule *list);
void gmap_print(HashMap *gmap);
void print_red_op_switch(FILE *out, HashMap *gmap);

void ProdRule_free(ProdRule *rule);
void ProdRule_free_list(ProdRule *prod_l);
void gmap_free(HashMap *gmap);


FirstSetEl *fmap_get(HashMap *fmap, const char *name);
HashMap *fmap_generate(HashMap *gmap, const char *root, HashMap *tok_map);
void fmap_generate_here(HashMap *map, const char *name, HashMap *gmap);
void fmap_print(HashMap *fmap);
void fmap_free(HashMap *fmap);


FirstSetEl *fset_insert(FirstSetEl *start, int t);
void fset_free(const char *_1, void *val, void *_2);

int cc_set_contains(LR1El *set, const char *sym, char *sym_l[],
      int pos, int lookahead, int num_rule_items);

int cc_set_equal(LR1El *a, LR1El *b);
int cc_set_is_subset(LR1El *set, LR1El *potential_subset);

LR1El *cc_set_append(LR1El *set, const char *sym, char *sym_l[],
      int pos, int lookahead, int num_rule_items);

void cc_set_free(LR1El *set);

void cc_set_print(LR1El *set);


CCStack *CCStack_push(CCStack *stack, CC *cc);
CCStack *CCStack_pop(CCStack *stack, CC *out);
void CCStack_free(CCStack *stack);


LR1El *closure_set(LR1El *set, HashMap *fmap, HashMap *gmap);
LR1El *goto_set(LR1El *set, const char *sym, HashMap *fmap, HashMap *gmap);

CC *CC_insert(CC *cc, int state_no, LR1El *cc_set);
CC *CC_find(CC *cc, LR1El *cc_set);
void CC_deconstruct(CC *root);
CC *CC_construct(HashMap *gmap, HashMap *fmap, const char *root);
void CC_print(CC *cc);


#endif
