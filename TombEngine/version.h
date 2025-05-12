#pragma once

#define TEN_MAJOR_VERSION   1
#define TEN_MINOR_VERSION   8
#define TEN_BUILD_NUMBER    2
#define TEN_REVISION_NUMBER 0

#define TEST_BUILD 1

#define TOSTR(x) #x
#define MAKE_VERSION_STRING(major, minor, build, revision) TOSTR(major) "." TOSTR(minor) "." TOSTR(build) "." TOSTR(revision)
#define TEN_VERSION_STRING MAKE_VERSION_STRING(TEN_MAJOR_VERSION, TEN_MINOR_VERSION, TEN_BUILD_NUMBER, TEN_REVISION_NUMBER)
