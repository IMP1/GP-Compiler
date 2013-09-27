/* //////////////////////////////////////////////////////////////////////////////////////////// */
/*
*						gpparser.y 					
* 
* This is the parser for GP2, written in Bison. In combination with Flex, it performs syntax 
* checking, creates a symbol table and generates an Abstract Syntax Tree for the input GP2 program.
*
* Created on 28/5/13 by Chris Bak 
* 
* Host Graphs must be parsed separately due to separate file entry and different start symbol,
* but the graph grammar of this parser can be used. I will focus on just GP programs for the
* time being.
* 
/* /////////////////////////////////////////////////////////////////////////////////////////// */


%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> /* for error functions */
#include "ast.h"

List *gp_program = NULL; /* root of the AST */
extern int yylineno; /* defined by lexer */
extern char *yytext; /* defined by lexer */
int yyerror(char *s);
void print_error(YYLTYPE loc, char *errormsg, ...);
char *file_name = NULL; /* for error messages */

/* flags for AST construction */
int is_root = 0;
int is_injective = 0;

%}

/* declare tokens */

%locations /* generates code to process locations. Automatically enabled when '@n' tokens used 
              in grammar */

%union {  /* defines types of tokens */
  int num;    							/* value of NUM token */
  char *str;  							/* value of STRING tokens */
  char *id;   							/* value of MACID and ID tokens */
  int mark; 							/* enum mark_t, value of MARK token */
}

/* single-character tokens do not need to be explicitly declared */
%token MAIN IF TRY THEN ELSE SKIP FAIL                          /* Program text keywords */
%token WHERE EDGETEST TRUE FALSE 		                        /* Schema condition keywords */
%token INDEG OUTDEG LLEN SLEN					/* Function keywords */
%token INT STRING ATOM LIST 	                                /* Type keywords */
%token INTERFACE EMPTY INJECTIVE 	
%token <mark> MARK			                        
%token ARROW					                
%token NEQ GTEQ LTEQ			                        /* Relational operators */
%token <num> NUM 
%token <str> STR 
%token <id> PROCID ID           				/* Identifiers */
%token ROOT							/* Root node */


%left '+' '-' AND	/* lowest precedence level, left associative */
%left '*' '/' OR
%left UMINUS NOT	/* UMINUS represents unary '-' */
%left '.'		/* highest precedence level */

%union {  /* defines types of non-terminal symbols */
  struct List *list; 
  struct GPDeclaration *decl;
  struct GPStatement *stmt;
  struct GPProcedure *proc;
  struct GPRule *rule;
  struct GPNodePair *node_pair;
  struct GPGraph *graph;
  struct GPNode *node;
  struct GPEdge *edge;
  struct GPPos *pos;
  struct GPCondExp *cond_exp;
  struct GPLabel *label;  
  struct GPAtomicExp *atom_exp;

  int list_type; /* enum list_t */
  int check_type; /* enum cond_exp_t */
} 

%type <list> Program ProcList ComSeq RuleSetCall IDList VarDecls VarList Inter NodePairList 
             NodeList EdgeList RelExp List
%type <decl> Declaration
%type <stmt> MainDecl Command Block SimpleCommand 
%type <proc> ProcDecl
%type <rule> RuleDecl
%type <node_pair> NodePair
%type <graph> Graph
%type <node> Node
%type <edge> Edge
%type <pos> Position
/* Type and Subtype */
%type <cond_exp> CondDecl Condition 
%type <label> LabelArg Label
%type <atom_exp> AtomExp
%type <list_type> Type RelOp 
%type <check_type> Subtype
%type <id> NodeID EdgeID ProcID RuleID Variable

%start GPProgram /* start symbol of the grammar */

%locations

%%

 /* Grammar for textual GP2 programs */

GPProgram: Program			{ gp_program = $1; }

Program: Declaration	      		{ $$ = addDecl(GLOBAL_DECLS, yylloc, $1, NULL); }  
       | Program Declaration            { $$ = addDecl(GLOBAL_DECLS, yylloc, $2, $1); }  

