// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HORSE64_COMPILER_ASTPARSER_H_
#define HORSE64_COMPILER_ASTPARSER_H_

#include "compileconfig.h"

#include <string.h>

#include "compiler/ast.h"
#include "compiler/lexer.h"
#include "compiler/operator.h"
#include "compiler/scope.h"
#include "widechar.h"

typedef struct h64compileproject h64compileproject;
typedef struct poolalloc poolalloc;

typedef struct h64ast {
    int global_storage_built, identifiers_resolved, threadable_map_done;
    h64wchar *fileuri; int64_t fileurilen;
    char *module_path, *library_name;
    h64result resultmsg;
    h64scope scope;
    int stmt_count;
    h64expression **stmt;
    int basic_file_access_was_successful;

    poolalloc *ast_expr_alloc;
} h64ast;

typedef struct tsinfo {
    h64token *token;
    int token_count;
} tsinfo;

typedef struct h64parsecontext {
    h64compileproject *project;
    h64result *resultmsg;
    h64scope *global_scope;
    const h64wchar *fileuri;
    int64_t fileurilen;
    tsinfo *tokenstreaminfo;
    h64ast *ast;
} h64parsecontext;

typedef struct h64parsethis {
    h64scope *scope;
    h64token *tokens;
    int max_tokens_touse;
} h64parsethis;

ATTR_UNUSED static h64parsethis *newparsethis(
        h64parsethis *_buf, h64parsethis *previous,
        h64token *tokens, int max_tokens_touse
        ) {
    memcpy(_buf, previous, sizeof(*previous));
    _buf->tokens = tokens;
    _buf->max_tokens_touse = max_tokens_touse;
    return _buf;
}

ATTR_UNUSED static h64parsethis *newparsethis_newscope(
        h64parsethis *_buf, h64parsethis *previous,
        h64scope *scope,
        h64token *tokens, int max_tokens_touse
        ) {
    memcpy(_buf, previous, sizeof(*previous));
    _buf->scope = scope;
    _buf->tokens = tokens;
    _buf->max_tokens_touse = max_tokens_touse;
    return _buf;
}

h64expression *ast_AllocExpr(h64ast *ast);

void ast_FreeContents(h64ast *ast);


#define INLINEMODE_NONGREEDY 0
#define INLINEMODE_GREEDY 1

int ast_ParseExprInline(
    h64parsecontext *context,
    h64parsethis *parsethis,
    int inlinemode,
    int *parsefail,
    int *outofmemory,
    h64expression **out_expr,
    int *out_tokenlen,
    int nestingdepth
);

#define STATEMENTMODE_TOPLEVEL 0
#define STATEMENTMODE_INFUNC 1
#define STATEMENTMODE_INCLASS 2
#define STATEMENTMODE_INCLASSFUNC 3

int ast_ParseExprStmt(
    h64parsecontext *context,
    h64parsethis *parsethis,
    int statementmode,
    int *parsefail,
    int *outofmemory,
    h64expression **out_expr,
    int *out_tokenlen,
    int nestingdepth
);

int ast_ParseCodeBlock(
    h64parsecontext *context,
    h64parsethis *parsethis,
    int statementmode,
    h64expression ***stmt_ptr,
    int *stmt_count_ptr,
    int *parsefail,
    int *outofmemory,
    int *out_tokenlen,
    int nestingdepth
);

h64ast* ast_ParseFromTokens(
    h64compileproject *project,
    const h64wchar *fileuri, int64_t fileurilen,
    h64token *tokens, int token_count
);

int ast_CanBeLValue(h64expression *e);

#endif  // HORSE64_COMPILER_ASTPARSER_H_
