/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @brief Lua circular buffer implementation @file */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif

#ifdef _MSC_VER
#pragma warning( disable : 4056 )
#pragma warning( disable : 4756 )
#endif

#ifdef LUA_SANDBOX
#include "luasandbox_output.h"
#include "luasandbox_serialize.h"
#endif

#define COLUMN_NAME_SIZE 16
#define UNIT_LABEL_SIZE 8

static const char *mozsvc_circular_buffer = "mozsvc.circular_buffer";
static const char *mozsvc_circular_buffer_table = "circular_buffer";

static const char *column_aggregation_methods[] = { "sum", "min", "max", "none",
  NULL };
static const char *default_unit = "count";

#if defined(LUA_SANDBOX) || defined(_MSC_VER)
static const char *not_a_number = "nan";
#endif

typedef enum {
  AGGREGATION_SUM   = 0,
  AGGREGATION_MIN   = 1,
  AGGREGATION_MAX   = 2,
  AGGREGATION_NONE  = 4,
} COLUMN_AGGREGATION;

typedef enum {
  OUTPUT_CBUF = 0,
  OUTPUT_CBUFD = 1,
} OUTPUT_FORMAT;

typedef struct
{
  char name[COLUMN_NAME_SIZE];
  char unit[UNIT_LABEL_SIZE];
  COLUMN_AGGREGATION aggregation;
} header_info;

typedef struct circular_buffer
{
  time_t        current_time;
  unsigned      seconds_per_row;
  unsigned      current_row;
  unsigned      rows;
  unsigned      columns;
  int           delta;
  OUTPUT_FORMAT format;
  int           ref;

  header_info   *headers;
  double        values[];
} circular_buffer;


static time_t get_start_time(circular_buffer *cb)
{
  return cb->current_time - (cb->seconds_per_row * (cb->rows - 1));
}


static void copy_cleared_row(circular_buffer *cb, double *cleared, size_t rows)
{
  size_t pool = 1;
  size_t ask;

  while (rows > 0) {
    if (rows >= pool) {
      ask = pool;
    } else {
      ask = rows;
    }
    memcpy(cleared + (pool * cb->columns), cleared,
           sizeof(double) * cb->columns * ask);
    rows -= ask;
    pool += ask;
  }
}


static void clear_rows(circular_buffer *cb, unsigned num_rows)
{
  if (num_rows >= cb->rows) {
    num_rows = cb->rows;
  }
  unsigned row = cb->current_row;
  ++row;
  if (row >= cb->rows) {row = 0;}
  for (unsigned c = 0; c < cb->columns; ++c) {
    cb->values[(row * cb->columns) + c] = NAN;
  }
  double *cleared = &cb->values[row * cb->columns];
  if (row + num_rows - 1 >= cb->rows) {
    copy_cleared_row(cb, cleared, cb->rows - row - 1);
    for (unsigned c = 0; c < cb->columns; ++c) {
      cb->values[c] = NAN;
    }
    copy_cleared_row(cb, cb->values, row + num_rows - 1 - cb->rows);
  } else {
    copy_cleared_row(cb, cleared, num_rows - 1);
  }
}


static int cb_new(lua_State *lua)
{
  int n = lua_gettop(lua);
  luaL_argcheck(lua, n >= 3 && n <= 4, 0, "incorrect number of arguments");
  int rows = luaL_checkint(lua, 1);
  luaL_argcheck(lua, 1 < rows, 1, "rows must be > 1");
  int columns = luaL_checkint(lua, 2);
  luaL_argcheck(lua, 0 < columns, 2, "columns must be > 0");
  int seconds_per_row = luaL_checkint(lua, 3);
  luaL_argcheck(lua, 0 < seconds_per_row, 3, "seconds_per_row is out of range");
  int delta = 0;
  if (4 == n) {
    delta = lua_toboolean(lua, 4);
  }

  size_t header_bytes = sizeof(header_info) * columns;
  size_t buffer_bytes = sizeof(double) * rows * columns;
  size_t struct_bytes = sizeof(circular_buffer);

  size_t nbytes = header_bytes + buffer_bytes + struct_bytes;
  circular_buffer *cb = (circular_buffer *)lua_newuserdata(lua, nbytes);
  cb->ref = LUA_NOREF;
  cb->delta = delta;
  cb->format = OUTPUT_CBUF;
  cb->headers = (header_info *)&cb->values[rows * columns];

  luaL_getmetatable(lua, mozsvc_circular_buffer);
  lua_setmetatable(lua, -2);

  cb->current_time = seconds_per_row * (rows - 1);
  cb->current_row = rows - 1;
  cb->rows = rows;
  cb->columns = columns;
  cb->seconds_per_row = seconds_per_row;
  memset(cb->headers, 0, header_bytes);
  for (unsigned column_idx = 0; column_idx < cb->columns; ++column_idx) {
    snprintf(cb->headers[column_idx].name, COLUMN_NAME_SIZE,
             "Column_%d", column_idx + 1);
    strncpy(cb->headers[column_idx].unit, default_unit,
            UNIT_LABEL_SIZE - 1);
  }
  clear_rows(cb, rows);
  return 1;
}


