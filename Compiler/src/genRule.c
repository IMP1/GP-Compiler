/* Copyright 2015-2016 Christopher Bak

  This file is part of the GP 2 Compiler. The GP 2 Compiler is free software: 
  you can redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation, either version 3
  of the License, or (at your option) any later version.

  The GP 2 Compiler is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
  more details.

  You should have received a copy of the GNU General Public License
  along with the GP 2 Compiler. If not, see <http://www.gnu.org/licenses/>. */

#include "genRule.h"

static void generateMatchingCode(Rule *rule, bool predicate);
static void emitDegreeCheck(RuleNode *left_node, int indent);
static void emitRootNodeMatcher(Rule *rule, RuleNode *left_node, SearchOp *next_op);
static void emitNodeMatcher(Rule *rule, RuleNode *left_node, SearchOp *next_op);
static void emitNodeFromEdgeMatcher(Rule *rule, RuleNode *left_node, char type, SearchOp *next_op);
static void emitNodeMatchResultCode(RuleNode *node, SearchOp *next_op, int indent);
static void emitEdgeMatcher(Rule *rule, RuleEdge *left_edge, SearchOp *next_op);
static void emitLoopEdgeMatcher(Rule *rule, RuleEdge *left_edge, SearchOp *next_op);
static void emitEdgeFromNodeMatcher(Rule *rule, RuleEdge *left_edge, bool source,
                                    bool initialise, bool exit, SearchOp *next_op);
static void emitEdgeMatchResultCode(int index, SearchOp *next_op, int indent);
static void emitNextMatcherCall(SearchOp *next_operation);

FILE *header = NULL;
FILE *file = NULL;
Searchplan *searchplan = NULL;

void generateRules(List *declarations, string output_dir)
{
   while(declarations != NULL)
   {
      GPDeclaration *decl = declarations->declaration;
      switch(decl->type)
      {
         case MAIN_DECLARATION:
              break;

         case PROCEDURE_DECLARATION:
              if(decl->procedure->local_decls != NULL)
                 generateRules(decl->procedure->local_decls, output_dir);
              break;

         case RULE_DECLARATION:
         {
              Rule *rule = transformRule(decl->rule);
              /* Annotate the AST's rule declaration node with information about
               * the rule. This is used when generating code to execute the GP 2
               * program. */
              decl->rule->empty_lhs = rule->lhs == NULL;
              decl->rule->is_predicate = isPredicate(rule);
              generateRuleCode(rule, decl->rule->is_predicate, output_dir);
              freeRule(rule);
              break;
         }
         default: 
              print_to_log("Error (generateRules): Unexpected declaration type "
                           "%d at AST node %d\n", decl->type, decl->id);
              break;
      }
      declarations = declarations->next;
   }
}

   const string ADDED_NODE = "__new_node";
   const string ADDED_EDGE = "__new_edge";
//   const string RELABLED_NODE = "__relabled_node";
//   const string RELABLED_EDGE = "__relabled_edge";
//   const string REMARKED_NODE = "__remarked_node";
//   const string REMARKED_EDGE = "__remarked_edge";
//   const string REMOVED_INCOMING_NODE = "__removed_in_node";
//   const string REMOVED_OUTGOING_NODE = "__removed_out_node";
   

/* Create a C module to match and apply the rule. */
void generateRuleCode(Rule *rule, bool predicate, string output_dir)
{
   /* Create files <output dir>/<rule name>.h and <output dir>/<rule name>.c */
   int length = strlen(output_dir) + strlen(rule->name) + 3;

   char header_name[length];
   strcpy(header_name, output_dir);
   strcat(header_name, "/");
   strcat(header_name, rule->name);
   strcat(header_name, ".h");

   header = fopen(header_name, "w");
   if(header == NULL) { 
      perror(header_name);
      exit(1);
   }  

   char file_name[length];
   strcpy(file_name, output_dir);
   strcat(file_name, "/");
   strcat(file_name, rule->name);
   strcat(file_name, ".c");

   file = fopen(file_name, "w");
   if(file == NULL) { 
      perror(file_name);
      exit(1);
   }

   PTH("#include \"globals.h\"\n"
       "#include \"graph.h\"\n"
       "#include \"label.h\"\n"
       "#include \"graphStacks.h\"\n"
       "#include \"hostParser.h\"\n"
       "#include \"morphism.h\"\n\n");   
   PTF("#include \"%s.h\"\n\n", rule->name);
  
   // ~IMP1 
   PTH("#ifndef HIGHLIGHT_STRUCT\n");
   PTH("#define HIGHLIGHT_STRUCT\n\n");
   PTH("typedef struct Highlight {\n");
   PTH("   int host_index;\n");
   PTH("   char* highlight;\n");
   PTH("} Highlight;\n\n");
   PTH("#endif\n\n");
   
   PTH("int get%sAddedNodes(void);\n", rule->name);
   PTH("int get%sAddedEdges(void);\n", rule->name);
   PTH("Highlight *get%sNodeHighlights(void);\n", rule->name);
   PTH("Highlight *get%sEdgeHighlights(void);\n\n", rule->name);
   
   PTF("#include <stdio.h>\n"); // ~IMP debug TODO remove
   PTF("static int added_node_count = 0;\n");
   PTF("static Highlight *node_highlights;\n");
   PTF("static int added_edge_count = 0;\n");
   PTF("static Highlight *edge_highlights;\n\n");

   PTF("int get%sAddedNodes(void)\n", rule->name);
   PTF("{\n");
   PTF("   return added_node_count;\n");
   PTF("}\n");
   
   PTF("int get%sAddedEdges(void)\n", rule->name);
   PTF("{\n");
   PTF("   return added_edge_count;\n");
   PTF("}\n");
   
   PTF("Highlight *get%sNodeHighlights(void)\n", rule->name);
   PTF("{\n");
   PTF("   return node_highlights;\n");
   PTF("}\n");
   
   PTF("Highlight *get%sEdgeHighlights(void)\n", rule->name);
   PTF("{\n");
   PTF("   return edge_highlights;\n");
   PTF("}\n");

   if(rule->condition != NULL)
   {
      /* The condition is iterated over three times.
       * The first iteration declares and initialises the runtime global boolean
       * varables, one for each predicate in the condition.
       * The second iteration writes the function to evaluate the condition.
       * The third iteration writes the functions to evaluate the predicates. */
      generateConditionVariables(rule->condition);
      PTF("\n");
      generateConditionEvaluator(rule->condition, false);
      generatePredicateEvaluators(rule, rule->condition);
   }
   if(rule->lhs != NULL) 
   {
      generateMatchingCode(rule, predicate);
      if(!predicate)
      {
         if(rule->rhs == NULL) generateRemoveLHSCode(rule->name);
         else generateApplicationCode(rule);
      }
   }
   else
   {
      if(rule->rhs != NULL) generateAddRHSCode(rule);
   }
   fclose(header);
   fclose(file);
   return;
}

