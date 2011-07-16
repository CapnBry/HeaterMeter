local os = require "os"
local rrd = require "rrd" 
local nixio = require "nixio" 
      nixio.fs = require "nixio.fs" 
      nixio.util = require "nixio.util" 
local uci = require "uci" 
local lucid = require "luci.lucid"

local pairs, ipairs, table, tonumber = pairs, ipairs, table, tonumber

module "luci.lucid.linkmeterd"

local serialPolle
local lastHmUpdate
local rfMap = {}
local rfStatus = {}

local segmentCall -- forward

local SERIAL_DEVICE = "/dev/ttyS1"
local RRD_FILE = "/tmp/hm.rrd"

local function rrdCreate()
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

local function jsonWrite(vals)
  local i,v
  for i,v in ipairs(vals) do
    if (tonumber(v) == nil) then v = "null" end
    JSON_TEMPLATE[JSON_FROM_CSV[i]] = v
  end

  -- add the rf status where applicable
  for i,src in ipairs(rfMap) do
    local rfval
    if (src ~= "") then
      local sts = rfStatus[src];
      if sts then
        rfval = (',"rf":{"s":%s,"b":%s}'):format(sts.rssi,sts.batt);
      else
        rfval = ',"rf":null';
      end
    else
      rfval = ''
    end
    JSON_TEMPLATE[10 + (i * 5)] = rfval
  end
end

local function segSplit(line)
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

local function segProbeNames(line)
  local vals = segSplit(line)
  if #vals < 4 then return end

  JSON_TEMPLATE[12] = vals[1]
  JSON_TEMPLATE[17] = vals[2]
  JSON_TEMPLATE[22] = vals[3]
  JSON_TEMPLATE[27] = vals[4]
end

local function segRfUpdate(line)
  local vals = segSplit(line)
  rfStatus = {}  -- clear the table to remove stales
  local idx = 1
  while (idx < #vals) do
    local nodeId = vals[idx]
    rfStatus[nodeId] = {
      batt = vals[idx+1],
      rssi = vals[idx+2],
      last = vals[idx+3]
    }
    idx = idx + 4
  end
end
                                                                      
local function segRfMap(line)
  local vals = segSplit(line)
  local idx
  for i,s in ipairs(vals) do
    rfMap[i] = s:sub(1,1)
  end
end

function segStateUpdate(line)
    local vals = segSplit(line)

    if #vals == 8 then
      -- If the time has shifted more than 24 hours since the last update
      -- the clock has probably just been set from 0 (at boot) to actual
      -- time. Recreate the rrd to prevent a 40 year long graph
      local time = os.time()
      if time - lastHmUpdate > (24*60*60) then
        nixio.syslog("notice", 
          "Time jumped forward by "..(time-lastHmUpdate)..", restarting database")
        rrdCreate()
      end
      lastHmUpdate = time

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
      -- if rfStatus.B then vals[5] = rfStatus.B.batt / 10 end

      rrd.update(RRD_FILE, table.concat(vals, ":"))
    end
end

local function lmdStart()
  if serialPolle then return true end

  local serialfd = nixio.open(SERIAL_DEVICE, nixio.open_flags("rdwr"))
  if not serialfd then
    return nil, -2, "Can't open serial device"
  end
  serialfd:setblocking(false) 

  lastHmUpdate = os.time()
  nixio.umask("0022")
  -- Create database
  if not nixio.fs.access(RRD_FILE) then
    rrdCreate()
  end

  serialPolle = {
    fd = serialfd,
    events = nixio.poll_flags("in"),
    revents = 0,
    handler = function (polle)
      for line in polle.fd:linesource() do
        segmentCall(line)
      end 
    end
  }
  
  lucid.register_pollfd(serialPolle)
  serialfd:write("/config\n")
  
  return true
end

local function lmdStop()
  if not serialPolle then return true end
  lucid.unregister_pollfd(serialPolle)
  serialPolle.fd:setblocking(true)
  serialPolle.fd:close()
  serialPolle = nil
  
  return true
end

local function segLmRfStatus(line)
  local retVal = ""
  for id, item in pairs(rfStatus) do
    if retVal ~= "" then 
      retVal = retVal .. ","
    end
    
    retVal = retVal .. ('{"id":%s,"batt":%s,"rssi":%s,"last":%s}'):format(
      id, item.batt, item.rssi, item.last)
  end
  retVal = "[" .. retVal .. "]"
  
  return retVal
end

local function segLmDaemonStart()
  lmdStart()
  return "OK"
end

local function segLmDaemonStop()
  lmdStop()
  return "OK"
end

local function segLmStateUpdate()
  return table.concat(JSON_TEMPLATE)
end

local segmentMap = {
  ["$HMSU"] = segStateUpdate,
  ["$HMPN"] = segProbeNames,
  ["$HMRF"] = segRfUpdate,
  ["$HMRM"] = segRfMap,

  ["$LMSU"] = segLmStateUpdate,
  ["$LMRF"] = segLmRfStatus,
  ["$LMD1"] = segLmDaemonStart,
  ["$LMD0"] = segLmDaemonStop,
}

function segmentCall(line)
  local segmentFunc = segmentMap[line:sub(1,5)]
  if segmentFunc then 
    return segmentFunc(line)
  else
    return "ERR"
  end
end

function prepare_daemon(config, server)
  nixio.syslog("info", "Preparing LinkMeter daemon")
  
  local ipcfd = nixio.socket("unix", "dgram")
  if not ipcfd then
    return nil, -2, "Can't create IPC socket"
  end
 
  nixio.fs.unlink("/var/run/linkmeter.sock")
  ipcfd:bind("/var/run/linkmeter.sock")
  ipcfd:setblocking(false) 
 
  server.register_pollfd({
    fd = ipcfd,
    events = nixio.poll_flags("in"),
    revents = 0,
    handler = function (polle)
      while true do
      local msg, addr = polle.fd:recvfrom(128)
      if not msg and addr then return end
   
      local result = segmentCall(msg)
      if result then polle.fd:sendto(result, addr) end
      end
    end
  }) 
  
  return lmdStart()
end
        
