local os = require "os"
local rrd = require "rrd" 
local sys = require "luci.sys"
local nixio = require "nixio" 
      nixio.fs = require "nixio.fs" 
      nixio.util = require "nixio.util" 
local uci = require "uci"
local lucid = require "luci.lucid"
local lmfit, t = pcall(require,"lmfit"); lmfit = lmfit and t

local pairs, ipairs, table, pcall, type = pairs, ipairs, table, pcall, type
local tonumber, tostring, print, next, io = tonumber, tostring, print, next, io
local collectgarbage, math, bxor = collectgarbage, math, nixio.bit.bxor

module "luci.lucid.linkmeterd"

local serialPolle
local statusListeners = {}
local lastHmUpdate
local lastAutoback
local autobackActivePeriod
local autobackInactivePeriod
local unkProbe

local rfMap = {}
local rfStatus = {}
local hmAlarms = {}
local hmConfig

-- forwards
local segmentCall

local RRD_FILE = uci.cursor():get("lucid", "linkmeter", "rrd_file")
local RRD_AUTOBACK = "/root/autobackup.rrd"
-- Must match recv size in lmclient if messages exceed this size
local LMCLIENT_BUFSIZE = 8192

local function rrdCreate()
  local status, last = pcall(rrd.last, RRD_AUTOBACK)
  if status then
    last = tonumber(last)
    if last and last <= os.time() then
      return nixio.fs.copy(RRD_AUTOBACK, RRD_FILE)
    end
  else
    nixio.syslog("err", "RRD last failed:"..last)
  end

 return rrd.create(
   RRD_FILE,
   "--step", "2",  --Time entry every 2 seconds
   "DS:sp:GAUGE:30:0:1000",  --data source named SP, Gauge type, expects every 30 seconds, no extra wait, min of 0, max of 1000
   "DS:t0:GAUGE:30:0:1000",  -- temp0
   "DS:t1:GAUGE:30:0:1000",  -- temp1
   "DS:t2:GAUGE:30:0:1000",   --temp2
   "DS:t3:GAUGE:30:0:1000",  --temp3
   "DS:co:GAUGE:30:-1000:100",  --Controller Output. IE - PidOutput
   "DS:fn:GAUGE:30:0:100",    -- Fan Speed
   "RRA:AVERAGE:0.6:5:360",   --Archive, Average type, ,5 PDP averaged into one row, 360 rows archived - Each record is 10 seconds( PDP * 5). 1 hour
   "RRA:AVERAGE:0.6:30:360",  --Archive, average type, ,30 CDP averaged into one row, 360 rows archived - Each record is 1 min ( PDP * 30). 6 hours
   "RRA:AVERAGE:0.6:60:360",  --Archive, Average type, ,60 CDP averaged into one row, 360 rows archived  - Each recod is 2 min ( PDP * 60). 12 hours
   "RRA:AVERAGE:0.6:90:480"   --Archive, Average type, ,90 CDP averaged into one row, 480 rows archived - Each record is 3 mins ( PDP * 90). 24 hours
 )
end

-- This might look a big hokey but rather than build the string
-- and discard it every time, just replace the values to reduce
-- the load on the garbage collector
local JSON_TEMPLATE_SRC = {
  '', -- 1
  '{"time":', 0, -- 3
  ',"set":', 0,  -- 5
  ',"lid":', 0,  -- 7
  ',"fan":{"c":', 0, ',"a":', 0, ',"f":', 0, -- 13
  '},"adc":[', '', -- 15
  '],"temps":[{"n":"', 'Pit', '","c":', 0, '', ',"dph":', 'null', -- 22
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 28
  '}},{"n":"', 'Food Probe1', '","c":', 0, '', ',"dph":', 'null', -- 35
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 41
  '}},{"n":"', 'Food Probe2', '","c":', 0, '', ',"dph":', 'null', -- 48
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 54
  '}},{"n":"', 'Ambient', '","c":', 0, '', ',"dph":', 'null', -- 61
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 67
  '}}]}', -- 68
  '' -- 69
}
local JSON_TEMPLATE
local JSON_FROM_CSV = {3, 5, 19, 32, 45, 58, 9, 11, 7, 13 }