static circular_buffer* check_circular_buffer(lua_State *lua, int min_args)
{
  circular_buffer *cb = luaL_checkudata(lua, 1, mozsvc_circular_buffer);
  luaL_argcheck(lua, min_args <= lua_gettop(lua), 0,
                "incorrect number of arguments");
  return cb;
}


static int check_row(circular_buffer *cb, double ns, int advance)
{
  time_t t = (time_t)(ns / 1e9);
  t = t - (t % cb->seconds_per_row);

  int current_row = (int)(cb->current_time / cb->seconds_per_row);
  int requested_row = (int)(t / cb->seconds_per_row);
  int row_delta = requested_row - current_row;
  int row = requested_row % cb->rows;

  if (row_delta > 0 && advance) {
    clear_rows(cb, row_delta);
    cb->current_time = t;
    cb->current_row = row;
  } else if (requested_row > current_row
             || abs(row_delta) >= (int)cb->rows) {
    return -1;
  }
  return row;
}


static int check_column(lua_State *lua, circular_buffer *cb, int arg)
{
  unsigned column = luaL_checkint(lua, arg);
  luaL_argcheck(lua, 1 <= column && column <= cb->columns, arg,
                "column out of range");
  --column; // make zero based
  return column;
}


static void cb_add_delta(lua_State *lua, circular_buffer *cb,
                         double ns, int column, double value)
{
  // Storing the deltas in a Lua table allows the sandbox to account for the
  // memory usage. todo: if too inefficient use a C data struct and report
  // memory usage back to the sandbox
  time_t t = (time_t)(ns / 1e9);
  t = t - (t % cb->seconds_per_row);
  lua_getglobal(lua, mozsvc_circular_buffer_table);
  if (lua_istable(lua, -1)) {
    if (cb->ref == LUA_NOREF) {
      lua_newtable(lua);
      cb->ref = luaL_ref(lua, -2);
    }
    // get the delta table for this cbuf
    lua_rawgeti(lua, -1, cb->ref);
    if (!lua_istable(lua, -1)) {
      lua_pop(lua, 2); // remove bogus table and cbuf table
      return;
    }

    // get the delta row using the timestamp
    lua_rawgeti(lua, -1, (int)t);
    if (!lua_istable(lua, -1)) {
      lua_pop(lua, 1); // remove non table entry
      lua_newtable(lua);
      lua_rawseti(lua, -2, (int)t);
      lua_rawgeti(lua, -1, (int)t);
    }

    // get the previous delta value
    lua_rawgeti(lua, -1, column);
    value += lua_tonumber(lua, -1);
    lua_pop(lua, 1); // remove the old value

    // push the new delta
    lua_pushnumber(lua, value);
    lua_rawseti(lua, -2, column);

    lua_pop(lua, 2); // remove ref table, timestamped row
  } else {
    luaL_error(lua, "Could not find table %s", mozsvc_circular_buffer_table);
  }
  lua_pop(lua, 1); // remove the circular buffer table or failed nil
  return;
}