static void generateMatchingCode(Rule *rule, bool predicate)
{
   searchplan = generateSearchplan(rule->lhs); 
   if(searchplan->first == NULL)
   {
      print_to_log("Error: empty searchplan. Aborting.\n");
      freeSearchplan(searchplan);
      return;
   }
   SearchOp *operation = searchplan->first;
   /* Iterator over the searchplan to print the prototypes of the matching functions. */
   while(operation != NULL)
   {
      char type = operation->type;
      switch(type)
      {
         case 'n':
         case 'r':
              PTF("static bool match_n%d(Morphism *morphism);\n", operation->index);
              break;

         case 'i': 
         case 'o': 
         case 'b':
              PTF("static bool match_n%d(Morphism *morphism, Edge *host_edge);\n",
                  operation->index);
              break;

         case 'e': 
         case 's': 
         case 't':
         case 'l':
              PTF("static bool match_e%d(Morphism *morphism);\n", operation->index);
              break;

         default:
              print_to_log("Error (generateMatchingCode): Unexpected "
                           "operation type %c.\n", operation->type);
              break;
      }
      operation = operation->next;
   }
   /* Generate the main matching function which sets up the runtime matching 
    * environment and calls the first matching function. */
   PTH("bool match%s(Morphism *morphism);\n\n", rule->name);
   PTF("\nbool match%s(Morphism *morphism)\n", rule->name);
   PTF("{\n");
   PTFI("if(%d > host->number_of_nodes || %d > host->number_of_edges) return false;\n",
        3, rule->lhs->node_index, rule->lhs->edge_index);
   char item = searchplan->first->is_node ? 'n' : 'e';
   
   if(predicate)
   {
      PTFI("bool match = match_%c%d(morphism);\n", 3, item, searchplan->first->index);
      /* Reset the matched flags in the host graph. This is normally done after
       * rule application, but predicate rules are not applied. */
      PTFI("initialiseMorphism(morphism, host);\n", 3);
      PTFI("return match;\n", 3);
   }
   else 
   {
      PTFI("if(match_%c%d(morphism)) return true;\n", 3, item, searchplan->first->index);
      PTFI("else\n", 3);
      PTFI("{\n", 3);
      PTFI("initialiseMorphism(morphism, host);\n", 6);
      PTFI("return false;\n", 6);
      PTFI("}\n", 3);
   }
   PTF("}\n\n");

   /* Iterator over the searchplan to print the definitions of the matching functions. */
   operation = searchplan->first;
   RuleNode *node = NULL;
   RuleEdge *edge = NULL;
   while(operation != NULL)
   {
      switch(operation->type)
      {        
         case 'r': 
              node = getRuleNode(rule->lhs, operation->index);
              emitRootNodeMatcher(rule, node, operation->next);
              break;

         case 'n': 
              node = getRuleNode(rule->lhs, operation->index);
              emitNodeMatcher(rule, node, operation->next);
              break;

         case 'i': 
         case 'o': 
         case 'b':
              node = getRuleNode(rule->lhs, operation->index);
              emitNodeFromEdgeMatcher(rule, node, operation->type, operation->next);
              break;

         case 'e': 
              edge = getRuleEdge(rule->lhs, operation->index);
              emitEdgeMatcher(rule, edge, operation->next);
              break;

         case 'l':
              edge = getRuleEdge(rule->lhs, operation->index);
              emitLoopEdgeMatcher(rule, edge, operation->next);
              break;

         case 's': 
              edge = getRuleEdge(rule->lhs, operation->index);
              if(edge->bidirectional) 
              {
                 emitEdgeFromNodeMatcher(rule, edge, true, true, false, operation->next);
                 emitEdgeFromNodeMatcher(rule, edge, false, false, true, operation->next);
              }
              else emitEdgeFromNodeMatcher(rule, edge, true, true, true, operation->next);
              break;

         case 't':
              edge = getRuleEdge(rule->lhs, operation->index);
              if(edge->bidirectional) 
              {
                 emitEdgeFromNodeMatcher(rule, edge, false, true, false, operation->next);
                 emitEdgeFromNodeMatcher(rule, edge, true, false, true, operation->next);
              }
              else emitEdgeFromNodeMatcher(rule, edge, false, true, true, operation->next);
              break;
         
         default:
              print_to_log("Error (generateMatchingCode): Unexpected "
                           "operation type %c.\n", operation->type);
              break;
      }
      operation = operation->next;
   }
   freeSearchplan(searchplan);
}


/* The host node does not match the rule node if:
 * (1) The host node's indegree is strictly less than the rule node's indegree.
 * (2) The host node's outdegree is strictly less than the rule node's outdegree.
 * (3) The number of edges incident to the host node is strictly less than the
 *     number of edges incident to the rule node. 
 *
 * For nodes deleted by the rule, the third condition is stricter because of
 * the dangling condition:
 * (3) The number of edges incident to the host node is not equal to the 
 *     number of edges incident to the rule node. Indeed, if it is less,
 *     then standard matching is violated (above). If it is greater,
 *     then the dangling condition is violated. */

