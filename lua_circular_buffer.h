/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*** @brief Lua circular buffer - time series data store and analysis @file */

#ifndef lua_circular_buffer_h_
#define lua_circular_buffer_h_

#include <lua.h>

extern const char* mozsvc_circular_buffer;
extern const char* mozsvc_circular_buffer_table;

/**
 * Circular buffer library loader
 *
 * @param lua Lua state.
 *
 * @return 1 on success
 *
 */
int luaopen_circular_buffer(lua_State* lua);

#endif
