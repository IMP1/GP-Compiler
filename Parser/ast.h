/* ////////////////////////////////////////////////////////////////////////////////////// */

/*                                       ast.h                               
 * 
 * This is a header file for the GP2 parser. It contains enumerated
 * type definitions, AST node definitions and prototypes for AST constructors.
 * 
 *
 * Created on 28/5/13 by Chris Bak 
 *
 * /////////////////////////////////////////////////////////////////////////////////////// */


extern char *file_name; /* defined in gpparser.y */

/* Bison uses a global variable yylloc of type YYLTYPE to keep track of the locations of 
   tokens and nonterminals. The scanner will set these values upon reading each token. 
   This is the standard YYLTYPE definition but I define it here so it is seen by every file. */

typedef struct YYLTYPE {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;

# define YYLTYPE_IS_DECLARED 1 /* tells the parser that YYLTYPE is defined here */

/* enum used by the lexer for mark keywords */

typedef enum {RED, GREEN, BLUE, GREY, DASHED, NONE} mark_t; 


/* AST node, node type and constructor declarations */

typedef enum {GLOBAL_DECLS, LOCAL_DECLS, COMMANDS, RULES, INT_DECLS, STRING_DECLS, ATOM_DECLS, LIST_DECLS, VARIABLES, INTERFACE_LIST, NODE_LIST, EDGE_LIST, EQUAL, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL, GP_LIST} list_t;

typedef struct List {
  list_t list_type;  
  YYLTYPE location;  /* location of symbol in the source file */
  union {
    struct GPDeclaration *decl;
    struct GPStatement *command;
    char *rule_name;
    struct List *vars; /* multiple variables declared with the same type: INT_DECLS, STRING_DECLS, ATOM_DECLS */
    char *var; 	  
    struct GPNodePair *node_pair; /* pair of nodes specified in the interface of a rule */	   
    struct GPNode *node;    
    struct GPEdge *edge;    
    struct List *rel_exp; /* chains of relational operators */
    struct GPAtomicExp *atom;
  } value;
  struct List *next;
} List;

List *addDecl (list_t list_type, YYLTYPE location, struct GPDeclaration *decl, struct List *next);
List *addCommand (YYLTYPE location, struct GPStatement *command, struct List *next);
List *addRule (YYLTYPE location, char *rule_name, struct List *next);
List *addVariableDecl (list_t list_type, YYLTYPE location, struct List *vars, struct List *next);
List *addVariable (YYLTYPE location, char *var, struct List *next);
List *addNodePair (YYLTYPE location, struct GPNodePair *node_pair, struct List *next);
List *addNode (YYLTYPE location, struct GPNode *node, struct List *next);
List *addEdge (YYLTYPE location, struct GPEdge *edge, struct List *next);
List *addRelationalExp (list_t list_type, YYLTYPE location, struct List *rel_exp, struct List *next);
List *addAtom (YYLTYPE location, struct GPAtomicExp *atom, struct List *next);


/* AST nodes and constructors for global declarations */

typedef enum {MAIN_DECL, PROCEDURE_DECL, RULE_DECL} decl_t;

typedef struct GPDeclaration {
  decl_t decl_type;
  YYLTYPE location;
  union {
    struct GPStatement *main_program;
    struct GPProcedure *proc;
    struct GPRule *rule;
  } value;
} GPDeclaration;

GPDeclaration *newMainDecl (YYLTYPE location, struct GPStatement *main_program);
GPDeclaration *newProcedureDecl (YYLTYPE location, struct GPProcedure *proc);
GPDeclaration *newRuleDecl (YYLTYPE location, struct GPRule *rule);


/* AST nodes and constructors for GP program statements */

typedef enum {COMMAND_SEQUENCE, RULE_CALL, RULE_SET_CALL, PROCEDURE_CALL, IF_STMT, TRY_STMT, ALAP_STMT, PROGRAM_OR, SKIP_STMT, FAIL_STMT} stmt_t;

typedef struct GPStatement {
  stmt_t statement_type;
  YYLTYPE location;
  union {    
    struct List *cmd_seq;
    char *rule_name;
    struct List *rule_set;
    char *proc_name;
    struct { struct GPStatement *condition; struct GPStatement *then_stmt; struct GPStatement *else_stmt; } cond_branch; /* IF_STMT, TRY_STMT */
    struct GPStatement *loop_stmt;
    struct { struct GPStatement *left_stmt; struct GPStatement *right_stmt; } or_stmt;
    /* skip and fail are predefined rules represented by a struct GPStatement containing only a statementtype and location */
  } value;
} GPStatement;

GPStatement *newCommandSequence(YYLTYPE location, struct List *cmd_seq);
GPStatement *newRuleCall(YYLTYPE location, char *rule_name);
GPStatement *newRuleSetCall(YYLTYPE location, struct List *rule_set);
GPStatement *newProcCall(YYLTYPE location, char *proc_name);
GPStatement *newCondBranch(stmt_t statement_type, YYLTYPE location, struct GPStatement *condition, struct GPStatement *then_stmt, struct GPStatement *else_stmt);
GPStatement *newAlap(YYLTYPE location, struct GPStatement *loop_stmt);
GPStatement *newOrStmt(YYLTYPE location, struct GPStatement *left_stmt, struct GPStatement *right_stmt);
GPStatement *newSkip(YYLTYPE location);
GPStatement *newFail(YYLTYPE location);


/* AST node and constructors for condition expressions */

typedef enum {INT_CHECK, STRING_CHECK, ATOM_CHECK, EDGE_PRED, REL_EXP, BOOL_NOT, BOOL_OR, BOOL_AND} condexp_t;

typedef struct GPCondExp {
  condexp_t exp_type;
  YYLTYPE location;
  union {
    char *var; /* type checking predicates: INT_CHECK, STRING_CHECK, ATOM_CHECK */
    struct { char *source; char *target; struct GPLabel *label; } edge_pred;
    struct List *rel_exp; 
    struct GPCondExp *not_exp;
    struct { struct GPCondExp *left_exp; struct GPCondExp *right_exp; } bin_exp; /* OR, AND */
  } value;
} GPCondExp;

GPCondExp *newSubtypePred (condexp_t exp_type, YYLTYPE location, char *var);
GPCondExp *newEdgePred (YYLTYPE location, char *source, char *target, struct GPLabel *label);
GPCondExp *newRelationalExp (YYLTYPE location, struct List *rel_exp);
GPCondExp *newNotExp (YYLTYPE location, struct GPCondExp *not_exp);
GPCondExp *newBinaryExp (condexp_t exp_type, YYLTYPE location, struct GPCondExp *left_exp, struct GPCondExp *right_exp);


/* AST node and constructors for expressions of type int or string */

typedef enum {VARIABLE, INT_CONSTANT, STRING_CONSTANT, INDEGREE, OUTDEGREE, LIST_LENGTH, STRING_LENGTH, NEG, ADD, SUBTRACT, MULTIPLY, DIVIDE, CONCAT} atomexp_t;

typedef struct GPAtomicExp {
  atomexp_t exp_type;
  YYLTYPE location;
  union {
    char *var;
    int num;
    char *str;
    char *node_id; /* INDEGREE, OUTDEGREE */
    struct List *list_arg; /* list length query */
    struct GPAtomicExp *str_arg; /* string length query */
    struct GPAtomicExp *exp; /* negated expression */
    struct { struct GPAtomicExp *left_exp; struct GPAtomicExp *right_exp; } bin_op; /* ADD, SUBTRACT, MULTIPLY, DIVIDE, CONCAT */
  } value;
} GPAtomicExp;

GPAtomicExp *newVariable (YYLTYPE location, char *name);
GPAtomicExp *newNumber (YYLTYPE location, int num);
GPAtomicExp *newString (YYLTYPE location, char *str);
GPAtomicExp *newDegreeOp (atomexp_t exp_type, YYLTYPE location, char *node_id);
GPAtomicExp *newListLength (YYLTYPE location, struct List *list_arg);
GPAtomicExp *newStringLength (YYLTYPE location, struct GPAtomicExp *str_arg);
GPAtomicExp *newNegExp (YYLTYPE location, struct GPAtomicExp *exp);
GPAtomicExp *newBinaryOp (atomexp_t exp_type, YYLTYPE location, struct GPAtomicExp *left_exp, struct GPAtomicExp *right_exp);


/* Declarations of other AST nodes and constructors */

typedef enum {PROCEDURE, RULE, NODE_PAIR, GRAPH, NODE, EDGE, POSITION, LABEL} ast_node_t;

/* Procedure declaration */

typedef struct GPProcedure {
  ast_node_t node_type;
  YYLTYPE location;
  char *name; 
  struct List *local_decls; 
  struct GPStatement *cmd_seq; 
} GPProcedure;

GPProcedure *newProcedure(YYLTYPE location, char *name, struct List *local_decls, struct GPStatement *cmd_seq);


/* Rule declaration */

typedef struct GPRule {
  ast_node_t node_type;
  YYLTYPE location;
  int injective; /* 1 for injective matching, 0 otherwise */
  char *name; 
  struct List *variables;
  struct GPGraph *lhs;
  struct GPGraph *rhs;
  struct List *interface;
  struct GPCondExp *condition;  
} GPRule;

GPRule *newRule(YYLTYPE location, int injective, char *name, struct List *variables, struct GPGraph *lhs, struct GPGraph *rhs, struct List *interface, struct GPCondExp *condition);


/* Pairs of nodes for the interface portion of a rule */

typedef struct GPNodePair {
  ast_node_t node_type; /* NODE_PAIR */
  YYLTYPE location;
  char *left_node;
  char *right_node;
} GPNodePair;

GPNodePair *newNodePair (YYLTYPE location, char *left_node, char *right_node);


typedef struct GPGraph {
  ast_node_t node_type;	
  YYLTYPE location;
  struct GPPos *position;
  struct List *nodes;
  struct List *edges;
} GPGraph;

GPGraph *newGraph (YYLTYPE location, struct GPPos *position, struct List *nodes, struct List *edges);


typedef struct GPNode {
  ast_node_t node_type;	
  YYLTYPE location; 
  int root; /* 1 if node is a root, 0 otherwise */
  char *name; 
  struct GPLabel *label; 
  struct GPPos *position; 
} GPNode;

GPNode *newNode (YYLTYPE location, int root, char *name, struct GPLabel *label, struct GPPos *position);


typedef struct GPEdge {
  ast_node_t node_type;	
  YYLTYPE location; 
  char *name; 
  char *source; 
  char *target; 
  struct GPLabel *label; 
} GPEdge;

GPEdge *newEdge (YYLTYPE location, char *name, char *source, char *target, struct GPLabel *label);


/* Data for graph presentation in the GUI */

typedef struct GPPos {
  ast_node_t node_type; 
  YYLTYPE location; 
  int x;
  int y;
} GPPos;

GPPos *newPosition (YYLTYPE location, int x, int y);


typedef struct GPLabel {
  ast_node_t node_type; 
  YYLTYPE location; 
  mark_t mark;
  struct List *gp_list;
} GPLabel;

GPLabel *newLabel (YYLTYPE location, mark_t mark, struct List *gp_list);