static void emitDegreeCheck(RuleNode *left_node, int indent)
{
   /* For condition (3) above, the number of edges incident to the host node
    * is given by the sum of the outdegree and the indegree. The edges
    * incident to the rule node is the sum of the node's outdegree, indegree
    * and bidegree. The number of rule edges is subtracted from the number of
    * host edges and the result is compared to 0. */
   if(left_node->interface == NULL)
   {
      /* Dangling node degree check. If the if condition evaluates to true,
       * then the node is not a valid match. */
      PTFI("if(host_node->indegree < %d || host_node->outdegree < %d ||\n",
           indent, left_node->indegree, left_node->outdegree);
      PTFI("   ((host_node->outdegree + host_node->indegree - %d - %d - %d) != 0)) ", 
           indent, left_node->outdegree, left_node->indegree, left_node->bidegree);
   }
   else
   {
      /* Standard node degree check. */
      PTFI("if(host_node->indegree < %d || host_node->outdegree < %d ||\n",
           indent, left_node->indegree, left_node->outdegree);
      PTFI("   ((host_node->outdegree + host_node->indegree - %d - %d - %d) < 0)) ", 
           indent, left_node->outdegree, left_node->indegree, left_node->bidegree);
   }
}

 
/* The emitMatcher functions in this module take an LHS item and emit a function 
 * that searches for a matching host item. The generated code queries the host graph
 * for the appropriate item or list of items according to the LHS item and the
 * searchplan operation from which the code is generated.
 *
 * Several checks are made by each function to check if a host item matches
 * the LHS-item in the order presented below.
 * The host item must:
 * (1) Not have been matched already (GP2 requires injective matching).
 * (2) Have the same mark as the the LHS-item (if the LHS-item's mark is not ANY).
 * (3) [Nodes only] Have degree compatibility with the LHS-node. For instance,
 *     if the candidate host node has fewer outgoing edges than the rule node,
 *     no match is possible. This code is generated by emitDegreeCheck, defined
 *     above.
 * (4) Have label compatibility with the LHS-item. This is the last step because
 *     label matching is more computationally demanding than the other steps.
 *
 * If a valid host item is found, the generated code pushes its index to the
 * appropriate morphism stack and calls the function for the following 
 * searchplan operation (see emitNextMatcherCall). If there are no operations 
 * left, code is generated to return true. */
static void emitRootNodeMatcher(Rule *rule, RuleNode *left_node, SearchOp *next_op)
{
   PTF("static bool match_n%d(Morphism *morphism)\n", left_node->index);
   PTF("{\n");
   PTFI("RootNodes *nodes;\n", 3);   
   PTFI("for(nodes = getRootNodeList(host); nodes != NULL; nodes = nodes->next)\n", 3);
   PTFI("{\n", 3);
   PTFI("Node *host_node = getNode(host, nodes->index);\n", 6);
   PTFI("if(host_node == NULL) continue;\n", 6);
   PTFI("if(host_node->matched) continue;\n", 6);
   if(left_node->label.mark != ANY)
      PTFI("if(host_node->label.mark != %d) continue;\n", 6, left_node->label.mark);
   emitDegreeCheck(left_node, 6);  
   PTF("continue;\n\n");

   PTFI("HostLabel label = host_node->label;\n", 6);
   PTFI("bool match = false;\n", 6);
   if(hasListVariable(left_node->label))
      generateVariableListMatchingCode(rule, left_node->label, 6);
   else generateFixedListMatchingCode(rule, left_node->label, 6);
   emitNodeMatchResultCode(left_node, next_op, 6);
   PTFI("}\n", 3);
   PTFI("return false;\n", 3);
   PTF("}\n\n");
}

/* The rule node is matched "in isolation", in that it is not the source or
 * target of a previously-matched edge. In this case, the candidate host
 * graph nodes are obtained from the appropriate label class tables. */
static void emitNodeMatcher(Rule *rule, RuleNode *left_node, SearchOp *next_op)
{
   PTF("static bool match_n%d(Morphism *morphism)\n", left_node->index);
   PTF("{\n");
   PTFI("int host_index;\n", 3);
   PTFI("for(host_index = 0; host_index < host->nodes.size; host_index++)\n", 3);
   PTFI("{\n", 3);
   PTFI("Node *host_node = getNode(host, host_index);\n", 6);
   PTFI("if(host_node == NULL || host_node->index == -1) continue;\n", 6);
   PTFI("if(host_node->matched) continue;\n", 6);
   if(left_node->label.mark != ANY)
      PTFI("if(host_node->label.mark != %d) continue;\n", 6, left_node->label.mark);
   emitDegreeCheck(left_node, 6);  
   PTF("continue;\n\n");

   PTFI("HostLabel label = host_node->label;\n", 6);
   PTFI("bool match = false;\n", 6);
   if(hasListVariable(left_node->label))
      generateVariableListMatchingCode(rule, left_node->label, 6);
   else generateFixedListMatchingCode(rule, left_node->label, 6);
   emitNodeMatchResultCode(left_node, next_op, 6);
   PTFI("}\n", 3);
   PTFI("return false;\n", 3);
   PTF("}\n\n");
}

/* Matching a node from a matched incident edge always follow an edge match in
 * the searchplan. The generated function takes the host edge matched by  
 * the previous searchplan function as one of its arguments. It gets the
 * appropriate host node (source or target of the host edge) and checks if this
 * node is compatible with the rule node. 
 * The type argument is either 'i', 'o', or 'b'. */
static void emitNodeFromEdgeMatcher(Rule *rule, RuleNode *left_node, char type,
                                    SearchOp *next_op)
{
   PTF("static bool match_n%d(Morphism *morphism, Edge *host_edge)\n",
       left_node->index);
   PTF("{\n");
   if(type == 'i' || type == 'b') 
        PTFI("Node *host_node = getTarget(host, host_edge);\n\n", 3);
   else PTFI("Node *host_node = getSource(host, host_edge);\n\n", 3);

   string fail_code = (type == 'b') ? "candidate_node = false;" : "return false;";
   if(type == 'b') PTFI("bool candidate_node = true;\n", 3);
   PTFI("if(host_node->matched) %s\n", 3, fail_code);
   if(left_node->root) PTFI("if(!(host_node->root)) %s\n", 3, fail_code);
   if(left_node->label.mark != ANY)
      PTFI("if(host_node->label.mark != %d) %s\n", 3, left_node->label.mark, fail_code);
   emitDegreeCheck(left_node, 6);  
   PTF("%s;\n\n", fail_code);

   /* If the above check fails and the edge is bidirectional, check the other 
    * node incident to the host edge. Otherwise return false. */
   if(type == 'b')
   {
      PTFI("if(!candidate_node)\n", 3);
      PTFI("{\n", 3); 
      PTFI("/* Matching from bidirectional edge: check the second incident node. */\n", 6);
      if(type == 'i' || type == 'b') 
           PTFI("host_node = getSource(host, host_edge);\n", 6);
      else PTFI("host_node = getTarget(host, host_edge);\n", 6);
      PTFI("if(host_node->matched) return false;\n", 6);
      if(left_node->root) PTFI("if(!(host_node->root)) return false;\n", 6);
      if(left_node->label.mark != ANY)
         PTFI("if(host_node->label.mark != %d) return false;\n", 6, left_node->label.mark);
      emitDegreeCheck(left_node, 6);  
      PTF("return false;\n\n");
      PTFI("}\n", 3);
   }

   PTFI("HostLabel label = host_node->label;\n", 3);
   PTFI("bool match = false;\n", 3);
   if(hasListVariable(left_node->label))
      generateVariableListMatchingCode(rule, left_node->label, 3);
   else generateFixedListMatchingCode(rule, left_node->label, 3);

   emitNodeMatchResultCode(left_node, next_op, 3);
   PTFI("return false;\n", 3);
   PTF("}\n\n");
}