static int cb_add(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 4);
  double ns = luaL_checknumber(lua, 2);
  int row = check_row(cb,
                      ns,
                      1); // advance the buffer forward if
                          // necessary
  int column = check_column(lua, cb, 3);
  double value = luaL_checknumber(lua, 4);
  if (row != -1) {
    int i = (row * cb->columns) + column;
    if (isnan(cb->values[i])) {
      cb->values[i] = value;
    } else {
      cb->values[i] += value;
    }
    lua_pushnumber(lua, cb->values[i]);
    if (cb->delta && value != 0) {
      if (cb->headers[column].aggregation != AGGREGATION_SUM) {
        value = cb->values[i];
      }
      cb_add_delta(lua, cb, ns, column, value);
    }
  } else {
    lua_pushnil(lua);
  }
  return 1;
}


static int cb_get(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 3);
  int row = check_row(cb,
                      luaL_checknumber(lua, 2),
                      0);
  int column = check_column(lua, cb, 3);

  if (row != -1) {
    lua_pushnumber(lua, cb->values[(row * cb->columns) + column]);
  } else {
    lua_pushnil(lua);
  }
  return 1;
}


static int cb_get_configuration(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 1);

  lua_pushnumber(lua, cb->rows);
  lua_pushnumber(lua, cb->columns);
  lua_pushnumber(lua, cb->seconds_per_row);
  return 3;
}


static int cb_set(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 4);
  double ns = luaL_checknumber(lua, 2);
  int row = check_row(cb, ns, 1); // advance the buffer forward if
                                  // necessary
  int column = check_column(lua, cb, 3);
  double value = luaL_checknumber(lua, 4);

  if (row != -1) {
    int i = (row * cb->columns) + column;
    double old = cb->values[i];
    switch (cb->headers[column].aggregation) {
    case AGGREGATION_MIN:
      if (isnan(cb->values[i]) || value < old) {
        cb->values[i] = value;
        if (cb->delta) {
          cb_add_delta(lua, cb, ns, column, value);
        }
      }
      break;
    case AGGREGATION_MAX:
      if (isnan(cb->values[i]) || value > old) {
        cb->values[i] = value;
        if (cb->delta) {
          cb_add_delta(lua, cb, ns, column, value);
        }
      }
      break;
    default:
      cb->values[i] = value;
      if (cb->delta) {
        if (!isnan(old) && old != INFINITY && old != -INFINITY) {
          value -= old;
        }
        cb_add_delta(lua, cb, ns, column, value);
      }
      break;
    }
    lua_pushnumber(lua, cb->values[i]);
  } else {
    lua_pushnil(lua);
  }
  return 1;
}


static int cb_set_header(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 3);
  int column = check_column(lua, cb, 2);
  const char *name = luaL_checkstring(lua, 3);
  const char *unit = luaL_optstring(lua, 4, default_unit);
  cb->headers[column].aggregation = luaL_checkoption(lua, 5, "sum",
                                                     column_aggregation_methods);

  strncpy(cb->headers[column].name, name, COLUMN_NAME_SIZE - 1);
  char *n = cb->headers[column].name;
  for (int j = 0; n[j] != 0; ++j) {
    if (!isalnum(n[j])) {
      n[j] = '_';
    }
  }
  strncpy(cb->headers[column].unit, unit, UNIT_LABEL_SIZE - 1);
  n = cb->headers[column].unit;
  for (int j = 0; n[j] != 0; ++j) {
    if (n[j] != '/' && n[j] != '*' && !isalnum(n[j])) {
      n[j] = '_';
    }
  }

  lua_pushinteger(lua, column + 1); // return the 1 based Lua column
  return 1;
}


static int cb_get_header(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 2);
  int column = check_column(lua, cb, 2);

  lua_pushstring(lua, cb->headers[column].name);
  lua_pushstring(lua, cb->headers[column].unit);
  lua_pushstring(lua,
                 column_aggregation_methods[cb->headers[column].aggregation]);
  return 3;
}