Declaration: MainDecl 			{ $$ = newMainDecl(yylloc, $1); }
     	   | ProcDecl			{ $$ = newProcedureDecl(yylloc, $1); }
           | RuleDecl			{ $$ = newRuleDecl(yylloc, $1); }

MainDecl: MAIN '=' ComSeq		{ $$ = newCommandSequence(yylloc,$3); }

ProcDecl: ProcID '=' ComSeq 		{ $$ = newProcedure(yylloc, $1, NULL, 
                                               newCommandSequence(yylloc,$3)); }
        | ProcID '=' '[' ProcList ']' ComSeq 
					{ $$ = newProcedure(yylloc, $1, $4, 
                                               newCommandSequence(yylloc,$6)); }

ProcList: /* empty */			{ $$ = NULL; }
        | ProcList RuleDecl             { $$ = addDecl(LOCAL_DECLS, yylloc, 
                                               newRuleDecl(yylloc, $2), $1); }
	| ProcList ProcDecl 		{ $$ = addDecl(LOCAL_DECLS, yylloc,
                                               newProcedureDecl(yylloc, $2), $1); }

ComSeq: Command 			{ $$ = addCommand(yylloc, $1, NULL); }
      | ComSeq ';' Command  		{ $$ = addCommand(yylloc, $3, $1); }

Command: Block 				/* default $$ = $1 */
       | IF Block THEN Block      	{ $$ = newCondBranch(IF_STMT, yylloc, $2, $4, newSkip(yylloc)); }
       | IF Block THEN Block ELSE Block { $$ = newCondBranch(IF_STMT, yylloc, $2, $4, $6); }
       | TRY Block 			{ $$ = newCondBranch(TRY_STMT, yylloc, $2, 
                                               newSkip(yylloc), newSkip(yylloc)); }
       | TRY Block THEN Block		{ $$ = newCondBranch(TRY_STMT, yylloc, $2, $4, newSkip(yylloc)); }
       | TRY Block THEN Block ELSE Block { $$ = newCondBranch(TRY_STMT, yylloc, $2, $4, $6); }

Block: '(' ComSeq ')' 	                { $$ = newCommandSequence(yylloc,$2); }
     | '(' ComSeq ')' '!' 		{ $$ = newAlap(yylloc, newCommandSequence(yylloc,$2)); } 
     | SimpleCommand 			/* default $$ = $1 */
     | SimpleCommand  '!'		{ $$ = newAlap(yylloc, $1); }
     | Block OR Block 			{ $$ = newOrStmt(yylloc, $1, $3); }
     | SKIP				{ $$ = newSkip(yylloc); }
     | FAIL				{ $$ = newFail(yylloc); }

SimpleCommand: RuleSetCall 	        { $$ = newRuleSetCall(yylloc, $1); }
             | RuleID                   { $$ = newRuleCall(yylloc, $1); }
	     | ProcID	 		{ $$ = newProcCall(yylloc, $1); }

RuleSetCall: '{' IDList '}'		{ $$ = $2; } 

IDList: RuleID				{ $$ = addRule(yylloc, $1, NULL); }
      | IDList ',' RuleID 		{ $$ = addRule(yylloc, $3, $1); } 

 /* Grammar for GP2 conditional rule schemata */

RuleDecl: RuleID '(' VarDecls ')' '[' Graph ']' ARROW '[' Graph ']' Inter CondDecl INJECTIVE '=' Bool
					{ $$ = newRule(yylloc, is_injective,
 					        $1, $3, $6, $10, $12, $13); }
					  
VarDecls: /* empty */			{ $$ = NULL; }
	| VarList ':' Type		{ $$ = addVariableDecl($3, yylloc, $1, NULL); }  
	| VarDecls ';' VarList ':' Type { $$ = addVariableDecl($5, yylloc, $3, $1); }

 /* some post-processing could be done to sort out the parameter list AST here as the AST generated
    in this manner is a bit strange. Need to remove the intermediate ASTs created from the reduction
    of the second VarList production so that their parents point directly to the variables. This may be achieved during AST construction with some pointer manipulation. 8/8/13: I suspect the above is nonsense. */

