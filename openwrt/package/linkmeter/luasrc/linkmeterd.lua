#! /usr/bin/env lua

local io = require("io")
local os = require("os")
local rrd = require("rrd")
local nixio = require("nixio")
nixio.fs = require("nixio.fs")
local uci = require("uci")

local SERIAL_DEVICE = "/dev/ttyS1"
local RRD_FILE = "/tmp/hm.rrd"
local JSON_FILE = "/tmp/json"

local probeNames = { "Pit", "Food Probe1", "Food Probe2", "Ambient" }

function rrdCreate()
 return rrd.create(
   RRD_FILE,
   "--step", "2",
   "DS:sp:GAUGE:30:0:1000",
   "DS:t0:GAUGE:30:0:1000",
   "DS:t1:GAUGE:30:0:1000",
   "DS:t2:GAUGE:30:0:1000",
   "DS:t3:GAUGE:30:0:1000",
   "DS:f:GAUGE:30:-1000:100",
   "RRA:AVERAGE:0.6:5:360",
   "RRA:AVERAGE:0.6:30:360",
   "RRA:AVERAGE:0.6:60:360",
   "RRA:AVERAGE:0.6:90:480"
 )
end

function jsonWrite(vals)
  local p, pn
  local data = ('{"time":%s,"temps":['):format(vals[1])

  for p,pn in ipairs(probeNames) do
    data = data .. ('{"n":"%s","c":%s,"a":%s},'):format(
      pn, vals[p*2+1], vals[p*2+2])
  end

  data = data .. ('{}],"set":%s,"lid":%s,"fan":{"c":%s,"a":%s}}'):format(
      vals[2],vals[13],vals[11],vals[12])
  return nixio.fs.writefile(JSON_FILE, data)
end

local hm = io.open(SERIAL_DEVICE, "rwb")
if hm == nil then
  die("Can not open serial device")
end

nixio.umask("0022")

-- Create database
if not nixio.fs.access(RRD_FILE) then
  rrdCreate()
end

-- Create json file
if not nixio.fs.access("/www/json") then
  if not nixio.fs.symlink(JSON_FILE, "/www/json") then
    print("Can not create JSON file link")
  end
end

local hmline
while true do
  hmline = hm:read("*l") 
  if hmline == nil then break end
  
  if hmline:sub(1,2) ~= "OK" then
    local vals = {}
    for i in hmline:gmatch("[0-9.]+") do
      vals[#vals+1] = i
    end
    
    if (#vals == 12) then 
      -- Add the time as the first item
      table.insert(vals, 1, tostring(os.time()))
      
      jsonWrite(vals)
      
      local lid = tonumber(vals[13]) or 0
      -- If the lid value is non-zero, it replaces the fan value
      if lid ~= 0 then
        vals[11] = lid
      end
      table.remove(vals, 13) -- lid
      table.remove(vals, 12) -- fan avg
      table.remove(vals, 10) -- amb avg
      table.remove(vals, 8) -- food2 avg
      table.remove(vals, 6) -- food1 avg
      table.remove(vals, 4) -- pit avg
      
      rrd.update(RRD_FILE, table.concat(vals, ":"))
    end
    -- break
  end
end

hm:close()
nixio.fs.unlink("/var/run/linkmeterd/lmd.pid")