/* Generates code to test the result of label matching a node. If the label
 * matching succeeds, any predicates in which the node participates are evaluated
 * and the condition checked. If everything succeeds, the morphism and matched_nodes
 * array are updated, and matching continues. If not, any runtime boolean variables
 * modified by predicate evaluation are reset, and any assignments made during label
 * matching are undone. */
static void emitNodeMatchResultCode(RuleNode *node, SearchOp *next_op, int indent)
{
   PTFI("if(match)\n", indent);
   PTFI("{\n", indent);
   PTFI("addNodeMap(morphism, %d, host_node->index, new_assignments);\n",
        indent + 3, node->index);
   PTFI("host_node->matched = true;\n", indent + 3);
   if(node->predicates != NULL)
   {
      PTFI("/* Update global booleans representing the node's predicates. */\n", indent + 3);
      int index;
      for(index = 0; index < node->predicate_count; index++)
         PTFI("evaluatePredicate%d(morphism);\n", indent + 3, 
              node->predicates[index]->bool_id);
      if(next_op != NULL) PTFI("bool next_match_result = false;\n", indent + 3);
      PTFI("if(evaluateCondition())", indent + 3);
      if(next_op == NULL)
      { 
         PTF("\n");
         PTFI("{\n", indent + 3);
         PTFI("/* All items matched! */\n", indent + 6);
         PTFI("return true;\n", indent + 6);
         PTFI("}\n", indent + 3);
      }
      else
      {
         PTF(" next_match_result = ");
         emitNextMatcherCall(next_op);
         PTF(";\n");
         PTFI("if(next_match_result) return true;\n", indent + 3);           
      }
      PTFI("else\n", indent + 3);
      PTFI("{\n", indent + 3);  
      PTFI("/* Reset the boolean variables in the predicates of this node. */\n", 
            indent + 6);
      for(index = 0; index < node->predicate_count; index++)
      { 
         Predicate *predicate = node->predicates[index];
         if(predicate->negated) PTFI("b%d = false;\n", indent + 6, predicate->bool_id);
         else PTFI("b%d = true;\n", indent + 6, predicate->bool_id);
      }
      PTFI("removeNodeMap(morphism, %d);\n", indent + 6, node->index);
      PTFI("host_node->matched = false;\n", indent + 6);  
      PTFI("}\n", indent + 3);
   }
   else
   {
      if(next_op == NULL)
      {
         PTFI("/* All items matched! */\n", indent + 3);
         PTFI("return true;\n", indent + 3);
      }
      else
      {
         PTFI("if(", indent + 3);
         emitNextMatcherCall(next_op); 
         PTF(") return true;\n");            
         PTFI("else\n", indent + 3);
         PTFI("{\n", indent + 3);  
         PTFI("removeNodeMap(morphism, %d);\n", indent + 6, node->index);
         PTFI("host_node->matched = false;\n", indent + 6);  
         PTFI("}\n", indent + 3);
      }
   }
   PTFI("}\n", indent);
   /* The else branch of the "if(match)" printed at the top of this function. */
   PTFI("else removeAssignments(morphism, new_assignments);\n", indent);
}

/* The rule edge is matched "in isolation", in that it is not incident to a
 * previously-matched node. In this case, the candidate host graph edges
 * are obtained from the appropriate label class tables. */
static void emitEdgeMatcher(Rule *rule, RuleEdge *left_edge, SearchOp *next_op)
{
   PTF("static bool match_e%d(Morphism *morphism)\n", left_edge->index);
   PTF("{\n");
   PTFI("int host_index;\n", 3);
   PTFI("for(host_index = 0; host_index < host->edges.size; host_index++)\n", 3);
   PTFI("{\n", 3);
   PTFI("Edge *host_edge = getEdge(host, host_index);\n", 6);
   PTFI("if(host_edge == NULL || host_edge->index == -1) continue;\n", 6);
   PTFI("if(host_edge->matched) continue;\n", 6);
   if(left_edge->label.mark != ANY) 
      PTFI("if(host_edge->label.mark != %d) continue;\n\n", 6, left_edge->label.mark);
   PTFI("HostLabel label = host_edge->label;\n", 6);
   PTFI("bool match = false;\n", 6);
   if(hasListVariable(left_edge->label))
      generateVariableListMatchingCode(rule, left_edge->label, 6);
   else generateFixedListMatchingCode(rule, left_edge->label, 6);
   emitEdgeMatchResultCode(left_edge->index, next_op, 6);
   PTFI("}\n", 3);
   PTFI("return false;\n", 3);
   PTF("}\n\n");
}

static void emitLoopEdgeMatcher(Rule *rule, RuleEdge *left_edge, SearchOp *next_op)
{
   PTF("static bool match_e%d(Morphism *morphism)\n", left_edge->index);
   PTF("{\n");
   PTFI("/* Matching a loop. */\n", 3);
   PTFI("int node_index = lookupNode(morphism, %d);\n", 3, left_edge->source->index);
   PTFI("if(node_index < 0) return false;\n", 3);
   PTFI("Node *host_node = getNode(host, node_index);\n\n", 3);

   PTFI("int counter;\n", 3);
   PTFI("for(counter = 0; counter < host_node->out_edges.size + 2; counter++)\n", 3);
   PTFI("{\n", 3);
   PTFI("Edge *host_edge = getNthOutEdge(host, host_node, counter);\n", 6);
   PTFI("if(host_edge == NULL) continue;\n", 6);
   PTFI("if(host_edge->matched) continue;\n", 6);
   PTFI("if(host_edge->source != host_edge->target) continue;\n", 6);
   if(left_edge->label.mark != ANY)
      PTFI("if(host_edge->label.mark != %d) continue;\n\n", 6, left_edge->label.mark);
   PTFI("HostLabel label = host_edge->label;\n", 6);
   PTFI("bool match = false;\n", 6);
   if(hasListVariable(left_edge->label))
      generateVariableListMatchingCode(rule, left_edge->label, 6);
   else generateFixedListMatchingCode(rule, left_edge->label, 6);
   emitEdgeMatchResultCode(left_edge->index, next_op, 6);
   PTFI("}\n", 3);
   PTFI("return false;\n}\n\n", 3);
}

