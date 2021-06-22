//
// Created by benpeng.jiang on 2021/6/21.
//

#include "ecma-bignumber.h"
inline bf_t *JS_GetBigInt(JSValueConst val)
{
    JSBigFloat *p = JS_VALUE_GET_PTR(val);
    return &p->num;
}