// return_anon_structs.c
//
// Created 2026/03/12 14:14:44 PDT

// Shows how to use anonymous structs to return more than one value from a function

#include <locale.h>
#include <stdio.h>
#include <string.h>

static struct { int x; }
whatever (char const *,
          unsigned long)
{
    return (typeof (whatever(nullptr, 0))){
        __LINE__
    };
}

static inline struct {
    char const string[
        sizeof __TIMESTAMP__
    ];
} build_time () {
    return (typeof(build_time())){
        __TIMESTAMP__
   };
}
static inline struct {
    char msg[256];
} oof (int         err,
       int         lin,
       char const *msg,
       char const *fil,
       char const *fun)
{
    typeof (oof(0,0,"","","")) r = {};
    (void)snprintf(r.msg, sizeof r.msg,
                   "%s:%d:%s: %s: %s",
                   fil, lin, fun, msg,
                   strerror(err));
    return r;
}
#define oof(e, m)          \
((oof)((e),__LINE__,(m), \
__FILE__,__func__))


int main ()
{
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // use user's system locale

    puts(build_time().string);
    printf("%d\n", whatever("oof", 7).x);
    puts(oof(ENOMEDIUM, "Can't talk to ghosts").msg);
}
