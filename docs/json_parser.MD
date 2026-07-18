# Docs and examples for json_parser


## Initialization

Before using json_parser for the first time,  you must initialize it by calling

```c
#include "roblib/json_parser.h"

Error init_err = jsonp_init();
if (init_err.err) {
    err_print(init_err);
    jsonp_destroy();
    return;
}
```

When done using json_parser, call

`jsonp_destroy();`

## Parsing

To parse a JSON text as a C-string use:

`JsonValue *jsonp_parse(const char *json_text, JsonParseError *error, Arena *arena)`

json_parser expects you to provide a `Arena` and `JsonErr` objects.

Create and initialize a new `Arena` as:
```c
Arena arena = {};
ArenaErrResult aer = arena_create_arena( &arena, 1024 * 124);  // initally 1MB as an example. Grows as needed.
if ( aer.err ) {
    printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
    jsonp_destroy();
    return;
}
```

json_parser allocates all its memory in the `Arena` you provide. You own its lifecycle. When you destroy the `Arena`, 
the parsed JSON object graph is also destroyed. If you need long-term access to the parsed JSON data, either copy it to your own
memory store or keep a reference to the `Arena`. Note that even if the `Arena` object goes out of scope, it is not destroyed
nor is its memory released until you call `arena_destroy_arena(&arena)`.

You also pass an empty `JsonParseError` object. If a parsing error occurs, information about the error will be recorded in this object. 

`JsonParseError err = {};`

Now that you have your `Arena` and `JsonParseError` objects, you can call `jsonp_parse()`, passing the JSON text as a C-string:
```c
JsonValue *jval = jsonp_parse(json_text, &err, &arena);
if (!jval) {
    // handle error
    printf("parse error: %d\n", err.err_type);
} else {
    jsonp_print_json_value(jval);
}
```

When you are done with the json_parser, clean up resources:
```c
arena_destroy_arena(&arena);
jsonp_destroy();
```
Remember, when you destroy the `Arena` you lose any parsed JSON data. 

### Complete simple parsing example

```c
#include "roblib/json_parser.h"

void simple_parse(char const *json_text) {

    Error init_err = jsonp_init();
    if (init_err.err) {
        err_print(init_err);
        jsonp_destroy();
        return;
    }

    Arena arena = {};
    ArenaErrResult aer = arena_create_arena( &arena, 1024 * 124);  // initially 1MB as an example. Grows as needed.
    if ( aer.err ) {
        printf("arena_create_arena failed with %d, %s\n", aer.reported_err, aer.msg);
        jsonp_destroy();
        return;
    }

    JsonParseError err = {};

    JsonValue *jval = jsonp_parse(json_text, &err, &arena);
    if (!jval) {
        // handle error
        jsonp_print_parse_error(&err);
    } else {
        jsonp_print_json_value(jval);
        putchar('\n');
    }

    arena_destroy_arena(&arena);
    jsonp_destroy();
}

int main(void) {
    printf("Output:\n");
    simple_parse("1");
    simple_parse("[\"string\", -2, 3.2, -4e-2, true, false, null ]");
    simple_parse(
        "{ \"nested array\": [ \"item 1\", \"item 2\", \"item 3\" ], "
                  "\"nested object\": { \"key 1\": \"value 1\", \"key 2\": \"value 2\", \"key 3\": \"value 3\" } }");

    // bad JSON text
    printf("\nfailing parses:\n");

    simple_parse("\"unterminated string ");
    simple_parse("[ \"unterminated array\", 2 ");
    simple_parse("[ \"trailing comma\", 2,] ");
}
```
Produces the following output:
```text
Output:
1
[ 'string', -2, 3.2, -0.04, true, false, null ]
{ 'nested array' : [ 'item 1', 'item 2', 'item 3' ], 'nested object' : { 'key 1' : 'value 1', 'key 2' : 'value 2', 'key 3' : 'value 3' } }

failing parses:
7:UNTERMINATED_STRING  line:1 col:22 pos:21 start:0 end:21 :  missing closing quote '"' 
"unterminated string ^^
6:UNTERMINATED_ARRAY  line:1 col:27 pos:26 start:24 end:26 :  missing closing bracket ']'
[ "unterminated array", 2 ^^
26:TRAILING_COMMA_NOT_ALLOWED  line:1 col:23 pos:21 start:21 end:22 :  trailing comma not allowed in array element list
[ "trailing comma", 2^,^] 
```
