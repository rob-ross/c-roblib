// error_result.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 18:07:42 PDT

#include "roblib/error_result.h"

#include <stdio.h>

void err_print(Error err) {
    if (!err.err) return;
    fprintf(stderr,"Error code: %d, msg: %s", err.reported_err, err.msg);
}
