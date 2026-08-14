# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2026-08-14

### Added
- JSON Pretty Printer: `jsonp_sprint`, `jsonp_print`, and `jsonp_fprint`.
- Per-allocation alignment parameter for `alok_arena_alloc`.
- `StringSlice` utilities: `slice_split` (implemented using temporary arenas), `slice_equal_arrays`, `slice_equal_arrays_by_case`, `slice_char_as_cstring`, and `slice_from_char`.
- Linked list manipulation macros in `base.h` (`DLLPushBack`, `DLLRemove`, `SLLQueuePush`, etc.).
- Comprehensive unit tests for `StringSlice` and JSON output functions.
- Enhanced allocator functionality: marking, pop-to-mark, and thread-local scratch arena support.

### Changed
- **BREAKING**: Optimized `JsonValue` union to reduce memory footprint, removing `long long` and `long double` support (struct size reduced from 32 to 24 bytes).
- **BREAKING**: Standardized Allocator module naming. Renamed `Arena` to `AlokArena` and updated function prefixes to `alok_arena_`.
- Removed `StackArena` in favor of new marker-based functionality.
- Refactored `error_result.h` for improved type safety and IDE compatibility.

### Fixed
- Improved logic in `StringSlice` functions: `trim`, `trim_left`, `trim_right`, and `slice_compare`.
  
## [0.1.1] - 2026-07-28

### Changed JSON Parser [0.1.1]
- Major refactoring to move parse state into the JsonContext's Input member.
- Implemented InputSource logic for StringSourceInputContext to parse C-strings
- Added function to set whitespace chars, max depth, and config flags on the JsonContext
- Added look-behind-buffer to Input to keep track of the last 40 chars for error reporting.
- Added more robust error reporting for invalid surrogate pairs.

## [Unreleased]

### Added
- Support for Unicode escape sequences (`\uXXXX`) and extended Unicode (`\UXXXXXXXX`) in JSON strings.
- Parameterized test suites for JSON Strings, Integers, and Floating-point numbers.
- Configuration flags for the JSON parser to allow or disallow trailing commas in arrays and objects.
- `jsonp_parse_ex` for parsing with explicit length constraints.
- Handling for `\0` null-terminator edge cases during number parsing.

### Changed
- Refined the JSON parser error reporting to include specific error types like `JSON_ERR_TRAILING_COMMA_NOT_ALLOWED`.
- Integrated `Arena` allocation for more efficient memory management during parsing.

### Fixed
- Parsing logic for multi-digit numbers followed by specific terminators to prevent unexpected EOF errors.

## [0.1.0] - 2026-07-15

### Added
- Initial implementation of the `roblib` JSON parser. See include/roblib/json_parser.h.
- Support for JSON literals: `null`, `true`, `false`.
- Support for JSON arrays, objects, and strings.
- Integer and Floating-point number parsing support.
- Support for standard escape characters in strings (`\n`, `\r`, `\t`, `\b`, `\f`, `\"`, `\\`, `\/`).
- Support for Unicode escape sequences as UTF-16 surrogate pairs
- Core `Arena` allocator functionality.
- Google Test integration for unit testing.

<!--
## [Guiding Principles]
### Added
For new features.
### Changed
For changes in existing functionality.
### Deprecated
For soon-to-be removed features.
### Removed
For now removed features.
### Fixed
For any bug fixes.
### Security
In case of vulnerabilities.
-->
