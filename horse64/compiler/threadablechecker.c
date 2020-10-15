// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#include "compileconfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "bytecode.h"
#include "compiler/ast.h"
#include "compiler/asthelpers.h"
#include "compiler/astparser.h"
#include "compiler/asttransform.h"
#include "compiler/compileproject.h"
#include "compiler/operator.h"
#include "compiler/threadablechecker.h"
#include "hash.h"

static int _func_get_nodeinfo_by_id(
        h64threadablecheck_graph *graph,
        funcid_t funcid,
        h64threadablecheck_nodeinfo **nodeinfo
        ) {
    assert(funcid >= 0);
    *nodeinfo = NULL;
    {
        uint64_t queryresult = 0;
        if (hash_IntMapGet(
                graph->func_id_to_nodeinfo,
                (int64_t)funcid, &queryresult
                )) {
            assert(queryresult != 0);
            *nodeinfo = (void*)(uintptr_t)queryresult;
        }
    }
    if (!*nodeinfo) {
        *nodeinfo = malloc(sizeof(**nodeinfo));
        if (!*nodeinfo)
            return 0;
        if (!hash_IntMapSet(
                graph->func_id_to_nodeinfo,
                (int64_t)funcid, (uintptr_t)*nodeinfo
                )) {
            free(*nodeinfo);
            return 0;
        }
        memset(*nodeinfo, 0, sizeof(**nodeinfo));
        (*nodeinfo)->associated_func_id = funcid;
    }
    return 1;
}

static int _func_get_nodeinfo_by_expr(
        h64threadablecheck_graph *graph,
        h64expression *expr,
        h64threadablecheck_nodeinfo **nodeinfo
        ) {
    int64_t bytecode_id = (int64_t)(
        expr->funcdef.bytecode_func_id
    );
    assert(bytecode_id >= 0);
    return _func_get_nodeinfo_by_id(
        graph, bytecode_id, nodeinfo
    );
}

int _threadablechecker_register_visitin(
        h64expression *expr, ATTR_UNUSED h64expression *parent,
        void *ud
        ) {
    asttransforminfo *rinfo = (asttransforminfo *)ud;
    h64threadablecheck_graph *graph = (
        rinfo->pr->threadable_graph
    );

    if (expr->type == H64EXPRTYPE_FUNCDEF_STMT ||
            expr->type == H64EXPRTYPE_INLINEFUNCDEF) {
        h64threadablecheck_nodeinfo *nodeinfo = NULL;
        if (!_func_get_nodeinfo_by_expr(
                graph, expr, &nodeinfo
                )) {
            rinfo->hadoutofmemory = 1;
            return 0;
        }
        assert(nodeinfo != NULL);
    } else if (expr->type == H64EXPRTYPE_IDENTIFIERREF) {
        h64expression *func = surroundingfunc(expr);
        if (!func)
            return 1;
        h64threadablecheck_nodeinfo *nodeinfo = NULL;
        if (!_func_get_nodeinfo_by_expr(
                graph, expr, &nodeinfo
                )) {
            rinfo->hadoutofmemory = 1;
            return 0;
        }
        funcid_t func_id = func->funcdef.bytecode_func_id;
        assert(func_id >= 0);
        if (expr->storage.set && expr->storage.ref.type ==
                H64EXPRTYPE_FUNCDEF_STMT) {
            h64threadablecheck_calledfuncinfo *
                called_func_info_new = realloc(
                    nodeinfo->called_func_info,
                    sizeof(*called_func_info_new) *
                    (nodeinfo->called_func_count + 1)
                );
            if (!called_func_info_new) {
                rinfo->hadoutofmemory = 1;
                return 0;
            }
            nodeinfo->called_func_info = called_func_info_new;
            h64threadablecheck_calledfuncinfo *ninfo = (
                &nodeinfo->called_func_info[
                    nodeinfo->called_func_count
                ]);
            memset(ninfo, 0, sizeof(*ninfo));
            ninfo->func_id = expr->storage.ref.id;
            ninfo->line = expr->line;
            ninfo->column = expr->column;
            nodeinfo->called_func_count++;
        } else if (expr->storage.set &&
                expr->storage.ref.type ==
                    H64STORETYPE_GLOBALVARSLOT) {
            h64globalvarsymbol *gvsymbol = (
                h64debugsymbols_GetGlobalvarSymbolById(
                    rinfo->pr->program->symbols,
                    expr->storage.ref.id
                ));
            if ((!gvsymbol->is_const ||
                    !gvsymbol->is_simple_const) &&
                    rinfo->pr->program->func[func_id].
                        user_set_canasync) {
                if (!result_AddMessage(
                        rinfo->pr->resultmsg, H64MSG_ERROR,
                        "func marked as \"canasync\" cannot access "
                        "global variable that isn't a simple "
                        "constant", NULL,
                        (expr->identifierref.
                            resolved_to_expr != NULL ?
                        expr->identifierref.
                            resolved_to_expr->line : -1),
                        (expr->identifierref.
                            resolved_to_expr != NULL ?
                        expr->identifierref.
                            resolved_to_expr->column : -1)
                        )) {
                    rinfo->hadoutofmemory = 1;
                    return 0;
                }
            }
            if (!gvsymbol->is_const ||
                    !gvsymbol->is_simple_const)
                rinfo->pr->program->func[func_id].is_threadable = 0;
        } else if (expr->identifierref.resolved_to_expr) {
            // This cannot be resolved to a function, since
            // otherwise storage must have been resolved if
            // we got this far without an error:
            assert(
                (expr->identifierref.resolved_to_expr->type !=
                 H64EXPRTYPE_FUNCDEF_STMT &&
                 expr->identifierref.resolved_to_expr->type !=
                 H64EXPRTYPE_INLINEFUNCDEF) ||
                (func_has_param_with_name(
                    expr->identifierref.resolved_to_expr,
                    expr->identifierref.value
                ))
            );
        }
    }
    return 1;
}

