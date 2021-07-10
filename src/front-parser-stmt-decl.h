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




#endif //LOX_JS_FRONT_PARSER_STMT_DECL_H
