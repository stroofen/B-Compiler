#ifndef WALK_NODES_H
#define WALK_NODES_H

typedef struct ast_node_s ast_node_t;
typedef void(*visit_fn_t)(ast_node_t*, void*);

void walkNodes(ast_node_t* node, void* ctx, visit_fn_t preVisit, visit_fn_t postVisit);

#endif