int threadablechecker_RegisterASTForCheck(
        h64compileproject *project, h64ast *ast
        ) {
    if (ast->threadable_map_done)
        return 1;

    if (project->threadable_graph == NULL) {
        project->threadable_graph = malloc(
            sizeof(*project->threadable_graph)
        );
        if (!project->threadable_graph) {
            oom:
            if (!result_AddMessage(
                    project->resultmsg, H64MSG_ERROR,
                    "out of memory during threadable check",
                    NULL, -1, -1
                    )) {
                // Nothing we can do.
            }
            project->resultmsg->success = 0;
            return 0;
        }
        memset(
            project->threadable_graph, 0,
            sizeof(*project->threadable_graph)
        );
        if ((project->threadable_graph->func_id_to_nodeinfo =
                hash_NewIntMap(1024)) == NULL) {
            goto oom;
        }
    }

    int result = asttransform_Apply(
        project, ast,
        &_threadablechecker_register_visitin, NULL, NULL
    );
    if (!result)
        return 0;

    ast->threadable_map_done = 1;
    return 1;
}

int threadablechecker_IterateFinalGraph(
        h64compileproject *project
        ) {
    if (!project->threadable_graph)
        return 1;
    h64threadablecheck_graph *graph = (
        project->threadable_graph
    );
    int success = 1;
    int gotchange = 1;
    while (gotchange) {
        gotchange = 0;
        funcid_t i = 0;
        while (i < project->program->func_count) {
            if (project->program->func[i].is_threadable == 0) {
                i++;
                continue;
            }
            h64threadablecheck_nodeinfo *nodeinfo = NULL;
            if (!_func_get_nodeinfo_by_id(
                    graph, i, &nodeinfo
                    ))
                return 0;
            assert(nodeinfo != NULL);
            int64_t k = 0;
            while (k < nodeinfo->called_func_count) {
                funcid_t f2 = nodeinfo->called_func_info[k].func_id;
                if (f2 != i &&
                        project->program->func[f2].is_threadable == 0) {
                    project->program->func[i].is_threadable = 0;
                    gotchange = 1;
                    if (project->program->func[i].user_set_canasync) {
                        if (!result_AddMessage(
                                project->resultmsg, H64MSG_ERROR,
                                "func marked as \"canasync\" cannot "
                                "access this func "
                                "that is not \"canasync\" itself",
                                NULL,
                                nodeinfo->called_func_info[k].line,
                                nodeinfo->called_func_info[k].column
                                )) {
                            if (!result_AddMessage(
                                    project->resultmsg, H64MSG_ERROR,
                                    "out of memory during threadable "
                                    "check",
                                    NULL, -1, -1
                                    )) {
                                // Nothing we can do.
                            }
                            project->resultmsg->success = 0;
                            success = 0;
                        }
                    }
                }
                k++;
            }
            i++;
        }
    }
    return success;
}

static int _graphfreecb(
        ATTR_UNUSED hashmap *map, ATTR_UNUSED int64_t key,
        uint64_t value, ATTR_UNUSED void *ud
        ) {
    h64threadablecheck_nodeinfo *nodeinfo = (
        (void*)(uintptr_t)value
    );
    if (nodeinfo) {
        free(nodeinfo->called_func_info);
        free(nodeinfo);
    }
    return 1;
}

void threadablechecker_FreeGraphInfoFromProject(
        h64compileproject *project
        ) {
    if (!project)
        return;
    h64threadablecheck_graph *graph = (
        project->threadable_graph
    );
    if (!graph)
        return;
    int iterate_result = hash_IntMapIterate(
        graph->func_id_to_nodeinfo, &_graphfreecb, NULL
    );
    if (!iterate_result) {
        // Ran out of memory in iteration handling.
        // That will cause a leak, but not much we can do about it.
    }
    hash_FreeMap(graph->func_id_to_nodeinfo);
    free(graph);
}