local function jsonWrite(vals)
  local i,v
  for i,v in ipairs(vals) do
    if tonumber(v) == nil then v = "null" end
    JSON_TEMPLATE[JSON_FROM_CSV[i]] = v
  end

  -- add the rfstatus where applicable
  for i,src in ipairs(rfMap) do
    local rfval
    if src ~= "" then
      local sts = rfStatus[src]
      if sts then
        rfval = (',"rf":{"s":%d,"b":%d}'):format(sts.rssi,sts.lobatt)
      else
        rfval = ',"rf":null'
      end
    else
      rfval = ''
    end
    JSON_TEMPLATE[7+(i*13)] = rfval
  end
end

local function broadcastStatus(fn)
  local o
  local i = 1
  while i <= #statusListeners do
    if not o then 
      o = fn()
    end
    
    if not statusListeners[i](o) then
      table.remove(statusListeners, i)
    else
      i = i + 1
    end
  end
end

local function buildConfigMap()
  if not hmConfig then return {} end

  local r = {}
  for k,v in pairs(hmConfig) do
    r[k] = v
  end
  
  if JSON_TEMPLATE[3] ~= 0 then
    -- Current temperatures
    for i = 0, 3 do
      r["pcurr"..i] = tonumber(JSON_TEMPLATE[19+(i*13)])
    end
    -- Setpoint
    r["sp"] = JSON_TEMPLATE[5]
  end
 
  for i,v in ipairs(hmAlarms) do
    i = i - 1
    local idx = math.floor(i/2)
    local aType = (i % 2 == 0) and "l" or "h"
    r["pal"..aType..idx] = tonumber(v.t)
  end
  
  return r
end

