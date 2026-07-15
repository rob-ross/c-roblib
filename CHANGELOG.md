# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