VarList: Variable 			{ $$ = addVariable(yylloc, $1, NULL); }
       | VarList ',' Variable          	{ $$ = addVariable(yylloc, $3, $1); }

Inter: INTERFACE '{' NodePairList '}'   { $$ = $3; }

NodePairList: /* empty */		{ $$ = NULL; }
	    | NodePair			{ $$ = addNodePair(yylloc, $1, NULL); }
            | NodePairList ',' NodePair { $$ = addNodePair(yylloc, $3, $1);   }

NodePair: '(' NodeID ',' NodeID ')'   	{ $$ = newNodePair(yylloc, $2, $4); }

Bool: TRUE 				{ is_injective = 1; }
    | FALSE				{ is_injective = 0; }

Type: INT				{ $$ = INT_DECLS; } 
    | STRING                            { $$ = STRING_DECLS; }
    | ATOM 	                        { $$ = ATOM_DECLS; }
    | LIST				{ $$ = LIST_DECLS; }

 /* Grammar for GP2 graphs */

Graph: Position '|' NodeList '|' EdgeList 
     					{ $$ = newGraph(yylloc, $1, $3, $5); }

NodeList: /* empty */			{ $$ = NULL; }
        | Node				{ $$ = addNode(yylloc, $1, NULL); }
        | NodeList ',' Node		{ $$ = addNode(yylloc, $3, $1); }

Node: '(' NodeID RootNode ',' Label ',' Position ')'
    					{ $$ = newNode(yylloc, is_root, $2, $5, $7); 
 					  is_root = 0; /* reset root flag */	    }

EdgeList: /* empty */			{ $$ = NULL; }
	| Edge				{ $$ = addEdge(yylloc, $1, NULL); }
        | EdgeList ',' Edge		{ $$ = addEdge(yylloc, $3, $1); }

Edge: '(' EdgeID ',' NodeID ',' NodeID ',' Label ')'
					{ $$ = newEdge(yylloc, $2, $4, $6, $8); }
RootNode: /* empty */
	| ROOT 				{ is_root = 1; }

Position: '(' NUM ',' NUM ')' 		{ $$ = newPosition(yylloc, $2, $4); }

 /* Grammar for GP2 conditions */

CondDecl: /* empty */                   { $$ = NULL; }
        | WHERE Condition		{ $$ = $2; }

Condition: Subtype '(' Variable ')' 	{ $$ = newSubtypePred($1, yylloc, $3); }
         | EDGETEST '(' NodeID ',' NodeID LabelArg ')' 
					{ $$ = newEdgePred(yylloc, $3, $5, $6); }	
         | RelExp 			{ $$ = newRelationalExp(yylloc, $1); }
         | NOT Condition	        { $$ = newNotExp(yylloc, $2); }
         | Condition OR Condition  	{ $$ = newBinaryExp(BOOL_OR, yylloc, $1, $3); }
         | Condition AND Condition      { $$ = newBinaryExp(BOOL_AND, yylloc, $1, $3); }
	 | '(' Condition ')' 		{ $$ = $2; }

Subtype: INT				{ $$ = INT_CHECK; } 
       | STRING                         { $$ = STRING_CHECK; }
       | ATOM 	                        { $$ = ATOM_CHECK; }

LabelArg: /* empty */			{ $$ = NULL; }
 	| ',' Label			{ $$ = $2; }

RelExp: List RelOp List 		{ $$ = addRelationalExp($2, yylloc, $1, $3); }
      | RelExp RelOp List 		{ $$ = addRelationalExp($2, yylloc, $3, $1); }

RelOp: '='				{ $$ = EQUAL; }
     | NEQ				{ $$ = NOT_EQUAL; }
     | '>'				{ $$ = GREATER; }
     | GTEQ				{ $$ = GREATER_EQUAL; }
     | '<'				{ $$ = LESS; }
     | LTEQ				{ $$ = LESS_EQUAL; }

  /* Grammar for GP2 Labels */

