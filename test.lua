-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.

require "circular_buffer"
require "math"
require "string"

local errors = {
    function() local cb = circular_buffer.new(2) end, -- new() incorrect # args
    function() local cb = circular_buffer.new(nil, 1, 1) end, -- new() non numeric row
    function() local cb = circular_buffer.new(1, 1, 1) end, -- new() 1 row
    function() local cb = circular_buffer.new(2, nil, 1) end,-- new() non numeric column
    function() local cb = circular_buffer.new(2, 0, 1) end, -- new() zero column
    function() local cb = circular_buffer.new(2, 1, nil) end, -- new() non numeric seconds_per_row
    function() local cb = circular_buffer.new(2, 1, 0) end, -- new() zero seconds_per_row
    function() local cb = circular_buffer.new(2, 1, 1) -- set() out of range column
    cb:set(0, 2, 1.0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- set() zero column
    cb:set(0, 0, 1.0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- set() non numeric column
    cb:set(0, nil, 1.0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- set() non numeric time
    cb:set(nil, 1, 1.0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- get() invalid object
    local invalid = 1
    cb.get(invalid, 1, 1) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- set() non numeric value
    cb:set(0, 1, nil) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- set() incorrect # args
    cb:set(0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- add() incorrect # args
    cb:add(0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- get() incorrect # args
    cb:get(0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- get_range() incorrect # args
    cb:get_range() end,
    function() local cb = circular_buffer.new(2, 1, 1) -- get_range() incorrect column
    cb:get_range(0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- get_range() start > end
    cb:get_range(1, 2e9, 1e9) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- format() invalid
    cb:format("invalid") end,
    function() local cb = circular_buffer.new(2, 1, 1) -- format() extra
    cb:format("cbuf", true) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- format() missing
    cb:format() end,
    function() local cb = circular_buffer.new(2, 1, 1) -- too few
    cb:fromstring("") end,
    function() local cb = circular_buffer.new(2, 1, 1) -- too few invalid
    cb:fromstring("0 0 na 1") end,
    function() local cb = circular_buffer.new(2, 1, 1) -- too many
    cb:fromstring("0 0 1 2 3") end,
    function() local cb = circular_buffer.new(10, 1, 1)
    cb:get_header() end, -- incorrect # args
    function() local cb = circular_buffer.new(10, 1, 1)
    cb:get_header(99) end -- out of range column
}

for i, v in ipairs(errors) do
    local ok = pcall(v)
    if ok then error(string.format("error test %d failed\n", i)) end
end

local tests = {
    function()
        local stats = circular_buffer.new(5, 1, 1)
        stats:set(1e9, 1, 1)
        stats:set(2e9, 1, 2)
        stats:set(3e9, 1, 3)
        stats:set(4e9, 1, 4)
        stats:set(5e9, 1, 5)

        local a = stats:get_range(1)
        assert(#a == 5, #a)
        for i=1, #a do assert(i == a[i]) end

        a = stats:get_range(1, 3e9, 4e9)
        assert(#a == 2, #a)
        for i=3, 4 do assert(i == a[i-2]) end

        a = stats:get_range(1, 3e9)
        assert(#a == 3, #a)
        for i=3, 5 do assert(i == a[i-2]) end

        a = stats:get_range(1, 3e9, nil)
        assert(#a == 3, #a)
        for i=3, 5 do assert(i == a[i-2]) end

        a = stats:get_range(1, 11e9, 14e9)
        if a then error(string.format("out of range %d", #a)) end
        end,
    function()
        local stats = circular_buffer.new(2, 1, 1)
        local nan = stats:get(0, 1)
        if nan == nan then
            error(string.format("initial value is a number %G", nan))
        end
        local v = stats:set(0, 1, 1)
        if v ~= 1 then
            error(string.format("set failed = %G", v))
        end
        v = stats:add(0, 1, 0/0)
        if v == v then
            error(string.format("adding nan returned a number %G", v))
        end
        end,
    function()
        local stats = circular_buffer.new(2, 1, 1)
        local cbuf_time = stats:current_time()
        if cbuf_time ~= 1e9 then
            error(string.format("current_time = %G", cbuf_time))
        end
        local v = stats:set(0, 1, 1)
        if stats:get(0, 1) ~= 1 then
            error(string.format("set failed = %G", v))
        end
        stats:fromstring("1 1 nan 99")
        local nan = stats:get(0, 1)
        if nan == nan then
            error(string.format("restored value is a number %G", nan))
        end
        v = stats:get(1e9, 1)
        if v ~= 99 then
            error(string.format("restored value is %G", v))
        end
        end,
    function()
        local cb = circular_buffer.new(10,1,1)
        local rows, cols, spr = cb:get_configuration()
        assert(rows == 10, "invalid rows")
        assert(cols == 1 , "invalid columns")
        assert(spr  == 1 , "invalid seconds_per_row")
        end,
    function()
        local cb = circular_buffer.new(10,1,1)
        local args = {"widget", "count", "max"}
        local col = cb:set_header(1, args[1], args[2], args[3])
        assert(col == 1, "invalid column")
        local n, u, m = cb:get_header(col)
        assert(n == args[1], "invalid name")
        assert(u == args[2], "invalid unit")
        assert(m == args[3], "invalid aggregation_method")
        end,
    function()
        local cb = circular_buffer.new(10,1,1)
        assert(not cb:get(10*1e9, 1), "value found beyond the end of the buffer")
        cb:set(20*1e9, 1, 1)
        assert(not cb:get(10*1e9, 1), "value found beyond the start of the buffer")
        end,
}

for i, v in ipairs(tests) do
  v()
end

