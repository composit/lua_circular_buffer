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
    function() local cb = circular_buffer.new(2, 1, 1) -- compute() incorrect # args
    cb:compute(0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- compute() incorrect function
    cb:compute("func", 1) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- compute() incorrect column
    cb:compute("sum", 0) end,
    function() local cb = circular_buffer.new(2, 1, 1) -- compute() start > end
    cb:compute("sum", 1, 2e9, 1e9) end,
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
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu() end,-- incorrect # args
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(nil, 0, 0, 0, 0) end, -- non numeric column
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(0, 0, 0, 0, 0) end, -- invalid column
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(1, 0, 5, 2, 7) end, -- overlapping x,y
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(1, 5, 0, 2, 7) end, -- inverted x
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(1, 0, 5, 10, 6) end, -- inverted y
    function() local cb = circular_buffer.new(2, 1, 1)
    cb:mannwhitneyu(1, 0, 1, 2, 3, true, 5) end, -- incorrect # args
    function() local cb = circular_buffer.new(10, 1, 1)
    cb:mannwhitneyu(1, 0, 5, 6, 10, "a") end, -- invalid use_continuity flag
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
        local t, c = stats:compute("sum", 1)
        if 10 ~= t then
            error(string.format("no range sum = %G", t))
        end
        if 4 ~= c then
            error(sting.format("active_rows = %d", c))
        end
        t, c = stats:compute("avg", 1)
        if 2.5 ~= t then
            error(string.format("no range avg = %G", t))
        end
        if 4 ~= c then
            error(sting.format("active_rows = %d", c))
        end
        t, c = stats:compute("variance", 1)
        if 1.25 ~= t then
            error(string.format("no range variance = %G", t))
        end
        t, c = stats:compute("sd", 1)
        if math.sqrt(1.25) ~= t then
            error(string.format("no range sd = %G", t))
        end
        if 4 ~= c then
            error(sting.format("active_rows = %d", c))
        end
        t, c = stats:compute("min", 1)
        if 1 ~= t then
            error(string.format("no range min = %G", t))
        end
        if 4 ~= c then
            error(sting.format("active_rows = %d", c))
        end
        t, c = stats:compute("max", 1)
        if 4 ~= t then
            error(string.format("no range max = %G", t))
        end
        if 4 ~= c then
            error(sting.format("active_rows = %d", c))
        end

        t = stats:compute("sum", 1, 3e9, 4e9)
        if 7 ~= t then
            error(string.format("range 3-4 sum = %G", t))
        end
        t = stats:compute("avg", 1, 3e9, 4e9)
        if 3.5 ~= t then
            error(string.format("range 3-4 avg = %G", t))
        end
        t = stats:compute("sd", 1, 3e9, 4e9)
        if math.sqrt(0.25) ~= t then
            error(string.format("range 3-4 sd = %G", t))
        end

        t = stats:compute("sum", 1, 3e9)
        if 7 ~= t then
            error(string.format("range 3- = %G", t))
        end
        t = stats:compute("sum", 1, 3e9, nil)
        if 7 ~= t then
            error(string.format("range 3-nil = %G", t))
        end
        t = stats:compute("sum", 1, nil, 2e9)
        if 3 ~= t then
            error(string.format("range nil-2 sum = %G", t))
        end
        t = stats:compute("sum", 1, 11e9, 14e9)
        if nil ~= t then
            error(string.format("out of range = %G", t))
        end
        end,
    function()
        local stats = circular_buffer.new(4, 1, 1)
        stats:set(1e9, 1, 0/0)
        stats:set(2e9, 1, 8)
        stats:set(3e9, 1, 8)
        local t = stats:compute("avg", 1)
        if 8 ~= t then
            error(string.format("no range avg = %G", t))
        end
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
        local empty = circular_buffer.new(4,1,1)
        local nan = empty:compute("avg", 1)
        if nan == nan then
            error(string.format("avg is a number %G", nan))
        end
        nan = empty:compute("sd", 1)
        if nan == nan then
            error(string.format("std is a number %G", nan))
        end
        nan = empty:compute("max", 1)
        if nan == nan then
            error(string.format("max is a number %G", m))
        end
        nan = empty:compute("min", 1)
        if nan == nan then
            error(string.format("min is a number %G", m))
        end
        end,
    function()
        local cb = circular_buffer.new(20,1,1)
        local u, p = cb:mannwhitneyu(1, 0e9, 9e9, 10e9, 19e9)
        if u or p then
            error("all the same values should return nil results")
        end
        end,
    function() -- default
        local cb = circular_buffer.new(40,1,1)
        local data = {15309,14092,13661,13412,14205,15042,14142,13820,14917,13953,14320,14472,15133,13790,14539,14129,14363,14202,13841,13610,13759,14428,14851,13838,13819,14468,14989,15557,14380,13500,14818,14632,13631,14663,14532,14188,14537,14109,13925,15022}
        for i,v in ipairs(data) do
            cb:set(i*1e9, 1, v)
        end
        local u, p = cb:mannwhitneyu(1, 1e9, 20e9, 21e9, 40e9)
        if u ~= 171 or math.floor(p * 100000) ~=  22037 then
            error(string.format("u is %g p is %g", u, p))
        end
        end,
    function() -- no continuity correction
        local cb = circular_buffer.new(40,1,1)
        local data = {15309,14092,13661,13412,14205,15042,14142,13820,14917,13953,14320,14472,15133,13790,14539,14129,14363,14202,13841,13610,13759,14428,14851,13838,13819,14468,14989,15557,14380,13500,14818,14632,13631,14663,14532,14188,14537,14109,13925,15022}
        for i,v in ipairs(data) do
            cb:set(i*1e9, 1, v)
        end
        local u, p = cb:mannwhitneyu(1, 1e9, 20e9, 21e9, 40e9, false)
        if u ~= 171 or math.floor(p * 100000) ~=  21638 then
            error(string.format("u is %g p is %g", u, p))
        end
        end,
    function() -- tie correction
        local cb = circular_buffer.new(40,1,1)
        local data = {15309,14092,13661,13412,14205,15042,14142,13820,14917,13953,14320,14472,15133,13790,14539,14129,14363,14202,13841,13610,13759,14428,14851,13838,13819,14468,14989,15557,14380,13500,14818,14632,13631,14663,14532,14188,14537,14109,13925,15309}
        for i,v in ipairs(data) do
            cb:set(i*1e9, 1, v)
        end
        local u, p = cb:mannwhitneyu(1, 1e9, 20e9, 21e9, 40e9)
        if u ~= 168.5 or math.floor(p * 100000) ~=  20084 then
            error(string.format("u is %g p is %g", u, p))
        end
        end,
    function()
        local cb = circular_buffer.new(40,1,1)
        local u, p = cb:mannwhitneyu(1, 41e9, 60e9, 61e9, 80e9)
        if u or p then
            error("times outside of buffer should return nil results")
        end
        end,
    function()
        local cb = circular_buffer.new(10,1,1)
        local data = {1,1,1,1,1,1,1,1,1,1}
        for i,v in ipairs(data) do
            cb:set(i*1e9, 1, v)
        end
        local u, p = cb:mannwhitneyu(1, 1e9, 5e9, 6e9, 10e9)
        if u or p then
            error("all the same values should return nil results")
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
    function() -- default
        local cb = circular_buffer.new(120,1,1)
        local data = {1,1,1,2,1,3,3,6,4,0/0,0/0,0/0,1,0/0,2,0/0,0/0,0/0,0/0,0/0,1,5,1,0/0,1,1,0/0,0/0,3,4,1,1,1,0/0,7,1,0/0,6,0/0,0/0,1,3,4,3,0/0,1,5,0/0,1,0/0,0/0,1,6,4,0/0,4,2,6,4,3,2,6,2,11,2,0/0,2,0/0,2,0/0,0/0,0/0,4,0/0,3,2,0/0,0/0,1,2,2,2,1,1,0/0,3,0/0,4,0/0,0/0,2,3,5,6,3,1,0/0,0/0,3,2,0/0,4,1,2,1,1,0/0,0/0,0/0,0/0,0/0,0/0,0/0,7,1,1,2,1,0/0,0/0}
        for i,v in ipairs(data) do
            cb:set(i*1e9, 1, v)
        end
        local u1 = cb:mannwhitneyu(1, 61e9, 120e9, 1e9, 60e9)
        local u2 = cb:mannwhitneyu(1, 1e9, 60e9, 61e9, 120e9)
        if u1 + u2 ~= 3600 then
            error(string.format("u1 is %g u2 is %g %g", u1, u2, maxu))
        end
        end
}

for i, v in ipairs(tests) do
  v()
end

