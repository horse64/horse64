// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HORSE64_COMPILER_VARSTORAGE_H_
#define HORSE64_COMPILER_VARSTORAGE_H_

#include "compileconfig.h"

#include <stdint.h>

typedef struct h64compileproject h64compileproject;
typedef struct h64ast h64ast;
typedef struct jsonvalue jsonvalue;
typedef struct h64scopedef h64scopedef;
typedef struct h64expression h64expression;

int varstorage_AssignLocalStorage(
    h64compileproject *pr, h64ast *ast
);

typedef struct h64localstorageassign {
    int valuetemporaryid;  // temporary to store actual value
    h64scopedef *vardef;  // the definition for this variable

    int ever_used_nonclosure, ever_used_closure;
} h64localstorageassign;

struct h64codegenstorageinfo {
    int max_extra_stack;

    int extra_temps_count;
    int *extra_temps_used, *extra_temps_deletepastline;
};

typedef struct h64funcstorageextrainfo {
    int lowest_guaranteed_free_temp;
    int temp_calculation_slots;

    struct h64codegenstorageinfo codegen;

    int32_t jump_targets_used;
    int16_t dostmts_used;

    int closure_with_self;
    int closureboundvars_count;
    h64scopedef **closureboundvars;

    int lstoreassign_was_computed;
    int lstoreassign_count;
    h64localstorageassign *lstoreassign;
} h64funcstorageextrainfo;

void varstorage_FreeExtraInfo(
    h64funcstorageextrainfo *einfo
);

jsonvalue *varstorage_ExtraInfoToJSON(
    h64funcstorageextrainfo *einfo
);

jsonvalue *varstorage_StorageAsJSON(h64expression *e);

#endif  // HORSE64_COMPILER_VARSTORAGE_H_
