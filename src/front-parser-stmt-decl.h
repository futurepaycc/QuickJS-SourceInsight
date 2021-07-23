//
// Created by benpeng.jiang on 2021/6/21.
//

#ifndef LOX_JS_FRONT_PARSER_STMT_DECL_H
#define LOX_JS_FRONT_PARSER_STMT_DECL_H

#include "qjs-front.h"

#define DECL_MASK_FUNC  (1 << 0) /* allow normal function declaration */
/* ored with DECL_MASK_FUNC if function declarations are allowed with a label */
#define DECL_MASK_FUNC_WITH_LABEL (1 << 1)
#define DECL_MASK_OTHER (1 << 2) /* all other declarations */
#define DECL_MASK_ALL   (DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER)

__exception int js_parse_statement_or_decl(JSParseState *s,
                                           int decl_mask);

__exception int js_parse_source_element(JSParseState *s);


int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                    BOOL is_local, BOOL is_arg,
                    int var_idx, JSAtom var_name,
                    BOOL is_const, BOOL is_lexical,
                    JSVarKindEnum var_kind);

__exception int js_parse_var(JSParseState *s, int parse_flags, int tok,
                             BOOL export_flag);

JSExportEntry *add_export_entry2(JSContext *ctx,
                                 JSParseState *s, JSModuleDef *m,
                                 JSAtom local_name, JSAtom export_name,
                                 JSExportTypeEnum export_type);

JSExportEntry *find_export_entry(JSContext *ctx, JSModuleDef *m,
                                 JSAtom export_name);

int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);

int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);

int resolve_scope_private_field1(JSContext *ctx,
                                 BOOL *pis_ref, int *pvar_kind,
                                 JSFunctionDef *s,
                                 JSAtom var_name, int scope_level);

int resolve_scope_private_field(JSContext *ctx, JSFunctionDef *s,
                                JSAtom var_name, int scope_level, int op,
                                DynBuf *bc);

JSFunctionDef *js_new_function_def(JSContext *ctx,
                                   JSFunctionDef *parent,
                                   BOOL is_eval,
                                   BOOL is_func_expr,
                                   const char *filename, int line_num);

__exception int js_parse_program(JSParseState *s);

int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd);
#endif //LOX_JS_FRONT_PARSER_STMT_DECL_H
