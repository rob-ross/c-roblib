#ifndef ROBLIB_VERSION_H
#define ROBLIB_VERSION_H

// The overall library version
#define ROBLIB_VERSION_MAJOR 0
#define ROBLIB_VERSION_MINOR 1
#define ROBLIB_VERSION_PATCH 0

#define ROBLIB_STR(x) #x
#define ROBLIB_VERSION_STR(a, b, c) ROBLIB_STR(a) "." ROBLIB_STR(b) "." ROBLIB_STR(c)

#define ROBLIB_VERSION ROBLIB_VERSION_STR(ROBLIB_VERSION_MAJOR, ROBLIB_VERSION_MINOR, ROBLIB_VERSION_PATCH)

/**
 * Component Maturity Levels
 * You can define specific versions for mature components here.
 */
#define ROBLIB_JSONP_VERSION_MAJOR 0
#define ROBLIB_JSONP_VERSION_MINOR 1
#define ROBLIB_JSONP_VERSION_PATCH 0

#define ROBLIB_BITSET_VERSION_MAJOR 0
#define ROBLIB_BITSET_VERSION_MINOR 0
#define ROBLIB_BITSET_VERSION_PATCH 1

const char* roblib_version(void);

#endif // ROBLIB_VERSION_H
