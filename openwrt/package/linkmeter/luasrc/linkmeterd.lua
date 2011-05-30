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

local rfMap = {}
local rfStatus = {}

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
  '{"time":', 0,
  ',"set":', 0,
  ',"lid":', 0,
  ',"fan":{"c":', 0, ',"a":', 0, 
  '},"temps":[{"n":"', 'Pit', '","c":', 0, '', -- probe1
  '},{"n":"', 'Food Probe1', '","c":', 0, '', -- probe2
  '},{"n":"', 'Food Probe2', '","c":', 0, '', -- probe3
  '},{"n":"', 'Ambient', '","c":', 0, '', -- probe4
  '}]}'
}
local JSON_FROM_CSV = {2, 4, 14, 19, 24, 29, 8, 10, 6 } 
  
function jsonWrite(vals)
  local i,v
  for i,v in ipairs(vals) do
    JSON_TEMPLATE[JSON_FROM_CSV[i]] = v  
  end
  
  -- add the rf status where applicable
  for i,src in ipairs(rfMap) do
    local rfval
    if (src ~= "") then
      rfval = ',"rf":' .. tostring(rfStatus[src] or 0)
    else
      rfval = ''
    end
    JSON_TEMPLATE[10 + (i * 5)] = rfval
  end
  
  return nixio.fs.writefile(JSON_FILE, table.concat(JSON_TEMPLATE))
end

function segSplit(line)
  local retVal = {}
  local fieldstart = 1
  line = line .. ','
  repeat
    local nexti = line:find(',', fieldstart)
    retVal[#retVal+1] = line:sub(fieldstart, nexti-1)
    fieldstart = nexti + 1
  until fieldstart > line:len()

  table.remove(retVal, 1) -- remove the segment name
  return retVal
end

function segProbeNames(line)
  local vals = segSplit(line)
  if #vals < 4 then return end

  JSON_TEMPLATE[12] = vals[1]
  JSON_TEMPLATE[17] = vals[2]
  JSON_TEMPLATE[22] = vals[3]
  JSON_TEMPLATE[27] = vals[4]
end

function segRfUpdate(line)
  local vals = segSplit(line)
  rfStatus = {}  -- clear the table to remove stales
  local idx = 1
  while (idx < #vals) do
    local nodeId = vals[idx]
    local signalLevel = vals[idx+1]
    local lastReceive = vals[idx+2]
    rfStatus[nodeId] = tonumber(signalLevel)
    idx = idx + 3
  end
end

function segRfMap(line)
  local vals = segSplit(line)
  local idx
  for i,s in ipairs(vals) do 
    rfMap[i] = s:sub(1,1)
  end
end

local lastUpdate = os.time()
function segStateUpdate(line)
    local vals = segSplit(line)
    
    if #vals == 8 then 
      -- If the time has shifted more than 24 hours since the last update
      -- the clock has probably just been set from 0 (at boot) to actual 
      -- time. Recreate the rrd to prevent a 40 year long graph
      local time = os.time()
      if time - lastUpdate > (24*60*60) then
        nixio.syslog("notice", "Time jumped forward by "..(time-lastUpdate)..", restarting database")
        rrdCreate()
      end
      lastUpdate = time

      -- Add the time as the first item
      table.insert(vals, 1, time)
      
      jsonWrite(vals)
      
      local lid = tonumber(vals[9]) or 0
      -- If the lid value is non-zero, it replaces the fan value
      if lid ~= 0 then
        vals[7] = -lid
      end
      table.remove(vals, 9) -- lid
      table.remove(vals, 8) -- fan avg
      
      rrd.update(RRD_FILE, table.concat(vals, ":"))
    end
end

local hm = io.open(SERIAL_DEVICE, "r+b")
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

local segmentMap = { 
  ["$HMSU"] = segStateUpdate,
  ["$HMPN"] = segProbeNames,
  ["$HMRF"] = segRfUpdate,
  ["$HMRM"] = segRfMap
}

-- Request current state
hm:write("/set?pnXXX\n")
hm:write("/set?rmXXX\n")

while true do
  local hmline = hm:read("*l") 
  if hmline == nil then break end
  print(hmline)
 
  local segmentFunc = segmentMap[hmline:sub(1,5)];
  if segmentFunc ~= nil then
    segmentFunc(hmline)
  end
end

hm:close()
nixio.fs.unlink("/var/run/linkmeterd/pid")