static int cb_get_range(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 2);
  int column = check_column(lua, cb, 2);

  // optional range arguments
  double start_ns = luaL_optnumber(lua, 3, get_start_time(cb) * 1e9);
  double end_ns = luaL_optnumber(lua, 4, cb->current_time * 1e9);
  luaL_argcheck(lua, end_ns >= start_ns, 4, "end must be >= start");

  int start_row = check_row(cb, start_ns, 0);
  int end_row = check_row(cb, end_ns, 0);
  if (-1 == start_row || -1 == end_row) {
    lua_pushnil(lua);
    return 1;
  }

  lua_newtable(lua);
  int row = start_row;
  int i = 0;
  do {
    if (row == (int)cb->rows) {
      row = 0;
    }
    lua_pushnumber(lua, cb->values[(row * cb->columns) + column]);
    lua_rawseti(lua, -2, ++i);
  } while (row++ != end_row);

  return 1;
}


static int cb_current_time(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 0);
  lua_pushnumber(lua, cb->current_time * 1e9);
  return 1; // return the current time
}

static int cb_format(lua_State *lua)
{
  static const char *output_types[] = { "cbuf", "cbufd", NULL };
  circular_buffer *cb = check_circular_buffer(lua, 2);
  luaL_argcheck(lua, 2 == lua_gettop(lua), 0,
                "incorrect number of arguments");

  cb->format = luaL_checkoption(lua, 2, NULL, output_types);
  lua_pop(lua, 1); // remove the format
  return 1; // return the circular buffer object
}


static void read_time_row(char **p, circular_buffer *cb)
{
  cb->current_time = (time_t)strtoll(*p, &*p, 10);
  cb->current_row = strtoul(*p, &*p, 10);
}


static int read_double(char **p, double *value)
{
  while (**p != 0 && isspace(**p)) {
    ++*p;
  }
  if (0 == **p) return 0;

#ifdef _MSC_VER
  if ((*p)[0] == 'n' && strncmp(*p, not_a_number, 3) == 0) {
    *p += 3;
    *value = NAN;
  } else if ((*p)[0] == 'i' && strncmp(*p, "inf", 3) == 0) {
    *p += 3;
    *value = INFINITY;
  } else if ((*p)[0] == '-' && strncmp(*p, "-inf", 4) == 0) {
    *p += 4;
    *value = -INFINITY;
  } else {
    *value = strtod(*p, &*p);
  }
#else
  *value = strtod(*p, &*p);
#endif
  return 1;
}


static void cb_delta_fromstring(lua_State *lua,
                                circular_buffer *cb,
                                char **p)
{
  double value, ns = 0;
  size_t pos = 0;
  while (read_double(&*p, &value)) {
    if (pos == 0) { // new row, starts with a time_t
      ns = value * 1e9;
    } else {
      cb_add_delta(lua, cb, ns, (int)(pos - 1), value);
    }
    if (pos == cb->columns) {
      pos = 0;
    } else {
      ++pos;
    }
  }
  if (pos != 0) {
    lua_pushstring(lua, "fromstring() invalid delta");
    lua_error(lua);
  }
  return;
}


static int cb_fromstring(lua_State *lua)
{
  circular_buffer *cb = check_circular_buffer(lua, 2);
  const char *values = luaL_checkstring(lua, 2);

  char *p = (char *)values;
  read_time_row(&p, cb);

  size_t pos = 0;
  size_t len = cb->rows * cb->columns;
  double value;
  while (pos < len && read_double(&p, &value)) {
    cb->values[pos++] = value;
  }
  if (pos == len) {
    if (cb->delta) {
      cb_delta_fromstring(lua, cb, &p);
    }
  } else {
    luaL_error(lua, "fromstring() too few values: %d, expected %d", pos, len);
  }
  if (read_double(&p, &value)) {
    luaL_error(lua, "fromstring() too many values, more than: %d", len);
  }
  return 0;
}


static int cb_version(lua_State *lua)
{
  lua_pushstring(lua, DIST_VERSION);
  return 1;
}


#ifdef LUA_SANDBOX
static int
output_cbuf(circular_buffer *cb, lsb_output_buffer *ob)
{
  unsigned column_idx;
  unsigned row_idx = cb->current_row + 1;
  for (unsigned i = 0; i < cb->rows; ++i, ++row_idx) {
    if (row_idx >= cb->rows) row_idx = 0;
    for (column_idx = 0; column_idx < cb->columns; ++column_idx) {
      if (column_idx != 0) {
        if (lsb_outputc(ob, '\t')) return 1;
      }
      if (lsb_outputd(ob, cb->values[(row_idx * cb->columns) + column_idx])) {
        return 1;
      }
    }
    if (lsb_outputc(ob, '\n')) return 1;
  }
  return 0;
}


