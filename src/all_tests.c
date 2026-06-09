// all_tests.c
//
// Copyright (c) Rob Ross 2026.
//
//
// Created 2026/03/07 20:41:22 PST

//
// Created by Rob Ross on 3/7/26.
//

#include <getopt.h>

int main_test_hashmap(int argc, char *argv[argc + 1]);
int main_test_string_counter(int argc, char *argv[argc + 1]) ;
int main_test_array_list(int argc, char *argv[argc + 1]) ;

int main(int argc, char *argv[argc + 1]) {
    main_test_hashmap(argc, argv);
    optind = 1;
    main_test_string_counter(argc, argv);
    optind = 1;
    main_test_array_list(argc, argv);
    optind = 1;
}