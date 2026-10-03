// error_result.h
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/06/08 18:11:09 PDT
#pragma once

#ifndef ROBLIB__ERROR_RESULT_H
#define ROBLIB__ERROR_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif



/*
 *v2 notes
```c
typedef struct ErrorFields {
    bool err;
    int reported_err;
    char msg[1024];
    void* err_obj;
} ErrorFields;

#define ERR_FIELDS_UNION         \
    union {                      \
        struct {                 \
            bool err;            \
            int reported_err;    \
            char msg[1024];      \
            void* err_obj;       \
        };                       \
        ErrorFields err_fields;  \
    }

typedef struct error_s {
    ERR_FIELDS_UNION;
} Error;

// This is like a base class for derived classes, where the `result` member is specifically typed to whatever the
// function returns in the happy path. It's here to allow casting to a common struct type, in case its useful.
typedef struct error_result_s {
    ERR_FIELDS_UNION;
    void * result;
} ErrorResult;

```

My C functions that could return an error or a result if there's no error return an ErrorResult struct. Specific functions would pseudo-override this with a definition that types the result pointer to the specific function. The struct is designed to be easy to use by caller and callee alike.

In general I will have
```c
ErrorResult do_something_and_return_result(...);
...

ErrorResult r = do_something_and_return_result();
if (r.err) { // handle error }
else ( // do something with r.result }
```

A concrete example, I have a function
`PoolErrResult alok_pool_create( const size_t num_objects, const size_t object_size, const size_t object_alignment );`

That tries to allocate a memory pool.
```c
typedef struct pool_err_result_s {
    ERR_FIELDS_UNION;
    AlokPool * result;
} PoolErrResult;
...
PoolErrResult r = alok_pool_create(...):
if (r.err) { // handle pool allocation failed}
else { // work with the object pool in *result }
```

If there *is* an error, you can query the `.reported_err member` , an int. This handles all cases where an internal function call results in some int error code. This preserves that underlying error code if you need to specifically see what the error was, like `ENOMEM`.  I try to create the error message for the user as close to the actual error as possible to provide the best context. So I will capture that message in the `.msg` field. `.err_obj` provides a pointer to attach a more complex error reporting object if needed.

I wanted a simple idiom so it wasn't a big speedbump to use by callers to check the return result for errors.
Also, this makes it easy for the functions returning this struct. In the case with no errors a function just runs:
`return (PoolErrResult){ .err = false, .result =  pool };`
Technically I could simplify this to
`return (PoolErrResult){ .result =  pool }; // pool is the pointer to the returned pool object`
since the default value for .err will then be 0, i.e. false.

If returning an error, I do something like
```c
    ...
    raw_mem = mmap(nullptr, page_aligned_block_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, MACH_NO_FLAGS, 0);
    if (raw_mem == MAP_FAILED) {
        int err_no = errno;
        BlockHeaderErrResult bher =  (BlockHeaderErrResult){ .err = true, .reported_err = err_no,  };
        strerror_r(err_no, bher.msg, sizeof bher.msg);
        return bher;
```




 *
 *
 *
 *
 * v1 notes
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
 * `err_obj` will be nullptr for most cases. But if an underlying function returns a more detailed error-reporting object
 * like an err struct, this pointer will hold that object.
 *
 * Let's see how well this works in practice!!!

*/

// other functions can create ErrorResult structures as needed to wrap other data types or structures they return.



typedef struct ErrorFields {
    bool err;
    int reported_err;
    char msg[1024];
    void* err_obj;
} ErrorFields;

#define ERR_FIELDS_UNION         \
    union {                      \
        struct {                 \
            bool err;            \
            int reported_err;    \
            char msg[1024];      \
            void* err_obj;       \
        };                       \
        ErrorFields err_fields;  \
    }

typedef struct error_s {
    ERR_FIELDS_UNION;
} Error;

// This is like a base class for derived classes, where the `result` member is specifically typed to whatever the
// function returns in the happy path. It's here to allow casting to a common struct type, in case its useful.
typedef struct error_result_s {
    ERR_FIELDS_UNION;
    void * result;
} ErrorResult;


void err_print(Error err);





// -----------------------------------------------------------------
//      ORIGINAL MACROS, DON'T DELETE
// -----------------------------------------------------------------
/*
#define ERROR_FIELDS_OLD \
    bool err; \
    int reported_err; \
    char msg[1024]; \
    void* err_obj;

typedef struct error_fields_s_OLD {
    ERROR_FIELDS_OLD
} ErrorFields_OLD;

#define ERROR_BASE_OLD \
    union { \
        ErrorFields_OLD error; \
        struct {ERROR_FIELDS_OLD}; \
}

typedef ERROR_BASE_OLD Error_OLD ;
*/


#ifdef __cplusplus
}
#endif

#endif //ROBLIB__ERROR_RESULT_H