static int
output_cb_cbufd(lua_State *lua, circular_buffer *cb,
                lsb_output_buffer *ob)
{
  lua_getglobal(lua, mozsvc_circular_buffer_table);
  if (lua_istable(lua, -1)) {
    // get the delta table for this cbuf
    lua_rawgeti(lua, -1, cb->ref);
    if (!lua_istable(lua, -1)) {
      lua_pop(lua, 2); // remove bogus table and cbuf table
      luaL_error(lua, "Could not find the delta table");
    }
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
      if (!lua_istable(lua, -1)) {
        luaL_error(lua, "Invalid delta table structure");
      }
      if (lsb_outputd(ob, lua_tonumber(lua, -2))) return 1;
      for (unsigned column_idx = 0; column_idx < cb->columns;
           ++column_idx) {
        if (lsb_outputc(ob, '\t')) return 1;
        lua_rawgeti(lua, -1, column_idx);
        if (LUA_TNIL == lua_type(lua, -1)) {
          if (lsb_outputs(ob, not_a_number, 3)) return 1;
        } else {
          if (lsb_outputd(ob, lua_tonumber(lua, -1))) return 1;
        }
        lua_pop(lua, 1); // remove the number
      }
      if (lsb_outputc(ob, '\n')) return 1;
      lua_pop(lua, 1); // remove the value, keep the key
    }
    lua_pop(lua, 1); // remove the delta table

    // delete the delta table
    luaL_unref(lua, -1, cb->ref);
    cb->ref = LUA_NOREF;
  } else {
    luaL_error(lua, "Could not find table %s", mozsvc_circular_buffer_table);
  }
  lua_pop(lua, 1); // remove the circular buffer table or failed nil
  return 0;
}


static int output_circular_buffer(lua_State *lua)
{
  lsb_output_buffer *ob = lua_touserdata(lua, -1);
  circular_buffer *cb = lua_touserdata(lua, -2);
  if (!(ob && cb)) {
    return 1;
  }
  if (OUTPUT_CBUFD == cb->format) {
    if (cb->ref == LUA_NOREF) return 0;
  }

  if (lsb_outputf(ob,
                  "{\"time\":%lld,\"rows\":%d,\"columns\":%d,\""
                  "seconds_per_row\":%d,\"column_info\":[",
                  (long long)get_start_time(cb),
                  cb->rows,
                  cb->columns,
                  cb->seconds_per_row)) {
    return 1;
  }

  unsigned column_idx;
  for (column_idx = 0; column_idx < cb->columns; ++column_idx) {
    if (column_idx != 0) {
      if (lsb_outputc(ob, ',')) return 1;
    }
    if (lsb_outputf(ob, "{\"name\":\"%s\",\"unit\":\"%s\",\""
                    "aggregation\":\"%s\"}",
                    cb->headers[column_idx].name,
                    cb->headers[column_idx].unit,
                    column_aggregation_methods[cb->headers[column_idx].aggregation])) {
      return 1;
    }
  }
  if (lsb_outputs(ob, "]}\n", 3)) return 1;

  if (OUTPUT_CBUFD == cb->format) {
    return output_cb_cbufd(lua, cb, ob);
  }
  return output_cbuf(cb, ob);
}


