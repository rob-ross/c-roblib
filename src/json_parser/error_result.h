// error_result.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 18:11:09 PDT
#pragma once

#ifndef CS50X_ERROR_RESULT_H
#define CS50X_ERROR_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif



/*
 * The inital version of our Glorious Error reporting technique!!!
 * Standardized error reporting. Error records will be the same for all functions.
 * .err a bool is true if error occurred or false if error did not occur
 *
 * To start with, an error code reported_err (int) and an optional error message string.
 * This will make it simple to initialize an err struct in the success case with an empty initializer.
 *
 * Caller and callee can interpret other values as needed.
 * Error checking idiom will be of the form:
 *
 * ErrResult er = foo_fucntion();
 * if ( er.err) {
 *      // respond to error
 *      // look at reported_err, msg
 * } else { // do happy path }
 *
 *
 * `err_obj` will be nullptr for most cases. But if an underlying function returns a more detailed error reporting object
 * like an err struct, this pointer will hold that object.
 *
 * Let's see how well this works in practice!!!

*/

// other functions can create err structures as needed to wrap other data types or structures they return.



#define ERROR_FIELDS \
bool err; \
int reported_err; \
char msg[1024]; \
void* err_obj;

typedef struct error_fields_s {
    ERROR_FIELDS
} ErrorFields;

#define ERROR_BASE \
union { \
ErrorFields error; \
struct {ERROR_FIELDS}; \
}

typedef ERROR_BASE Error ;

#endif //CS50X_ERROR_RESULT_H


void err_print(Error err);

#ifdef __cplusplus
}
#endif