local function segSplit(line)
  local retVal = {}
  local fieldstart = 1
  while true do
    local nexti = line:find(',', fieldstart)
    if nexti then
      -- Don't add the segment name
      if fieldstart > 1 then
        retVal[#retVal+1] = line:sub(fieldstart, nexti - 1)
      end
      fieldstart = nexti + 1
    else
      if fieldstart > 1 then
        retVal[#retVal+1] = line:sub(fieldstart)
      end
      break
    end
  end

  return retVal
end

local lastLogMessage
local function stsLogMessage()
  local vals = segSplit(lastLogMessage)
  return ('event: log\ndata: {"msg": "%s"}\n\n'):format(vals[1])
end

local function segLogMessage(line)
  lastLogMessage = line
  broadcastStatus(stsLogMessage)
end

local lastPidInternals
local function stsPidInternals()
  local vals = segSplit(lastPidInternals)
  return ('event: pidint\ndata: {"b":%s,"p":%s,"i":%s,"d":%s,"t":%s}\n\n')
    :format(vals[1], vals[2], vals[3], vals[4], vals[5])
end

local function segPidInternals(line)
  lastPidInternals = line
  broadcastStatus(stsPidInternals)
end
          
local function segConfig(line, names, numeric)
  local vals = segSplit(line)
  for i, v in ipairs(names) do
    if i > #vals then break end

    if v ~= "" then
      if numeric then
        hmConfig[v] = tonumber(vals[i])
      else
        hmConfig[v] = vals[i]
      end
    end
  end
  return vals
end

local function segProbeNames(line)
  local vals = segConfig(line, {"pn0", "pn1", "pn2", "pn3"})
 
  for i,v in ipairs(vals) do
    JSON_TEMPLATE[4+i*13] = v
  end
end

local function segProbeOffsets(line)
  return segConfig(line, {"po0", "po1", "po2", "po3"}, true)
end

local function segPidParams(line)
  return segConfig(line, {"pidb", "pidp", "pidi", "pidd"}, true)
end

local function segLidParams(line)
  return segConfig(line, {"lo", "ld"}, true)
end

local function segFanParams(line)
  return segConfig(line, {"fmin", "fmax", "smin", "smax", "oflag", "fsmax"}, true)
end

local function segProbeCoeffs(line)
  local i = line:sub(7, 7)
  segConfig(line, {"", "pca"..i, "pcb"..i, "pcc"..i, "pcr"..i, "pt"..i}, false)
  -- The resistance looks better in the UI without scientific notation
  hmConfig["pcr"..i] = tonumber(hmConfig["pcr"..i])
end

local function segLcdBacklight(line)
  return segConfig(line, {"lb", "lbn", "le0", "le1", "le2", "le3"})
end

local function segRfUpdate(line)
  local vals = segSplit(line)
  rfStatus = {}  -- clear the table to remove stales
  local idx = 1
  --local now = os.time()
  local band = nixio.bit.band
  while (idx < #vals) do
    local nodeId = vals[idx]
    local flags = tonumber(vals[idx+1])
    rfStatus[nodeId] = {
      lobatt = band(flags, 0x01) == 0 and 0 or 1,
      reset = band(flags, 0x02) == 0 and 0 or 1,
      native = band(flags, 0x04) == 0 and 0 or 1,
      rssi = vals[idx+2]
    }
    
    -- If this isn't the NONE source, save the stats as the ANY source
    if nodeId ~= "255" then
      rfStatus["127"] = rfStatus[nodeId]
    end
    
    idx = idx + 3
  end
end

local function segRfMap(line)
  local vals = segSplit(line)
  rfMap = {}
  for i,s in ipairs(vals) do
    rfMap[i] = s
    hmConfig["prfn"..(i-1)] = s
  end
end

local function segUcIdentifier(line)
  local vals = segSplit(line)
  if #vals > 1 then
    hmConfig.ucid = vals[2]
  end
end

local function setStateUpdateUnk(vals)
  if vals[2] == 'U' or vals[3] == 'U' then return end
  local t = tonumber(vals[2])
  local r = math.floor(vals[3])
  unkProbe[t] = r
end

function stsLmStateUpdate()
  JSON_TEMPLATE[1] = "event: hmstatus\ndata: "
  JSON_TEMPLATE[#JSON_TEMPLATE] = "\n\n"
  return table.concat(JSON_TEMPLATE)
end

local function postDeviceData(dd)
  if uci.cursor():get("linkmeter", "live", "optout") == "1" then
    return
  end
  cpuinfo = {}
  for line in io.lines("/proc/cpuinfo") do
    local iPos = line:find(":")
    if iPos then
      cpuinfo[line:sub(1, iPos-1):gsub("%s+$", "")] = line:sub(iPos+1):gsub("^%s+","")
    end
  end

  local hardware, revision, serial
  if cpuinfo['Hardware'] == 'BCM2708' then
    hardware = cpuinfo['Hardware']
    revision = cpuinfo['Revision']
    serial = cpuinfo['Serial']
  elseif cpuinfo['system type'] == 'Ralink SoC' then
    hardware = cpuinfo['system type']
    revision = cpuinfo['cpu model']
    serial = ""
  end

  local uptime = sys.uptime()
  local hostname = sys.hostname()
  dd = dd ..
    ('],"serial":"%s","revision":"%s","model":"%s","uptime":%s,"hostname":"%s"}'):format(
    serial, revision, hardware, uptime, hostname)

  os.execute(("curl --silent -o /dev/null -d devicedata='%s' %s &"):format(
   dd, 'http://heatermeter.com/devices/'));
end

local lastIpCheck
local lastIp
local lastIfaceHash
local lastIfaceHashTime
local function checkIpUpdate()
  local newIp
  local ifaces = nixio.getifaddrs()
  local packets = {}
  -- First find out how many packets have been sent on each interface
  -- from the packet family instance of each adapter
  for _,v in ipairs(ifaces) do
    if not v.flags.loopback and v.family == "packet" then
      packets[v.name] = v.data.tx_packets
    end
  end

  -- Static interfaces are always 'up' just not 'running' but nixio does not
  -- have a flag for running so look for an interface that has sent packets
  local ifaceJson = '{"ifaces":['
  local ifaceHash = ""
  for _,v in ipairs(ifaces) do
    if not v.flags.loopback and v.flags.up and v.family == "inet" then
      if ifaceHash ~= "" then ifaceJson = ifaceJson .. "," end
      ifaceJson = ifaceJson ..('{"name":"%s","addr":"%s","cnt":%s}'):format(
        v.name, v.addr, packets[v.name])
      ifaceHash = ifaceHash .. v.name .. v.addr

      if packets[v.name] > 0 then
        newIp = v.addr
      end
    end
  end

  if newIp and newIp ~= lastIp and serialPolle then
    serialPolle.fd:write("/set?tt=Network Address,"..newIp.."\n")
    lastIp = newIp
  end

  local time = os.time()
  if ifaceHash ~= lastIfaceHash or time - lastIfaceHashTime > 3600 then
    lastIfaceHashTime = time
    lastIfaceHash = ifaceHash
    postDeviceData(ifaceJson)
  end
end

local function checkAutobackup(now, vals)
  -- vals is the last status update
  local pit = tonumber(vals[3])
  if (autobackActivePeriod ~= 0 and pit and
    now - lastAutoBackup > (autobackActivePeriod * 60)) or
    (autobackInactivePeriod ~= 0 and
    now - lastAutoBackup > (autobackInactivePeriod * 60)) then
    nixio.fs.copy(RRD_FILE, RRD_AUTOBACK)

    lastAutoBackup = now
  end
end

local lastDphUpdate = 0
local dphEstimates = { {0,0}, {0,0}, {0,0}, {0,0} }
local function checkDphUpdate(now)
  -- Update the degrees per hour estimates once every minute
  if not lmfit or (now - lastDphUpdate) < 60 then return end
  lastDphUpdate = now

  local step = 10
  local soff = 3600
  local last = math.floor(rrd.last(RRD_FILE)/step) * step
  start, step, _, data = rrd.fetch(RRD_FILE, "AVERAGE",
    "--end", now, "--start", now - soff, "-r", step)

  for probe = 1,4 do
    local x = {}
    local y = {}
    for n, dp in ipairs(data) do
      local val = dp[probe+1]
      if (val == val) then
        x[#x+1] = n / 360
        y[#y+1] = val
      end
    end

    local dph = "null"
    if #x > 180 then
      local ok, p, status, evals = pcall(lmfit.linear, x, y, dphEstimates[probe])
      if ok and status >= 1 and status <= 3 then
        dph = ("%.2f"):format(p[1])
        dphEstimates[probe] = p
        --print(("probe=%d m=%.3f b=%.3f evals=%d status=%d %s"):format(
        --  probe, p[1], p[2] or 0.0, evals, status, lmfit.message(status)))
      end
    end -- if #x

    JSON_TEMPLATE[9+(probe*13)] = dph
  end -- for probe
end

local lastStateUpdate
local spareUpdates
local skippedUpdates
local function unthrottleUpdates()
  -- Forces the next two segStateUpdate()s to be unthrottled, which
  -- can be used to make sure any data changed is pushed out to clients
  -- instead of being eaten by the throttle. Send 2 because the first one
  -- is likely to be mid-period already
  skippedUpdates = 99
  spareUpdates = 2
end

local function throttleUpdate(line)
  -- Max updates that can be sent in a row
  local MAX_SEQUENTIAL = 2
  -- Max updates that will be skipped in a row
  local MAX_THROTTLED = 4

  -- SLOW: If (line) is the same, only every fifth update
  -- NORMAL: If (line) is different, only every second update
  -- Exception: If (line) is different during a SLOW period, do not skip that line
  -- In:  A B C D E E E E F G H
  -- Out: A   C   E     E F   H
  if line == lastStateUpdate then
    if skippedUpdates >= 2 then
      if spareUpdates < (MAX_SEQUENTIAL-1) then
        spareUpdates = spareUpdates + 1
      end
    end
    if skippedUpdates < MAX_THROTTLED then
      skippedUpdates = skippedUpdates + 1
      return true
    end
  else
    if skippedUpdates == 0 then
      if spareUpdates == 0 then
        skippedUpdates = skippedUpdates + 1
        return true
      else
        spareUpdates = spareUpdates - 1
      end
    end
  end
  lastStateUpdate = line
  skippedUpdates = 0
end

local function segStateUpdate(line)
    if throttleUpdate(line) then return end
    local vals = segSplit(line)

    if #vals >= 8 then
      if unkProbe then setStateUpdateUnk(vals) end
      
      -- If the time has shifted more than 24 hours since the last update
      -- the clock has probably just been set from 0 (at boot) to actual
      -- time. Recreate the rrd to prevent a 40 year long graph
      local time = os.time()
      if time - lastHmUpdate > (24*60*60) then
        nixio.syslog("notice", 
          "Time jumped forward by "..(time-lastHmUpdate)..", restarting database")
        rrdCreate()
      elseif time == lastHmUpdate then
        -- RRD hates it when you try to insert two PDPs at the same timestamp
        return nixio.syslog("info", "Discarding duplicate update")
      end
      lastHmUpdate = time

      -- Add the time as the first item
      table.insert(vals, 1, time)

      -- if rfStatus.B then vals[4] = rfStatus.B.batt / 10 end
      -- vals[5] = collectgarbage("count") / 10
      jsonWrite(vals)

      local lid = tonumber(vals[9]) or 0
      -- If the lid value is non-zero, it replaces the output value
      if lid ~= 0 then
        vals[7] = -lid
      end
      --if #vals > 9 then table.remove(vals, 10) end -- fan
      table.remove(vals, 9) -- lid
      table.remove(vals, 8) -- output avg

      -- update() can throw an error if you try to insert something it
      -- doesn't like, which will take down the whole server, so just
      -- ignore any error
      local status, err = pcall(rrd.update, RRD_FILE, table.concat(vals, ":"))
      if not status then nixio.syslog("err", "RRD error: " .. err) end
      
      broadcastStatus(stsLmStateUpdate)
      checkAutobackup(time, vals)
      checkDphUpdate(time)
    end
end

local function broadcastAlarm(probeIdx, alarmType, thresh)
  local pname = JSON_TEMPLATE[17+(probeIdx*13)]
  local curTemp = JSON_TEMPLATE[19+(probeIdx*13)]
  local retVal
  
  if alarmType then
    nixio.syslog("warning", "Alarm "..probeIdx..alarmType.." started ringing")
    retVal = nixio.fork()
    if retVal == 0 then
      local cm = buildConfigMap()
      cm["ip"] = lastIp
      cm["al_probe"] = probeIdx
      cm["al_type"] = alarmType
      cm["al_thresh"] = thresh
      cm["al_prep"] = alarmType == "H" and "above" or "below"
      cm["pn"] = cm["pn"..probeIdx]
      cm["pcurr"] = cm["pcurr"..probeIdx]
      nixio.exece("/usr/share/linkmeter/alarm", {}, cm)
    end
    alarmType = '"'..alarmType..'"'
  else
    nixio.syslog("warning", "Alarm stopped")
    alarmType = "null"
    retVal = 0
  end

  unthrottleUpdates() -- force the next update
  JSON_TEMPLATE[28+(probeIdx*13)] = alarmType
  broadcastStatus(function ()
    return ('event: alarm\ndata: {"atype":%s,"p":%d,"pn":"%s","c":%s,"t":%s}\n\n'):format(
      alarmType, probeIdx, pname, curTemp, thresh)
    end)

  return retVal
end

local function segAlarmLimits(line)
  local vals = segSplit(line)
  
  for i,v in ipairs(vals) do
    -- make indexes 0-based
    local alarmId = i - 1
    local ringing = v:sub(-1)
    ringing = ringing == "H" or ringing == "L"
    if ringing then v = v:sub(1, -2) end
  
    local curr = hmAlarms[i] or {}
    local probeIdx = math.floor(alarmId/2)
    local alarmType = alarmId % 2
    JSON_TEMPLATE[24+(probeIdx*13)+(alarmType*2)] = v
    -- Wait until we at least have some config before broadcasting
    if (ringing and not curr.ringing) and (hmConfig and hmConfig.ucid) then
      curr.ringing = os.time()
      broadcastAlarm(probeIdx, (alarmType == 0) and "L" or "H", v)
    elseif not ringing and curr.ringing then
      curr.ringing = nil
      broadcastAlarm(probeIdx, nil, v)
    end
    curr.t = v
    
    hmAlarms[i] = curr
  end
end

local function segAdcRange(line)
  JSON_TEMPLATE[15] = line:sub(7)
end

local function segmentValidate(line)
  -- First character always has to be $
  if line:sub(1, 1) ~= "$" then return false end
  
  -- The line optionally ends with *XX hex checksum
  local _, _, csum = line:find("*(%x%x)$", -3)
  if csum then
    csum = tonumber(csum, 16)
    for i = 2, #line-3 do
      csum = bxor(csum, line:byte(i))
    end
   
    csum = csum == 0
    if not csum then
      nixio.syslog("warning", "Checksum failed: "..line)
      if hmConfig then hmConfig.cerr = (hmConfig.cerr or 0) + 1 end
    end
  end
 
  -- Returns nil if no checksum or true/false if checksum checks 
  return csum
end

local function serialHandler(polle)
  for line in polle.lines do
    local csumOk = segmentValidate(line)
    if csumOk ~= false then
      if hmConfig == nil then 
        hmConfig = {}
        serialPolle.fd:write("\n/config\n")
      end
 
      -- Remove the checksum of it was there
      if csumOk == true then line = line:sub(1, -4) end 
      segmentCall(line)
    end -- if validate
  end -- for line

end

local function lmclientSendTo(fd, addr, val)
  if #val <= LMCLIENT_BUFSIZE then
    fd:sendto(val, addr)
  else
    while val ~= "" do
      fd:sendto(val:sub(1, LMCLIENT_BUFSIZE), addr)
      val = val:sub(LMCLIENT_BUFSIZE)
    end
  end
end

local function initHmVars()
  hmConfig = nil
  lastIp = nil
  lastIpCheck = 0
  rfMap = {}
  rfStatus = {}
  hmAlarms = {}
  JSON_TEMPLATE = {}
  for _,v in pairs(JSON_TEMPLATE_SRC) do
    JSON_TEMPLATE[#JSON_TEMPLATE+1] = v
  end
  unthrottleUpdates()
end

local function lmdStart()
  if serialPolle then return true end
  local cfg = uci.cursor()
  local SERIAL_DEVICE = cfg:get("lucid", "linkmeter", "serial_device")
  local SERIAL_BAUD = cfg:get("lucid", "linkmeter", "serial_baud")
  autobackActivePeriod = tonumber(cfg:get("lucid", "linkmeter", "autoback_active")) or 0
  autobackInactivePeriod = tonumber(cfg:get("lucid", "linkmeter", "autoback_inactive")) or 0
  
  initHmVars() 
  if os.execute("/bin/stty -F " .. SERIAL_DEVICE .. " raw -echo " .. SERIAL_BAUD) ~= 0 then
    return nil, -2, "Can't set serial baud"
  end

  local serialfd = nixio.open(SERIAL_DEVICE, nixio.open_flags("rdwr"))
  if not serialfd then
    return nil, -2, "Can't open serial device"
  end
  serialfd:setblocking(false) 

  lastHmUpdate = os.time()
  lastAutoBackup = lastHmUpdate
  nixio.umask("0022")
  -- Create database
  if not nixio.fs.access(RRD_FILE) then
    rrdCreate()
  end

  serialPolle = {
    fd = serialfd,
    lines = serialfd:linesource(),
    events = nixio.poll_flags("in"),
    handler = serialHandler
  }
  
  lucid.register_pollfd(serialPolle)
  
  return true
end

local function lmdStop()
  if not serialPolle then return true end
  lucid.unregister_pollfd(serialPolle)
  serialPolle.fd:setblocking(true)
  serialPolle.fd:close()
  serialPolle = nil
  initHmVars()
  
  return true
end

local function segLmSet(line)
  if not serialPolle then return "ERR" end
  -- Replace the $LMST,k,v with /set?k=v
  serialPolle.fd:write(line:gsub("^%$LMST,(%w+),(.*)", "\n/set?%1=%2\n"))
  -- Let the next updates come immediately to make it seem more responsive
  unthrottleUpdates()
  return "OK"
end

local function segLmReboot(line)
  if not serialPolle then return "ERR" end
  serialPolle.fd:write("\n/reboot\n")
  -- Clear our cached config to request it again when reboot is complete
  initHmVars()
  return "OK"
end

local function segLmGet(line)
  local vals = segSplit(line)
  local retVal = {}
  for i = 1, #vals, 2 do
    retVal[#retVal+1] = hmConfig and hmConfig[vals[i]] or vals[i+1] or ""
  end
  return table.concat(retVal, '\n')
end

local function segLmRfStatus(line)
  local retVal = ""
  for id, item in pairs(rfStatus) do
    if retVal ~= "" then 
      retVal = retVal .. ","
    end
    
    retVal = retVal ..
      ('{"id":%s,"lobatt":%d,"rssi":%d,"reset":%d,"native":%d}'):format(
      id, item.lobatt, item.rssi, item.reset, item.native)
  end
  retVal = "[" .. retVal .. "]"
  
  return retVal
end

local function segLmDaemonControl(line)
  local vals = segSplit(line)
  -- Start the daemon if there is any non-zero parameter else stop it
  if #vals > 0 and vals[1] ~= "0" then
    lmdStart()
  else
    lmdStop()
  end
  return "OK"
end

local function segLmStateUpdate()
  JSON_TEMPLATE[1] = ""
  JSON_TEMPLATE[#JSON_TEMPLATE] = ""
  -- If the "time" field is still 0, we haven't gotten an update
  if JSON_TEMPLATE[3] == 0 then
    return "{}"
  else
    return table.concat(JSON_TEMPLATE)
  end
end

local function segLmConfig()
  local cm = buildConfigMap()
  local r = {}
  -- Make sure we have an entry for temperatures even if there isn't a value
  for i = 0, 3 do
    if not cm["pcurr"..i] then r[#r+1] = ('"pcurr%d":null'):format(i) end
  end
  
  for k,v in pairs(cm) do
    local s
    if type(v) == "number" then
      s = '%q:%s'
    else
      s = '%q:%q'
    end
    r[#r+1] = s:format(k,v)
  end
  
  return "{" .. table.concat(r, ',') .. "}"
end

local function unkProbeCurveFit()
  if not lmfit then return end
  local tt = {} -- table of temps
  local tr = {} -- table of resistances
  for k,v in pairs(unkProbe) do
    tr[#tr+1] = v
    tt[#tt+1] = k
  end

  local ok, p, status, evals = pcall(lmfit.steinhart, tr, tt)
  if ok then
    return
    ('{"a":%.7e,"b":%.7e,"c":%.7e,"n":%d,"e":%d,"status":%s,"message":"%s"}')
      :format(p[1], p[2], p[3], #tt, evals, status, lmfit.message(status))
  else
    return "ERR: " .. p
  end
end

local function unkProbeCsv()
  local r = { "C,R" }
  for k,v in pairs(unkProbe) do
    r[#r+1] = ("%.1f,%d"):format(k,v)
  end

  return table.concat(r, '\n')
end

local function segLmUnknownProbe(line)
  local vals = segSplit(line) 
  if vals[1] == "start" then
    unkProbe = {}
    if serialPolle then serialPolle.fd:write("/set?sp=0R\n") end
    return "OK"
  elseif vals[1] == "fit" and unkProbe then
    return unkProbeCurveFit()
  elseif vals[1] == "csv" and unkProbe then
    return unkProbeCsv()
  elseif vals[1] == "stop" and unkProbe then
    unkProbe = nil
    if serialPolle then serialPolle.fd:write("/reboot\n") end
    return "OK"
  else
    return "ERR"
  end
end

local function segLmAlarmTest(line)
  local vals = segSplit(line)
  if #vals > 0 then
    local probeIdx = tonumber(vals[1])
    local alarmType = vals[2] or ""
    local thresh = vals[3] or ""

    -- If alarmType is blank, use nil to simulate turn off
    alarmType = alarmType ~= "" and alarmType or nil

    -- If thresh is blank, use current
    thresh = thresh and tonumber(thresh) or
      math.floor(tonumber(JSON_TEMPLATE[19+(probeIdx*13)]) or 0)

    local pid = broadcastAlarm(probeIdx, alarmType, thresh)
    return "OK " .. pid
  else
    return "ERR"
  end
end

local function registerStreamingStatus(fn)
  statusListeners[#statusListeners + 1] = fn
end

local lmdStartTime
local function lmdTick()
  local time = os.time()
  if lastIp == nil or time - lastIpCheck > 60 then
    checkIpUpdate()
    lastIpCheck = time
  end

  if lmdStartTime and serialPolle and not hmConfig and time - lmdStartTime > 10 then
    lmdStartTime = nil -- prevent from running more than once
    nixio.syslog("warning", "No response from HeaterMeter, running avrupdate")
    lmdStop()
    if os.execute("/usr/bin/avrupdate -d") ~= 0 then
      nixio.syslog("err", "avrupdate failed")
    else
      nixio.syslog("info", "avrupdate OK")
    end
    lmdStart()
  end
end

local segmentMap = {
  ["$HMAL"] = segAlarmLimits,
  ["$HMAR"] = segAdcRange,
  ["$HMFN"] = segFanParams,
  ["$HMLB"] = segLcdBacklight,
  ["$HMLD"] = segLidParams,
  ["$HMLG"] = segLogMessage,
  ["$HMPC"] = segProbeCoeffs,
  ["$HMPD"] = segPidParams,
  ["$HMPN"] = segProbeNames,
  ["$HMPO"] = segProbeOffsets,
  ["$HMPS"] = segPidInternals,
  ["$HMRF"] = segRfUpdate,
  ["$HMRM"] = segRfMap,
  ["$HMSU"] = segStateUpdate,
  ["$UCID"] = segUcIdentifier,

  ["$LMAT"] = segLmAlarmTest,
  ["$LMGT"] = segLmGet,
  ["$LMST"] = segLmSet,
  ["$LMSU"] = segLmStateUpdate,
  ["$LMRB"] = segLmReboot,
  ["$LMRF"] = segLmRfStatus,
  ["$LMDC"] = segLmDaemonControl,
  ["$LMCF"] = segLmConfig,
  ["$LMUP"] = segLmUnknownProbe
  -- $LMSS
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
    handler = function (polle)
      while true do
        local msg, addr = polle.fd:recvfrom(128)
        if not (msg and addr) then return end

	if msg == "$LMSS" then
	  registerStreamingStatus(function (o) return polle.fd:sendto(o, addr) end)
	else
          lmclientSendTo(polle.fd, addr, segmentCall(msg))
        end
      end
    end
  }) 

  lmdStartTime = os.time()
  server.register_tick(lmdTick)
  
  return lmdStart()
end