/* The following function matches a rule edge from one of its matched incident
 * nodes. Unlike matching a node from a matched incident edge, the LHS-node from
 * which this LHS-edge is matched may not necessarily be the previously matched 
 * node in the searchplan. The generated code uses the index of the incident nodes
 * of the LHS-edge to find the host node to which it has been matched. The candidate
 * host edges come from the edges lists of that node. 
 *
 * Called for various searchplan operations: matching an edge from its source,
 * matching an edge from its target, and matching bidirectional edges. In 
 * particular, the bidirectional edge code is generated by two calls to this
 * function. The skeleton of the generated code is largely the same for these 
 * operations, but different parts of the code printed by this function are required
 * for different operations. This is controlled by the three boolean flags:
 *
 * source - When set, the generated code searches from the match of the source of 
 *          the rule edge. Otherwise, it searches from the match of the target.
 * initialise - When set, this prints the header of the generated matching function.
 *              This is set in all cases except for the second call in the generation
 *              of bidirectional edge matching code.
 * exit - When set, this prints the return statement of the generated matching function.
 *        This is set in all cases except for the first call in the generation of
 *        bidirectional edge matching code. */
static void emitEdgeFromNodeMatcher(Rule *rule, RuleEdge *left_edge, bool source,
                                    bool initialise, bool exit, SearchOp *next_op)
{
   int start_index = source ? left_edge->source->index : left_edge->target->index;
   int end_index = source ? left_edge->target->index : left_edge->source->index;
   string end_node_type = source ? "target" : "source";

   if(initialise)
   {
      PTF("static bool match_e%d(Morphism *morphism)\n", left_edge->index);
      PTF("{\n");
      PTFI("/* Start node is the already-matched node from which the candidate\n", 3);
      PTFI("   edges are drawn. End node may or may not have been matched already. */\n", 3);
      PTFI("int start_index = lookupNode(morphism, %d);\n", 3, start_index);
      PTFI("int end_index = lookupNode(morphism, %d);\n", 3, end_index);
      PTFI("if(start_index < 0) return false;\n", 3);
      PTFI("Node *host_node = getNode(host, start_index);\n\n", 3);
      PTFI("int counter;\n", 3);
   }
   if(source)
   {
      PTFI("for(counter = 0; counter < host_node->out_edges.size + 2; counter++)\n", 3);
      PTFI("{\n", 3);
      PTFI("Edge *host_edge = getNthOutEdge(host, host_node, counter);\n", 6);
   }
   else
   {
      PTFI("for(counter = 0; counter < host_node->in_edges.size + 2; counter++)\n", 3);
      PTFI("{\n", 3);
      PTFI("Edge *host_edge = getNthInEdge(host, host_node, counter);\n", 6);
   }

   PTFI("if(host_edge == NULL) continue;\n", 6);
   PTFI("if(host_edge->matched) continue;\n", 6);
   PTFI("if(host_edge->source == host_edge->target) continue;\n", 6);
   if(left_edge->label.mark != ANY)
      PTFI("if(host_edge->label.mark != %d) continue;\n\n", 6, left_edge->label.mark);

   PTFI("/* If the end node has been matched, check that the %s of the\n", 6, end_node_type);
   PTFI(" * host edge is the image of the end node. */\n", 6);
   PTFI("if(end_index >= 0)\n", 6);
   PTFI("{\n", 6);
   PTFI("if(host_edge->%s != end_index) continue;\n", 9, end_node_type);
   PTFI("}\n", 6);
   PTFI("/* Otherwise, the %s of the host edge should be unmatched. */\n", 6, end_node_type);
   PTFI("else\n", 6);
   PTFI("{\n", 6);
   PTFI("Node *end_node = getNode(host, host_edge->%s);\n", 9, end_node_type);
   PTFI("if(end_node->matched) continue;\n", 9);
   PTFI("}\n\n", 6);

   PTFI("HostLabel label = host_edge->label;\n", 6);
   PTFI("bool match = false;\n", 6);
   if(hasListVariable(left_edge->label))
      generateVariableListMatchingCode(rule, left_edge->label, 6);
   else generateFixedListMatchingCode(rule, left_edge->label, 6);
   emitEdgeMatchResultCode(left_edge->index, next_op, 6);
   PTFI("}\n", 3);

   if(exit) PTFI("return false;\n}\n\n", 3);
}

/* Generates code to test the result of label matching a edge. If the label matching
 * succeeds, the morphism and matched_edges array are updated, and matching
 * continues. If not,  any assignments made during label matching are undone. */
static void emitEdgeMatchResultCode(int index, SearchOp *next_op, int indent)
{
   PTFI("if(match)\n", indent);
   PTFI("{\n", indent);
   PTFI("addEdgeMap(morphism, %d, host_edge->index, new_assignments);\n", indent + 3, index);
   PTFI("host_edge->matched = true;\n", indent + 3);
   if(next_op == NULL)
   {
      PTFI("/* All items matched! */\n", indent);
      PTFI("return true;\n", indent);
   }
   else
   {
      PTFI("if(", indent + 3);
      emitNextMatcherCall(next_op); 
      PTF(") return true;\n");           
      PTFI("else\n", indent + 3);
      PTFI("{\n", indent + 3);                              
      PTFI("removeEdgeMap(morphism, %d);\n", indent + 6, index);
      PTFI("host_edge->matched = false;\n", indent + 6); 
      PTFI("}\n", indent + 3);
   } 
   PTFI("}\n", indent);
   PTFI("else removeAssignments(morphism, new_assignments);\n", indent);
}

static void emitNextMatcherCall(SearchOp *next_operation)
{
   switch(next_operation->type)
   {
      case 'n':
      case 'r':
           PTF("match_n%d(morphism)", next_operation->index);
           break;

      case 'i':
      case 'o':
      case 'b':
           PTF("match_n%d(morphism, host_edge)", next_operation->index);
           break;
  
      case 'e':
      case 's':
      case 't':
      case 'l':
           PTF("match_e%d(morphism)", next_operation->index);
           break;

      default:
           print_to_log("Error (emitNextMatcherCall): Unexpected "
                        "operation type %c.\n", next_operation->type);
           break;
   }
}

