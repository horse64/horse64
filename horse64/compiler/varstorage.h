#ifndef HORSE64_COMPILER_VARSTORAGE_H_
#define HORSE64_COMPILER_VARSTORAGE_H_

typedef struct h64compileproject h64compileproject;
typedef struct h64ast h64ast;

int varstorage_AssignLocalStorage(
    h64compileproject *pr, h64ast *ast
);

typedef struct h64storageextrainfo {
    int temps_for_locals_startindex;

    int closureboundvars_count;
    h64scopedef **closureboundvars;
    int *externalclosurevar_valuetempid;
} h64storageextrainfo;

void varstorage_FreeExtraInfo(
    h64storageextrainfo *einfo
);

#endif  // HORSE64_COMPILER_VARSTORAGE_H_