Label: List 				{ $$ = newLabel(yylloc, NONE, $1); }
     | List '#' MARK			{ $$ = newLabel(yylloc, $3, $1); } 

List: EMPTY  				{ $$ = addAtom(yylloc, NULL, NULL); }
    | AtomExp				{ $$ = addAtom(yylloc, $1, NULL); } 
    | List ':' AtomExp			{ $$ = addAtom(yylloc, $3, $1); }

AtomExp: Variable			{ $$ = newVariable(yylloc, $1); }
       | NUM 				{ $$ = newNumber(yylloc, $1); }
       | STR 				{ $$ = newString(yylloc, $1); }
       | INDEG '(' NodeID ')' 		{ $$ = newDegreeOp(INDEGREE, yylloc, $3); }
       | OUTDEG '(' NodeID ')' 		{ $$ = newDegreeOp(OUTDEGREE, yylloc, $3); }
       | LLEN '(' List ')' 		{ $$ = newListLength(yylloc, $3); }
       | SLEN '(' AtomExp ')' 		{ $$ = newStringLength(yylloc, $3); }
       | '-' AtomExp %prec UMINUS 	{ $$ = newNegExp(yylloc, $2); } 
       | '(' AtomExp ')' 		{ $$ = $2; }
       | AtomExp '+' AtomExp 		{ $$ = newBinaryOp(ADD, yylloc, $1, $3);  }
       | AtomExp '-' AtomExp 		{ $$ = newBinaryOp(SUBTRACT, yylloc, $1, $3); }
       | AtomExp '*' AtomExp 		{ $$ = newBinaryOp(MULTIPLY, yylloc, $1, $3); }
       | AtomExp '/' AtomExp 		{ $$ = newBinaryOp(DIVIDE, yylloc, $1, $3); }
	/* Ambiguity resolved by explicit precedences */
       | AtomExp '.' AtomExp 		{ $$ = newBinaryOp(CONCAT, yylloc, $1, $3); }

 /* Identifiers */

 /* symtable contains scoping information for rules/macros */
ProcID: PROCID 				/* default $$ = $1 */
RuleID: ID		         	/* default $$ = $1 */
NodeID: ID				/* default $$ = $1 */
EdgeID: ID				/* default $$ = $1 */
Variable: ID		  		/* default $$ = $1 */

%%

/* add a token printing procedure here for debugging */

int main(int argc, char** argv) {
       
  extern FILE *yyin; 
 
  if(argc > 1 && !strcmp(argv[1], "-d")) {  /* if called with the -d flag */
    yydebug = 1; 	/* for producing the debugging file gpparser.output */
    argc--; argv++;	/* effectively removing "-d" from the command line call */
  }

  if(argc != 2) {
    fprintf(stderr, "ERROR: filename required\n");
    return 1;
  }

  if(!(yyin = fopen(argv[1], "r"))) {	/* flex scans from yyin. */
     perror(argv[1]);
     yylineno = 1;	
     return 1;
  }

  file_name = argv[1];
  printf("Processing %s\n", file_name);

  if(!yyparse()) {
    printf("GP2 parse succeeded\n\n");
    printf("GP Program %s\n\n", file_name); 
    print_ast(gp_program);  
  }
  else printf("GP2 parse failed\n");
 
  fclose(yyin);  

  return 0;
}

/* default bison error function, implicitly uses the location stored in yylloc */

int yyerror(char *s)
{
   if(yylloc.first_line)
     fprintf(stderr, "%s:%d.%d-%d.%d: error at '%s': %s\n", file_name, yylloc.first_line,
       yylloc.first_column, yylloc.last_line, yylloc.last_column, yytext, s);

   return 0;
}

/* alternate error function for an explicit location */

void print_error(YYLTYPE location, char *errormsg, ...)
{
   va_list args;
   va_start(args, errormsg);

   if(location.first_line)
     fprintf(stderr, "%s:%d.%d-%d.%d: error at '%s': ", file_name, location.first_line,
       location.first_column, location.last_line, location.last_column, yytext);
     vfprintf(stderr, errormsg, args);
     fprintf(stderr, "\n");

   va_end(args);
}
        