void generateRemoveLHSCode(string rule_name)
{
   PTH("void apply%s(Morphism *morphism, bool record_changes);\n", rule_name);
   PTF("void apply%s(Morphism *morphism, bool record_changes)\n", rule_name);
   PTF("{\n");

   PTFI("int count;\n", 3);
   PTFI("for(count = 0; count < morphism->edges; count++)\n", 3);
   PTFI("{\n", 3);                        
   PTFI("if(record_changes)\n", 6);
   PTFI("{\n", 6);
   PTFI("Edge *edge = getEdge(host, morphism->edge_map[count].host_index);\n", 9);
   PTFI("/* A hole is created if the edge is not at the right-most index of the array. */\n", 9);
   PTFI("pushRemovedEdge(edge->label, edge->source, edge->target, edge->index,\n", 9);
   PTFI("                edge->index < host->edges.size - 1);\n", 9);  
   PTFI("}\n", 6);
   PTFI("removeEdge(host, morphism->edge_map[count].host_index);\n", 6);
   PTFI("}\n", 3);
                                                                           
   PTFI("for(count = 0; count < morphism->nodes; count++)\n", 3);
   PTFI("{\n", 3);                        
   PTFI("if(record_changes)\n", 6);
   PTFI("{\n", 6);
   PTFI("Node *node = getNode(host, morphism->node_map[count].host_index);\n", 9); 
   PTFI("/* A hole is created if the node is not at the right-most index of the array. */\n", 9);
   PTFI("pushRemovedNode(node->root, node->label, node->index,\n", 9);
   PTFI("                node->index < host->nodes.size - 1);\n", 9);  
   PTFI("}\n", 6);
   PTFI("removeNode(host, morphism->node_map[count].host_index);\n", 6);
   PTFI("}\n", 3);
   PTFI("initialiseMorphism(morphism, NULL);\n", 3);
   PTFI("}\n\n", 3);
}

void generateAddRHSCode(Rule *rule)
{
   PTH("void apply%s(bool record_changes);\n", rule->name);
   PTF("void apply%s(bool record_changes)\n", rule->name);
   PTF("{\n");
   PTFI("int index;\n", 3);
   PTFI("HostLabel label;\n\n", 3);
   /* Generate code to retrieve the values assigned to the variables in the
    * matching phase. */
   PTFI("/* Get the values of variables used in rule application. */\n", 3);
   int index;
   for(index = 0; index < rule->variables; index++)
   {
      Variable variable = rule->variable_list[index];
      if(variable.used_by_rule) generateVariableCode(index, variable.type);
   }
   PTF("\n");
   /* Flag to prevent repeated writing of "label = blank_label" when
    * consecutive blank nodes are added to the graph. */
   bool blank_label = false;
   if(rule->adds_edges)
   {
      PTFI("/* Array of host node indices indexed by RHS node index. */\n", 3);
      PTFI("int map[%d];\n\n", 3, rule->rhs->node_index);
   }
   for(index = 0; index < rule->rhs->node_index; index++)
   {
      /* Add each node to the host graph. If the rule adds edges, extra
       * code is emitted to maintain a rule-to-host index map so that
       * the correct edges are added. */
      RuleNode *node = getRuleNode(rule->rhs, index);
      if(node->label.mark == NONE && node->label.length == 0)
      {
         if(!blank_label)
         {
            PTFI("label = blank_label;\n", 3);
            blank_label = true;
         }
      }
      else generateLabelEvaluationCode(node->label, true, index, 0, 3);
      PTFI("int node_array_size%d = host->nodes.size;\n", 3, index);
      PTFI("index = addNode(host, %d, label);\n", 3, node->root);
      if(rule->adds_edges) PTFI("map[%d] = index;\n", 3, node->index);
      PTFI("/* If the node array size has not increased after the node addition, then\n", 3);
      PTFI("   the node was added to a hole in the array. */\n", 3);
      PTFI("if(record_changes)\n", 3);
      PTFI("pushAddedNode(index, node_array_size%d == host->nodes.size);\n", 6, index);
   }
   PTF("\n");
   for(index = 0; index < rule->rhs->edge_index; index++)
   {
      RuleEdge *edge = getRuleEdge(rule->rhs, index);
      if(edge->label.mark == NONE && edge->label.length == 0)
      {
         if(!blank_label)
         {
            PTFI("label = blank_label;\n", 3);
            blank_label = true;
         }
      }
      else generateLabelEvaluationCode(edge->label, false, index, 0, 3);
      /* The host-source and host-target of added edges are taken from the 
       * map populated in the previous loop. */
      PTFI("int edge_array_size%d = host->edges.size;\n", 3, index);
      PTFI("index = addEdge(host, label, map[%d], map[%d]);\n",
           3, edge->source->index, edge->target->index);
      PTFI("/* If the edge array size has not increased after the edge addition, then\n", 3);
      PTFI("   the edge was added to a hole in the array. */\n", 3);
      PTFI("if(record_changes)\n", 3);
      PTFI("pushAddedEdge(index, edge_array_size%d == host->edges.size);\n", 6, index);
   }     
   PTF("}\n");
   return;
}

