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

-- This might look a big hokey but rather than build the string
-- and discard it every time, just replace the values to reduce
-- the load on the garbage collector
local JSON_TEMPLATE = {
  '{"time":',
  0,
  ',"temps":[{"n":"', 'Pit', '","c":', 0, ',"a":', 0, -- probe1
  '},{"n":"', 'Food Probe1', '","c":', 0, ',"a":', 0, -- probe2
  '},{"n":"', 'Food Probe2', '","c":', 0, ',"a":', 0, -- probe3
  '},{"n":"', 'Ambient', '","c":', 0, ',"a":', 0, -- probe4
  '}],"set":', 0,
  ',"lid":', 0,
  ',"fan":{"c":', 0, ',"a":', 0, '}}'
}
local JSON_FROM_CSV = {2, 28, 6, 8, 12, 14, 18, 20, 24, 26, 32, 34, 30 } 
  
function jsonWrite(vals)
  local i,v
  for i,v in ipairs(vals) do
    JSON_TEMPLATE[JSON_FROM_CSV[i]] = v  
  end
  return nixio.fs.writefile(JSON_FILE, table.concat(JSON_TEMPLATE))
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
local lastUpdate = os.time()
while true do
  hmline = hm:read("*l") 
  if hmline == nil then break end
  
  if hmline:sub(1,2) ~= "OK" then
    local vals = {}
    for i in hmline:gmatch("[0-9.]+") do
      vals[#vals+1] = i
    end
    
    if #vals == 12 then 
      -- If the time has shifted more than 24 hours since the last update
      -- the clock has probably just been set from 0 (at boot) to actual 
      -- time. Recreate the rrd to prevent a 40 year long graph
      local time = os.time()
      if time - lastUpdate > (24*60*60) then
        nixio.syslog("notice", "Time jumped forward by "..(time-lastUpdate)..", restarting database")
        rrdCreate()
      end
      lastUpadte = time

      -- Add the time as the first item
      table.insert(vals, 1, time)
      
      -- vals[9] = gcinfo()
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
  end
end

hm:close()
nixio.fs.unlink("/var/run/linkmeterd/pid")

