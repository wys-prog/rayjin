#ifndef RAYJIN_CONFIG_H
#define RAYJIN_CONFIG_H

#define RAYJIN_VERSION 1
#define RAYJIN_PATCH   0

#include <raylib/src/raylib.h>
#include <lua/lua.h>

#define RAYJIN_LUA_VERSION    LUA_VERSION_RELEASE_NUM
#define RAYJIN_RAYLIB_VERSION RAYLIB_VERSION
#define RAYJIN_CXX_VERSION    __cplusplus
#define RAYJIN_CC_VERSION     __STDC__

#endif // RAYJIN_CONFIG_H