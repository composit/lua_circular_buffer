Lua Circular Buffer Library
---------------------------

## Overview
The circular buffer library is a sliding window time series data store with
basic data analysis functionality.

## Installation

### Prerequisites
* C compiler (GCC 4.7+, Visual Studio 2013, MinGW (Lua 5.1))
* Lua 5.1, Lua 5.2, or LuaJIT
* [CMake (2.8.7+)](http://cmake.org/cmake/resources/software.html)

### CMake Build Instructions

    git clone https://github.com/mozilla-services/lua_circular_buffer.git
    cd lua_circular_buffer 
    mkdir release
    cd release

    # UNIX
    cmake -DCMAKE_BUILD_TYPE=release ..
    make

    # Windows Visuas Studio 2013
    cmake -DCMAKE_BUILD_TYPE=release -G "NMake Makefiles" ..
    nmake

    ctest
    cpack

## Module

### Example Usage
```lua
require "circular_buffer"

local cb = circular_buffer.new(1440, 1, 60)
local ERRORS = cb:set_header(1, "Errors")
cb:add(1e9, ERRORS, 1)
cb:add(1e9, ERRORS, 7)
local val = cb:get(1e9, ERRORS)
-- val == 8
```

### API Functions

#### new
```lua
require "circular_buffer"
local cb = circular_buffer.new(1440, 1, 60)
```

Import the Lua _circular_buffer_ via the Lua 'require' function. The module is
globally registered and returned by the require function. 

*Arguments*
- rows (unsigned) The number of rows in the buffer (must be > 1)
- columns (unsigned)The number of columns in the buffer (must be > 0)
- seconds_per_row (unsigned) The number of seconds each row represents (must be > 0).
- enable_delta (bool _optional, default false_) When true the changes made to the 
    circular buffer between delta outputs are tracked.

*Return*
- circular_buffer userdata object.

#### version
```lua
local v = circular_buffer.version()
-- v == "0.1.0"
```

Returns a string with the running version of circular_buffer.

*Arguments*
- none

*Return*
- Semantic version string

### API Methods
**Note:** All column arguments are 1 based. If the column is out of range for the configured circular buffer a fatal error is generated.

#### add
```lua
d = cb:add(1e9, 1, 1)
-- d == 1
d = cb:add(1e9, 1, 99)
-- d == 100
```

Adds a value to the specified row/column in the circular buffer.

*Arguments*
- nanosecond (unsigned) The number of nanosecond since the UNIX epoch. The value is 
    used to determine which row is being operated on.
- column (unsigned) The column within the specified row to perform an add operation on.
- value (double) The value to be added to the specified row/column.

*Return*
- The value of the updated row/column or nil if the time was outside the range of the buffer.


#### set
```lua
d = cb:set(1e9, 1, 1)
-- d == 1
d = cb:set(1e9, 1, 99)
-- d == 99
```

Overwrites the value at a specific row/column in the circular buffer.

*Arguments*
- nanosecond (unsigned) The number of nanosecond since the UNIX epoch. The value is
    used to determine which row is being operated on.
- column (unsigned) The column within the specified row to perform a set operation on.
- value (double) The value to be overwritten at the specified row/column. 
  For aggregation methods "min" and "max" the value is only overwritten if it is smaller/larger than the current value.

*Return*
- The resulting value of the row/column or nil if the time was outside the range of the buffer.

#### get
```lua
d = cb:get(1e9, 1)
-- d == 99
```

Fetches the value at a specific row/column in the circular buffer.

*Arguments*
- nanosecond (unsigned) The number of nanosecond since the UNIX epoch. The value is used
    to determine which row is being operated on.
- column (unsigned) The column within the specified row to retrieve the data from.

*Return*
- The value at the specifed row/column or nil if the time was outside the range of the buffer.

#### get_configuration
```lua
rows, columns, seconds_per_row = cb:get_configuration()
-- rows == 1440
-- columns = 1
-- seconds_per_row = 60
```

Returns the configuration options passed to _new_.

*Arguments*
- none

*Return*
- The circular buffer dimension values specified in the constructor.
    - rows
    - columns
    - seconds_per_row

#### set_header
```lua
column = cb:set_header(1, "Errors")
-- column == 1

```

Sets the header metadata for the specifed column.

*Arguments*
- column (unsigned) The column number where the header information is applied.
- name (string) Descriptive name of the column (maximum 15 characters). Any non alpha
    numeric characters will be converted to underscores. (default: Column_N)
- unit (string _optional_) The unit of measure (maximum 7 characters). Alpha numeric,
    '/', and '*' characters are allowed everything else will be converted to underscores.
    i.e. KiB, Hz, m/s (default: count)
- aggregation_method (string _optional_) Controls how the column data is aggregated
    when combining multiple circular buffers.
    - **sum** The total is computed for the time/column (default).
    - **min** The smallest value is retained for the time/column.
    - **max** The largest value is retained for the time/column.
    - **none** No aggregation will be performed the column.

*Return*
- The column number passed into the function.

#### get_header
```lua
name, unit, aggregation_method = cb:get_header(1)
-- name == "Errors"
-- unit == "count"
-- aggregation_method == "sum"

```

Retrieves the header metadata for the specified column.

*Arguments*
- column (unsigned) The column number of the header information to be retrieved.

*Return*
- The current values of specified header column.
    - name
    - unit
    - aggregation_method

#### compute
```lua
cb:set(1e9, 5)
cb:set(60e9, 10)
cb:set(180e9, 1)
d, active_rows = cb:compute("sum", 1)
-- d = 16
-- active_rows = 3
```

Performs a basic calculation on a column spaning the specified number of rows.

*Arguments*
- function (string) The name of the compute function (sum|avg|sd|min|max|variance).
- column (unsigned) The column that the computation is performed against.
- start (unsigned _optional_) The number of nanosecond since the UNIX epoch. Sets the
    start time of the computation range; if nil the buffer's start time is used.
- end (unsigned _optional_) The number of nanosecond since the UNIX epoch. Sets the 
    end time of the computation range (inclusive); if nil the buffer's end time is used.
    The end time must be greater than or equal to the start time.

*Returns*
- The result of the computation for the specifed column over the given range or nil if the range fell outside of the buffer.
- The number of rows that contained a valid numeric value.

#### mannwhitneyu
```lua
local cb = circular_buffer.new(40,1,1)
local data = {15309,14092,13661,13412,14205,15042,14142,13820,14917,13953,14320,14472,15133,13790,14539,14129,14363,14202,13841,13610,13759,14428,14851,13838,13819,14468,14989,15557,14380,13500,14818,14632,13631,14663,14532,14188,14537,14109,13925,15022}
for i,v in ipairs(data) do
    cb:set(i*1e9, 1, v)
end
local u, p = cb:mannwhitneyu(1, 1e9, 20e9, 21e9, 40e9)
-- u == 171
-- p == 0.22037
```

Computes the Mann-Whitney rank test on samples x and y.

*Arguments*
- column (unsigned) The column that the computation is performed against.
- start_1 (unsigned) The number of nanosecond since the UNIX epoch.
- end_1 (unsigned) The number of nanosecond since the UNIX epoch. The end time must be greater than or equal to the start time.
- start_2 (unsigned).
- end_2 (unsigned).
- use_continuity (bool _optional_) Whether a continuity correction (1/2) should be taken into account (default: true).

*Returns* (nil if the range fell outside the buffer)
- U_1 Mann-Whitney statistic.
- One-sided p-value assuming a asymptotic normal distribution.

**Note:** Use only when the number of observation in each sample is > 20 and you have 2 independent samples of ranks. 
Mann-Whitney U is significant if the u-obtained is LESS THAN or equal to the critical value of U.

This test corrects for ties and by default uses a continuity correction. The reported p-value is for a one-sided
hypothesis, to get the two-sided p-value multiply the returned p-value by 2.

#### current_time
```lua
t = cb:current_time()
-- t == 86340000000000

```

Returns the timestamp of the newest row.

*Arguments*
- none

*Return*
- The time of the most current row in the circular buffer (nanoseconds).

#### format
```lua
cb:format("cbufd")

```

Sets an internal flag to control the output format of the circular buffer data structure; if deltas are not enabled or there haven't been any modifications, nothing is output.

*Arguments*
- format (string)
    - **cbuf** The circular buffer full data set format.
    - **cbufd** The circular buffer delta data set format.

*Return*
- The circular buffer object.

### Output 
**todo** make this accessible as __tostring for non lua_sandbox use.
```lua
-- only available when using the lua_sandbox
cb:format("cbuf")
output(cb) -- serializes the full buffer
cb:format("cbufd")
output(cb) -- serializes the delta of the buffer since the last output

```

The circular buffer can be passed to the lua_sandbox output() function. The
output format can be selected using the format() function.

The cbuf (full data set) output format consists of newline delimited rows
starting with a json header row followed by the data rows with tab delimited
columns. The time in the header corresponds to the time of the first data row,
the time for the other rows is calculated using the seconds_per_row header value.

    {json header}
    row1_col1\trow1_col2\n
    .
    .
    .
    rowN_col1\trowN_col2\n

The cbufd (delta) output format consists of newline delimited rows starting with
a json header row followed by the data rows with tab delimited columns. The
first column is the timestamp for the row (time_t). The cbufd output will only
contain the rows that have changed and the corresponding delta values for each
column.

    {json header}
    row14_timestamp\trow14_col1\trow14_col2\n
    row10_timestamp\trow10_col1\trow10_col2\n

Sample Cbuf Output
------------------

    {"time":2,"rows":3,"columns":3,"seconds_per_row":60,"column_info":[{"name":"HTTP_200","unit":"count","aggregation":"sum"},{"name":"HTTP_400","unit":"count","aggregation":"sum"},{"name":"HTTP_500","unit":"count","aggregation":"sum"}]}
    10002   0   0
    11323   0   0
    10685   0   0