static int serialize_cb_delta(lua_State *lua, circular_buffer *cb,
                              lsb_output_buffer *ob)
{
  if (cb->ref == LUA_NOREF) return 0;
  lua_getglobal(lua, mozsvc_circular_buffer_table);
  if (lua_istable(lua, -1)) {
    // get the delta table for this cbuf
    lua_rawgeti(lua, -1, cb->ref);
    if (!lua_istable(lua, -1)) {
      lua_pop(lua, 2); // remove bogus table and cbuf table
      luaL_error(lua, "Could not find the delta table");
    }
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
      if (!lua_istable(lua, -1)) {
        luaL_error(lua, "Invalid delta table structure");
      }
      if (lsb_outputc(ob, ' ')) return 1;
      // intentionally not serialized as Lua
      if (lsb_outputd(ob, lua_tonumber(lua, -2))) return 1;

      for (unsigned column_idx = 0; column_idx < cb->columns;
           ++column_idx) {
        if (lsb_outputc(ob, ' ')) return 1;
        lua_rawgeti(lua, -1, column_idx);
        // intentionally not serialized as Lua
        if (lsb_outputd(ob, lua_tonumber(lua, -1))) return 1;
        lua_pop(lua, 1); // remove the number
      }
      lua_pop(lua, 1); // remove the value, keep the key
    }
    lua_pop(lua, 1); // remove the delta table

    // delete the delta table
    lua_pushnil(lua);
    lua_rawseti(lua, -2, cb->ref);
    cb->ref = LUA_NOREF;
  } else {
    luaL_error(lua, "Could not find table %s", mozsvc_circular_buffer_table);
  }
  lua_pop(lua, 1); // remove the circular buffer table or failed nil
  return 0;
}


static int serialize_circular_buffer(lua_State *lua)
{
  lsb_output_buffer *ob = lua_touserdata(lua, -1);
  const char *key = lua_touserdata(lua, -2);
  circular_buffer *cb = lua_touserdata(lua, -3);
  if (!(ob && key && cb)) {
    return 1;
  }
  char *delta = "";
  if (cb->delta) {
    delta = ", true";
  }
  if (lsb_outputf(ob,
                  "if %s == nil then %s = circular_buffer.new(%d, %d, %d%s) end\n",
                  key,
                  key,
                  cb->rows,
                  cb->columns,
                  cb->seconds_per_row,
                  delta)) {
    return 1;
  }

  unsigned column_idx;
  for (column_idx = 0; column_idx < cb->columns; ++column_idx) {
    if (lsb_outputf(ob, "%s:set_header(%d, \"%s\", \"%s\", \"%s\")\n",
                    key,
                    column_idx + 1,
                    cb->headers[column_idx].name,
                    cb->headers[column_idx].unit,
                    column_aggregation_methods[cb->headers[column_idx].aggregation])) {
      return 1;
    }
  }

  if (lsb_outputf(ob, "%s:fromstring(\"%lld %d",
                  key,
                  (long long)cb->current_time,
                  cb->current_row)) {
    return 1;
  }
  for (unsigned row_idx = 0; row_idx < cb->rows; ++row_idx) {
    for (column_idx = 0; column_idx < cb->columns; ++column_idx) {
      if (lsb_outputc(ob, ' ')) return 1;
      // intentionally not serialized as Lua
      if (lsb_outputd(ob, cb->values[(row_idx * cb->columns) + column_idx])) {
        return 1;
      }
    }
  }
  if (serialize_cb_delta(lua, cb, ob)) {
    return 1;
  }
  if (lsb_outputs(ob, "\")\n", 3)) {
    return 1;
  }
  return 0;
}
#endif


static const struct luaL_reg circular_bufferlib_f[] =
{
  { "new", cb_new },
  { "version", cb_version },
  { NULL, NULL }
};

static const struct luaL_reg circular_bufferlib_m[] =
{
  { "add", cb_add },
  { "get", cb_get },
  { "get_configuration", cb_get_configuration },
  { "set", cb_set },
  { "set_header", cb_set_header },
  { "get_header", cb_get_header },
  { "get_range", cb_get_range },
  { "current_time", cb_current_time },
  { "format", cb_format },
  { "fromstring", cb_fromstring } // used for data restoration
  , { NULL, NULL }
};


int luaopen_circular_buffer(lua_State *lua)
{
#ifdef LUA_SANDBOX
  lua_newtable(lua);
  lsb_add_serialize_function(lua, serialize_circular_buffer);
  lsb_add_output_function(lua, output_circular_buffer);
  lua_replace(lua, LUA_ENVIRONINDEX);
#endif
  luaL_newmetatable(lua, mozsvc_circular_buffer);
  lua_pushvalue(lua, -1);
  lua_setfield(lua, -2, "__index");
  luaL_register(lua, NULL, circular_bufferlib_m);
  luaL_register(lua, mozsvc_circular_buffer_table, circular_bufferlib_f);
  return 1;
}