void generateApplicationCode(Rule *rule)
{
   PTH("void apply%s(Morphism *morphism, bool record_changes);\n", rule->name);
   PTF("void apply%s(Morphism *morphism, bool record_changes)\n", rule->name);
   PTF("{\n");
   PTF("   added_node_count = 0;\n");
   PTF("   added_edge_count = 0;\n\n");
   
   /* Generate code to retrieve the values assigned to the variables in the
    * matching phase. */
   int index;
   for(index = 0; index < rule->variables; index++)
   {
      Variable variable = rule->variable_list[index];
      if(variable.used_by_rule) generateVariableCode(index, variable.type);
   }
   bool node_index_declared = false;
   for(index = 0; index < rule->lhs->node_index; index++)
   {
      RuleNode *node = getRuleNode(rule->lhs, index);
      if(node->indegree_arg || node->outdegree_arg)
      {
         if(!node_index_declared) 
         {
            PTFI("int node_index = lookupNode(morphism, %d);\n", 3, index);
            node_index_declared = true;
         }
         else PTFI("node_index = lookupNode(morphism, %d);\n", 3, index);
         if(node->indegree_arg) 
            PTFI("int indegree%d = getIndegree(host, node_index);\n", 3, index);
         if(node->outdegree_arg) 
            PTFI("int outdegree%d = getOutdegree(host, node_index);\n", 3, index);
      }
   }
   bool label_declared = false, host_edge_index_declared = false,
        host_node_index_declared = false;
   /* Host graph modifications are performed in the following order: 
    * (1) Delete/relabel edges.
    * (2) Delete/relabel nodes.
    * (3) Add nodes.
    * (4) Add edges.
    *
    * Nodes must be added before edges are added, because the incident nodes of
    * added edges may be added nodes. Edges must be deleted before nodes are
    * deleted because deleting nodes first may leave dangling edges. */

   /* Variable passed to generateLabelEvaluationCode. */
   int list_count = 0;
   /* (1) Delete/relabel edges. */
   for(index = 0; index < rule->lhs->edge_index; index++)
   {
      RuleEdge *edge = getRuleEdge(rule->lhs, index);
      if(edge->interface == NULL) 
      {
         /* Generate code to remove the edge. */
         if(!host_edge_index_declared)
         {
            PTFI("int host_edge_index = lookupEdge(morphism, %d);\n", 3, index);
            host_edge_index_declared = true;
         }
         else PTFI("host_edge_index = lookupEdge(morphism, %d);\n", 3, index);
         PTFI("if(record_changes)\n", 3);
         PTFI("{\n", 3);
         PTFI("Edge *edge = getEdge(host, host_edge_index);\n", 6);
         PTFI("/* A hole is created if the edge is not at the right-most index of the array. */\n", 6);
         PTFI("pushRemovedEdge(edge->label, edge->source, edge->target, edge->index,\n", 6);
         PTFI("                edge->index < host->edges.size - 1);\n", 6);
         PTFI("}\n", 3);
         PTFI("removeEdge(host, host_edge_index);\n\n", 3);   
      }
      else
      {
         if(edge->interface->relabelled || edge->interface->remarked)
         {
            if(!host_edge_index_declared)
            {
               PTFI("int host_edge_index = lookupEdge(morphism, %d);\n", 3, index);
               host_edge_index_declared = true;
            }
            else PTFI("host_edge_index = lookupEdge(morphism, %d);\n", 3, index);
            RuleLabel label = edge->interface->label;
            PTFI("HostLabel label_e%d = getEdgeLabel(host, host_edge_index);\n", 3, index);
            if(edge->interface->relabelled)
            {
               /* Generate code to evaluate the RHS label. Note that the code generated
                * here is suitable for an edge relabelling independently of whether it
                * is remarked or not. */
               if(!label_declared) 
               {
                  PTFI("HostLabel label;\n", 3);
                  label_declared = true;
               }
               if(label.length == 0 && label.mark == NONE) PTFI("label = blank_label;\n", 3);
               else generateLabelEvaluationCode(label, false, list_count++, 0, 3);
               PTFI("/* Relabel the edge if its label is not equal to the RHS label. */\n", 3);
               PTFI("if(equalHostLabels(label_e%d, label)) removeHostList(label.list);\n", 3, index);
               PTFI("else\n", 3);
               PTFI("{\n", 3);
               PTFI("if(record_changes) pushRelabelledEdge(host_edge_index, label_e%d);\n",
                    6, index);
               PTFI("relabelEdge(host, host_edge_index, label);\n", 6);
               PTFI("printf(\"Relabelling Edge! :)\\n\");\n", 6); // ~IMP: debug TODO remove
               PTFI("}\n", 3);
            }
            /* The else branch is entered when only the mark needs to change (not the list
             * component of the label). */
            else if(edge->interface->remarked)
            {
               /* Generate code to re-mark the edge. */
               PTFI("if(record_changes) pushRemarkedEdge(host_edge_index, label_e%d.mark);\n",
                    3, index);
               PTFI("changeEdgeMark(host, host_edge_index, %d);\n\n", 3, label.mark);
            }
         }
      }
   }
   /* (2) Delete/relabel nodes. */
   for(index = 0; index < rule->lhs->node_index; index++)
   { 
      RuleNode *node = getRuleNode(rule->lhs, index);
      if(node->interface == NULL) 
      {
         if(!host_node_index_declared)
         {
            PTFI("int host_node_index = lookupNode(morphism, %d);\n", 3, index);
            host_node_index_declared = true;
         }
         else PTFI("host_node_index = lookupNode(morphism, %d);\n", 3, index);
         /* Generate code to remove the node. */
         PTFI("if(record_changes)\n", 3);
         PTFI("{\n", 3);
         PTFI("Node *node = getNode(host, host_node_index);\n", 6);
         PTFI("/* A hole is created if the node is not at the right-most index of the array. */\n", 6);
         PTFI("pushRemovedNode(node->root, node->label, node->index,\n", 6);
         PTFI("                node->index < host->nodes.size - 1);\n", 6);  
         PTFI("}\n", 3);
         PTFI("removeNode(host, host_node_index);\n\n", 3);   
      }
      else
      {
         RuleNode *rhs_node = node->interface;
         if(rhs_node->relabelled || rhs_node->remarked)
         {
            if(!host_node_index_declared)
            {
               PTFI("int host_node_index = lookupNode(morphism, %d);\n", 3, index);
               host_node_index_declared = true;
            }
            else PTFI("host_node_index = lookupNode(morphism, %d);\n", 3, index);
            RuleLabel label = rhs_node->label;
            PTFI("HostLabel label_n%d = getNodeLabel(host, host_node_index);\n", 3, index);
            if(rhs_node->relabelled)
            {
               /* Generate code to evaluate the RHS label. Note that the code generated
                * here is suitable for a node relabelling independently of whether it
                * is re-marked or not. */
               if(!label_declared) 
               {
                  PTFI("HostLabel label;\n", 3);
                  label_declared = true;
               }
               if(label.length == 0 && label.mark == NONE) PTFI("label = blank_label;\n", 3);
               else generateLabelEvaluationCode(label, true, list_count++, 0, 3);
               
               /* If the two labels are equal, no relabelling needs to be done. */
               PTFI("if(equalHostLabels(label_n%d, label)) removeHostList(label.list);\n", 3, index);
               PTFI("else\n", 3);
               PTFI("{\n", 3);
               PTFI("if(record_changes) pushRelabelledNode(host_node_index, label_n%d);\n",
                    6, index);
               PTFI("printf(\"Relabelling Node! :)\\n\");\n", 6); // ~IMP: debug TODO remove
               PTFI("relabelNode(host, host_node_index, label);\n", 6);
               PTFI("}\n", 3);
            }
            /* The else branch is entered when only the mark needs to change (not the list
             * component of the label). */
            else if(rhs_node->remarked)
            {
               /* Generate code to re-mark the edge. */
               PTFI("if(record_changes) pushRemarkedNode(host_node_index, label_n%d.mark);\n",
                    3, index);
               PTFI("changeNodeMark(host, host_node_index, %d);\n\n", 3, label.mark);
            }
         }
         if(rhs_node->root_changed)
         {
            if(!host_node_index_declared)
            {
               PTFI("int host_node_index = lookupNode(morphism, %d);\n", 3, index);
               host_node_index_declared = true;
            }
            else PTFI("host_node_index = lookupNode(morphism, %d);\n", 3, index);
            /* The root is changed in two cases:
             * (1) The LHS node is rooted and the RHS node is non-rooted.
             * (2) The LHS node is non-rooted, the RHS node is rooted, and
             *     the matched host node is non-rooted. */

            /* Case (1) */
            if(node->root && !node->interface->root) 
            {
               PTFI("if(record_changes) pushChangedRootNode(host_node_index);\n", 3);
               PTFI("changeRoot(host, host_node_index);\n", 3);
            }
            /* Case (2) */
            if(!node->root && node->interface->root) 
            {
               PTFI("Node *node%d = getNode(host, host_node_index);\n", 3, index);
               PTFI("if(!node%d->root)\n", 3, index);
               PTFI("{\n", 3);
               PTFI("if(record_changes) pushChangedRootNode(host_node_index);\n", 6);
               PTFI("changeRoot(host, host_node_index);\n", 6);
               PTFI("}\n", 3);
            }
         }
      }
   }
   /* If both nodes and edges are added by the rule, the host indices of the 
    * added nodes need to be recorded in case the added edges require them. */
   if(rule->adds_nodes && rule->adds_edges)
   {
      PTFI("/* Array of host node indices indexed by RHS node index. */\n", 3);
      PTFI("int rhs_node_map[%d];\n\n", 3, rule->rhs->node_index);
   }
   /* (3) Add nodes. */
   PTFI("Highlight node_additions[%d];\n", 3, rule->rhs->node_index);
   for(index = 0; index < rule->rhs->node_index; index++)
   { 
      RuleNode *node = getRuleNode(rule->rhs, index);
      if(node->interface != NULL) continue;
      if(!host_node_index_declared)
      {
         PTFI("int host_node_index;\n", 3);
         host_node_index_declared = true;
      }
      PTFI("int node_array_size%d = host->nodes.size;\n", 3, index);
      if(node->label.length == 0 && node->label.mark == NONE)
         PTFI("host_node_index = addNode(host, %d, blank_label);\n", 3, node->root);
      else
      {
         if(!label_declared) 
         {
            PTFI("HostLabel label;\n", 3);
            label_declared = true;
         }
         generateLabelEvaluationCode(node->label, true, list_count++, 0, 3);
         PTFI("host_node_index = addNode(host, %d, label);\n", 3, node->root);
      }
      // ~IMP1
      PTFI("added_node_count ++;\n", 3);
      PTFI("node_additions[%d].host_index = host_node_index;\n", 3, index);
      PTFI("node_additions[%d].highlight = \"%s\";\n", 3, index, ADDED_NODE);
      PTFI("printf(\"Adding Node! :)\\n\");\n", 6); // ~IMP: debug TODO remove
      if(rule->adds_edges) PTFI("rhs_node_map[%d] = host_node_index;\n", 3, node->index);
      PTFI("/* If the node array size has not increased after the node addition, then\n", 3);
      PTFI("   the node was added to a hole in the array. */\n", 3);
      PTFI("if(record_changes)\n", 3);
      PTFI("pushAddedNode(host_node_index, node_array_size%d == host->nodes.size);\n", 6, index);
   }
   PTFI("node_highlights = node_additions;\n", 3);
   /* (4) Add edges. */
   PTFI("Highlight edge_additions[%d];\n", 3, rule->rhs->edge_index);
   bool source_target_declared = false;
   for(index = 0; index < rule->rhs->edge_index; index++)
   { 
      RuleEdge *edge = getRuleEdge(rule->rhs, index);
      if(edge->interface != NULL) continue;
      if(!host_edge_index_declared)
      {
         PTFI("int host_edge_index;\n", 3);
         host_edge_index_declared = true;
      }
      if(!source_target_declared)
      {
         PTFI("int source, target;\n", 3);
         source_target_declared = true;
      }
      PTFI("int edge_array_size%d = host->edges.size;\n", 3, index);
      /* The source and target edges are either nodes preserved by the rule or 
       * nodes added by the rule. 
       * The host node indices of preserved nodes are obtained from the morphism.
       * The host node indices of added nodes are obtained from rhs_node_map. */
      if(edge->source->interface != NULL)
           PTFI("source = lookupNode(morphism, %d);\n", 3, edge->source->interface->index);
      else PTFI("source = rhs_node_map[%d];\n", 3, edge->source->index);

      if(edge->target->interface != NULL)
           PTFI("target = lookupNode(morphism, %d);\n", 3, edge->target->interface->index);
      else PTFI("target = rhs_node_map[%d];\n", 3, edge->target->index);

      if(edge->label.length == 0 && edge->label.mark == NONE)
         PTFI("host_edge_index = addEdge(host, blank_label, source, target);\n", 3);
      else
      {
         if(!label_declared) 
         {
            PTFI("HostLabel label;\n", 3);
            label_declared = true;
         }
         generateLabelEvaluationCode(edge->label, false, list_count++, 0, 3);
         PTFI("host_edge_index = addEdge(host, label, source, target);\n", 3);
      }
      // ~IMP1
      PTFI("added_edge_count ++;\n", 3);
      PTFI("edge_additions[%d].host_index = host_edge_index;\n", 3, index);
      PTFI("edge_additions[%d].highlight = \"%s\";\n", 3, index, ADDED_EDGE);
      PTFI("printf(\"Adding Edge! :)\\n\");\n", 6); // ~IMP: debug TODO remove
      PTFI("/* If the edge array size has not increased after the edge addition, then\n", 3);
      PTFI("   the edge was added to a hole in the array. */\n", 3);
      PTFI("if(record_changes)\n", 3);
      PTFI("pushAddedEdge(host_edge_index, edge_array_size%d == host->edges.size);\n", 6, index);
   }
   PTFI("edge_highlights = edge_additions;\n", 3);
   PTFI("/* Reset the morphism. */\n", 3);
   PTFI("initialiseMorphism(morphism, host);\n}\n\n", 3);
}

