local os = require "os"
local rrd = require "rrd"
local sys = require "luci.sys"
local nixio = require "nixio"
      nixio.fs = require "nixio.fs"
      nixio.util = require "nixio.util"
local uci = require "uci"
local jsonc = require "luci.jsonc"
-- Plugins
local lmpeaks = require "linkmeter.peaks"
local lmunkprobe = require "linkmeter.unkprobe"
local lmdph = require "linkmeter.dph"
local lmramp = require "linkmeter.ramp"
local lmipwatch = require "linkmeter.ipwatch"

local pairs, ipairs, table, pcall, type = pairs, ipairs, table, pcall, type
local tonumber, tostring, print, next, io = tonumber, tostring, print, next, io
local collectgarbage, math, bxor = collectgarbage, math, nixio.bit.bxor

module "linkmeterd"

local serialPolle
local statusListeners = {}
local pluginStatusListeners = {}
local pluginSegmentListeners = {}
local lastHmUpdate
local lastAutoback
local autobackActivePeriod
local autobackInactivePeriod

local rfMap = {}
local rfStatus = {}
local hmAlarms = {}
local extendedStatusProbeVals = {} -- array[probeIdx] of tables of kv pairs, per probe
local extendedStatusVals = {} -- standard kv pairs of items to include in status, global
local hmConfig

-- forwards
local segmentCall
local statusValChanged
local JSON_TEMPLATE
local broadcastStatus
local segLmToast

local RRD_FILE = uci.cursor():get("linkmeter", "daemon", "rrd_file")
local RRD_AUTOBACK = "/root/autobackup.rrd"
-- Must match recv size in lmclient if messages exceed this size
local LMCLIENT_BUFSIZE = 8192

-- Server Module
local Server = {
  _pollt = {},
  _tickt = {}
}

function Server.register_pollfd(polle)
  Server._pollt[#Server._pollt+1] = polle
end

function Server.unregister_pollfd(polle)
  for k, v in ipairs(Server._pollt) do
    if v == polle then
      table.remove(Server._pollt, k)
      return true
    end
  end
end

function Server.register_tick(ticke)
  Server._tickt[#Server._tickt+1] = ticke
end

function Server.unregister_tick(ticke)
  for k, v in ipairs(Server._tickt) do
    if v == ticke then
      table.remove(Server._tickt, k)
      return true
    end
  end
end

function Server.daemonize()
  if nixio.getppid() == 1 then return end

  local pid, code, msg = nixio.fork()
  if not pid then
    return nil, code, msg
  elseif pid > 0 then
    os.exit(0)
  end

  nixio.setsid()
  nixio.chdir("/")

  local devnull = nixio.open("/dev/null", nixio.open_flags("rdwr"))
  nixio.dup(devnull, nixio.stdin)
  nixio.dup(devnull, nixio.stdout)
  nixio.dup(devnull, nixio.stderr)

  return true
end

function Server.run()
  local lastTick
  while true do
    -- Listen for fd events, but break every 10s
    local stat, code = nixio.poll(Server._pollt, 10000)

    if stat and stat > 0 then
      for _, polle in ipairs(Server._pollt) do
        if polle.revents ~= 0 and polle.handler then
          polle.handler(polle)
        end
      end
    end

    local now = os.time()
    if now ~= lastTick then
      lastTick = now
      for _, cb in ipairs(Server._tickt) do
        cb(now)
      end
    end -- if new tick
  end -- while true
end

-- External API functions
function getConf(k, default)
  return hmConfig and hmConfig[k] or default
end

function setConf(k, v)
  if hmConfig then hmConfig[k] = v end
end

function toast(line1, line2)
  return segLmToast('$LMTT,' .. (line1 or '') .. ',' .. (line2 or ''))
end

function publishStatusVal(k, v, probeIdx)
  local t
  if probeIdx == nil or probeIdx < 0 then
    -- is a global status val
    probeIdx = nil
    t = extendedStatusVals
  else
    -- is a probe value, make sure there is storage available in extendedStatusProbeVals
    while #extendedStatusProbeVals < probeIdx do
      extendedStatusProbeVals[#extendedStatusProbeVals+1] = {}
    end
    t = extendedStatusProbeVals[probeIdx]
  end

  local newVals = statusValChanged(t, k, v)
  if newVals ~= nil then
    if probeIdx == nil then
      newVals = newVals == "" and "" or (newVals .. ",")
     JSON_TEMPLATE[15] = newVals
    else
      newVals = newVals == "" and "" or ("," .. newVals)
      JSON_TEMPLATE[9+(probeIdx*11)] = newVals
    end
  end -- newVals != nil
end

function publishBroadcastMessage(event, t)
  if type(t) == "table" then
    broadcastStatus(function () return ('event: %s\ndata: %s\n\n'):format(event, jsonc.stringify(t)) end)
  else
    broadcastStatus(function () return ('event: %s\ndata: %s\n\n'):format(event, t) end)
  end
end

function registerSegmentListener(seg, f)
  -- function (line) line is the whole string. Use segSplit to split
  if seg:sub(1, 1) ~= "$" then seg = "$" .. seg end
  unregisterSegmentListener(seg)
  pluginSegmentListeners[seg] = f
end

function unregisterSegmentListener(f)
  for i = 1, #pluginSegmentListeners do
    if pluginSegmentListeners[i] == f then
      table.remove(pluginSegmentListeners, i)
      return
    end
  end
end

function registerStatusListener(f)
  -- function (now, vals) vals = array of HMSU parameters
  unregisterStatusListener(f)
  pluginStatusListeners[#pluginStatusListeners+1] = f
end

function unregisterStatusListener(f)
  for i = 1, #pluginStatusListeners do
    if pluginStatusListeners[i] == f then
      table.remove(pluginStatusListeners, i)
      return
    end
  end
end

function registerTickListener(cb)
  Server.register_tick(cb)
end

function unregisterTickListener(cb)
  Server.unregister_tick(cb)
end

function hmWrite(s)
  if serialPolle then
    -- Break the data into 8 byte chunks (the size of the FIFO on the Pi3/W minuart)
    -- write always returns that it wrote the entire data, and poll('out') always times out
    for i=1, #s, 8 do
      serialPolle.fd:write(s:sub(i,i+7))
      nixio.nanosleep(0, serialPolle.bytens * 8)
    end
  end
end

function segSplit(line)
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

function statusValChanged(t, k, v)
  -- JSON encode the new value
  local tkv = {}
  tkv[k] = v
  local newVal = jsonc.stringify(tkv):sub(2, -2)
  newVal = newVal ~= "" and newVal or nil
  -- See if the value has changed
  local oldVal = t[k]
  if oldVal ~= newVal then
    -- Value changed, convert the k,v table to v only
    -- (because v is '"k": v' already and JSONifying it would give you "k": "k": v)
    t[k] = newVal
    local newt = {}
    for _,v in pairs(t) do
      newt[#newt+1] = v
    end
    return table.concat(newt, ",")
  end
end

local function rrdCreate()
  local status, last = pcall(rrd.last, RRD_AUTOBACK)
  if status then
    last = tonumber(last)
    --nixio.syslog("err", ("Autoback restore: last=%d now=%d"):format(last or -1, os.time()))
    if last and last <= os.time() then
      return nixio.fs.copy(RRD_AUTOBACK, RRD_FILE)
    end
  else
    nixio.syslog("err", "RRD last failed:"..last)
  end

  return rrd.create(
    RRD_FILE,
    "--step", "2",
    "DS:sp:GAUGE:30:0.1:1000",
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
local JSON_TEMPLATE_SRC = {
  '', -- 1
  '{"time":', 0, -- 3
  ',"set":', 0,  -- 5
  ',"lid":', 0,  -- 7
  ',"fan":{"c":', 0, ',"a":', 0, ',"f":', 0, -- 13
  '},', '', -- 15 (placeholder: extendedStatusVals)
  '"temps":[{"n":"', 'Pit',   '","c":', 0, '', -- 20 (placeholder: extendedStatusProbeVals)
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 26
  '}},{"n":"', 'Food Probe1', '","c":', 0, '', -- 31 (placeholder: extendedStatusProbeVals)
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 37
  '}},{"n":"', 'Food Probe2', '","c":', 0, '', -- 42 (placeholder: extendedStatusProbeVals)
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 48
  '}},{"n":"', 'Ambient',     '","c":', 0, '', -- 53 (placeholder: extendedStatusProbeVals)
    ',"a":{"l":', 'null', ',"h":', 'null', ',"r":', 'null', -- 59
  '}}]}', -- 60
  '' -- 61
}
local JSON_FROM_CSV = {3, 5, 19, 30, 41, 52, 9, 11, 7, 13 }

local function jsonWrite(vals)
  local i,v
  for i,v in ipairs(vals) do
    if tonumber(v) == nil then v = "null" end
    JSON_TEMPLATE[JSON_FROM_CSV[i]] = v
  end
end

function broadcastStatus(fn)
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

local function doStatusListeners(now, vals)
  for i = 1, #pluginStatusListeners do
    pluginStatusListeners[i](now, vals)
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
      r["pcurr"..i] = tonumber(JSON_TEMPLATE[19+(i*11)])
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
    JSON_TEMPLATE[6+i*11] = v
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
  return segConfig(line, {"fmin", "fmax", "smin", "smax", "oflag", "fsmax", "fflor", "sceil"}, true)
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

local function rfStatusRefresh()
  local rfval
  for i,src in ipairs(rfMap) do
    if src ~= "" then
      local sts = rfStatus[src]
      if sts then
        rfval = { s = sts.rssi, b = sts.lobatt }
      else
        rfval = 0; -- 0 indicates mapped but offline
      end
    else
      rfval = nil
    end
    publishStatusVal("rf", rfval, i)
  end
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
      rssi = tonumber(vals[idx+2])
    }

    -- If this isn't the NONE source, save the stats as the ANY source
    if nodeId ~= "255" then
      rfStatus["127"] = rfStatus[nodeId]
    end

    idx = idx + 3
  end
  rfStatusRefresh()
end

local function segRfMap(line)
  local vals = segSplit(line)
  rfMap = {}
  for i,s in ipairs(vals) do
    rfMap[i] = s
    hmConfig["prfn"..(i-1)] = s
  end
  rfStatusRefresh()
end

local function segResetConfig(line)
  toast("Resetting", "configuration...")
  os.execute("jffs2reset -y -r")
end

local function segUcIdentifier(line)
  local vals = segSplit(line)
  if #vals > 1 then
    hmConfig.ucid = vals[2]
  end
end

function stsLmStateUpdate()
  JSON_TEMPLATE[1] = "event: hmstatus\ndata: "
  JSON_TEMPLATE[#JSON_TEMPLATE] = "\n\n"
  return table.concat(JSON_TEMPLATE)
end

function segLmToast(line)
  local vals = segSplit(line)
  if serialPolle and #vals > 0 then
    hmWrite(("/set?tt=%s,%s\n"):format(vals[1],vals[2] or ""))
    return "OK"
  end

  return "ERR"
end

local function checkAutobackup(now, vals)
  -- vals is the last status update
  local pit = tonumber(vals[3])
  if (autobackActivePeriod ~= 0 and pit and
    now - lastAutoBackup > (autobackActivePeriod * 60)) or
    (autobackInactivePeriod ~= 0 and
    now - lastAutoBackup > (autobackInactivePeriod * 60)) then
    --nixio.syslog("err", ("Autobackup last=%d now=%d"):format(lastAutoBackup, now))
    nixio.fs.copy(RRD_FILE, RRD_AUTOBACK)

    lastAutoBackup = now
  end
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
    local vals = segSplit(line)
    local time = os.time()
    doStatusListeners(time, vals)

    if throttleUpdate(line) then return end

    if #vals >= 8 then
      -- If the time has shifted more than 24 hours since the last update
      -- the clock has probably just been set from 0 (at boot) to actual
      -- time. Recreate the rrd to prevent a 40 year long graph
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

      -- If setpoint is '-' that means manual mode
      -- and output is the manual setpoint
      if vals[2] == '-' then vals[2] = '-' .. vals[7] end

      jsonWrite(vals)

      local lid = tonumber(vals[9]) or 0
      -- If the lid value is non-zero, it replaces the output value
      if lid ~= 0 then
        vals[7] = -lid
      end
      if #vals > 9 then table.remove(vals, 10) end -- fan
      table.remove(vals, 9) -- lid
      table.remove(vals, 8) -- output avg

      -- update() can throw an error if you try to insert something it
      -- doesn't like, which will take down the whole server, so just
      -- ignore any error
      local status, err = pcall(rrd.update, RRD_FILE, table.concat(vals, ":"))
      if not status then nixio.syslog("err", "RRD error: " .. err) end

      broadcastStatus(stsLmStateUpdate)
      checkAutobackup(lastHmUpdate, vals)
    end
end

local function broadcastAlarm(probeIdx, alarmType, thresh)
  local pname = JSON_TEMPLATE[17+(probeIdx*11)]
  local curTemp = JSON_TEMPLATE[19+(probeIdx*11)]
  local retVal

  if alarmType then
    nixio.syslog("warning", "Alarm "..probeIdx..alarmType.." started ringing")
    retVal = nixio.fork()
    if retVal == 0 then
      local cm = buildConfigMap()
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
  JSON_TEMPLATE[26+(probeIdx*11)] = alarmType
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
    JSON_TEMPLATE[22+(probeIdx*11)+(alarmType*2)] = v
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
  local vals = segSplit(line)
  for i = 1, #vals do
    vals[i] = tonumber(vals[i])
  end
  publishStatusVal("adc", vals)
end

local function segmentValidate(line)
  -- First character always has to be $
  if line:sub(1, 1) ~= "$" then return false end

  -- The line optionally ends with *XX hex checksum
  local _, _, csum = line:find("*(%x%x)$", -3)
  if csum then
    csum = tonumber(csum, 16)
    for i = 2, #line-3 do
      local b = line:byte(i)
      -- If there is a null, force checksum invalid
      if b == 0 then b = 170 end
      csum = bxor(csum, b)
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
        hmWrite("\n/config\n")
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
  rfMap = {}
  rfStatus = {}
  hmAlarms = {}
  extendedStatusProbeVals = {}
  extendedStatusVals = {}
  pluginStatusListeners = {}
  pluginSegmentListeners = {}

  JSON_TEMPLATE = {}
  for _,v in pairs(JSON_TEMPLATE_SRC) do
    JSON_TEMPLATE[#JSON_TEMPLATE+1] = v
  end
  unthrottleUpdates()
end

local function initPlugins()
  lmunkprobe.init()
  lmpeaks.init()
  lmdph.init()
  lmramp.init()
  lmipwatch.init()
end

local function lmdStart()
  if serialPolle then return true end
  local cfg = uci.cursor()
  local SERIAL_DEVICE = cfg:get("linkmeter", "daemon", "serial_device") or "auto"
  local SERIAL_BAUD = cfg:get("linkmeter", "daemon", "serial_baud") or "38400"
  autobackActivePeriod = tonumber(cfg:get("linkmeter", "daemon", "autoback_active") or 0)
  autobackInactivePeriod = tonumber(cfg:get("linkmeter", "daemon", "autoback_inactive") or 0)

  if (SERIAL_DEVICE:lower() == "auto") then
    if nixio.fs.access("/dev/ttyS0") then
      SERIAL_DEVICE = "/dev/ttyS0"
    else
      SERIAL_DEVICE = "/dev/ttyAMA0"
    end
  end

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

  initPlugins()

  -- Attempt to flush the serial buffer
  -- otherwise when /config is sent it could overrun
  local discard
  repeat
    discard = serialfd:read(1024)
  until (not discard or #discard == 0)

  serialPolle = {
    fd = serialfd,
    lines = serialfd:linesource(),
    events = nixio.poll_flags("in"),
    handler = serialHandler,
    bytens = math.floor(1000 * 1000 * 1000 / SERIAL_BAUD * 10) -- time to transmit 1 byte in nanoseconds
  }
  Server.register_pollfd(serialPolle)

  return true
end

local function lmdStop()
  if not serialPolle then return true end
  Server.unregister_pollfd(serialPolle)
  serialPolle.fd:setblocking(true)
  serialPolle.fd:close()
  serialPolle = nil
  initHmVars()

  return true
end

local function segLmSet(line)
  if not serialPolle then return "ERR" end
  -- Replace the $LMST,k,v with /set?k=v
  hmWrite(line:gsub("^%$LMST,(%w+),(.*)", "\n/set?%1=%2\n"))
  -- Let the next updates come immediately to make it seem more responsive
  unthrottleUpdates()
  return "OK"
end

local function segLmReboot(line)
  if not serialPolle then return "ERR" end
  hmWrite("\n/reboot\n")
  -- Clear our cached config to request it again when reboot is complete
  initHmVars()
  initPlugins()
  return "OK"
end

local function segLmGet(line)
  local vals = segSplit(line)
  local cm = buildConfigMap()
  local retVal = {}
  for i = 1, #vals, 2 do
    retVal[#retVal+1] = cm[vals[i]] or vals[i+1] or ""
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
    if type(v) == "number" or v == "null" then
      s = '%q:%s'
    else
      s = '%q:%q'
    end
    r[#r+1] = s:format(k,v)
  end

  return "{" .. table.concat(r, ',') .. "}"
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
      math.floor(tonumber(JSON_TEMPLATE[19+(probeIdx*11)]) or 0)

    local pid = broadcastAlarm(probeIdx, alarmType, thresh)
    return "OK " .. pid
  else
    return "ERR"
  end
end

local function registerStreamingStatus(fn)
  statusListeners[#statusListeners + 1] = fn
  unthrottleUpdates() -- make sure the new client gets the next available update
end

local lmdStartTime
local function lmdTick(now)
  if lmdStartTime and serialPolle and now - lmdStartTime > 10 then
    unregisterTickListener(lmdTick) -- always stop checking after timeout
    if not hmConfig then
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
  ["$HMRC"] = segResetConfig,
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
  ["$LMTT"] = segLmToast,
  -- "$LMUP" -- unkprobe plugin
  -- "$LMDS" -- stats plugin

  -- $LMSS -- streaming status (lmclient request)
}

function segmentCall(line)
  local seg = line:sub(1,5)
  local segmentFunc = segmentMap[seg] or pluginSegmentListeners[seg]
  if segmentFunc then
    return segmentFunc(line)
  else
    return "ERR"
  end
end

local function prepare_daemon()
  local ipcfd = nixio.socket("unix", "dgram")
  if not ipcfd then
    return nil, -2, "Can't create IPC socket"
  end

  nixio.fs.unlink("/var/run/linkmeter.sock")
  ipcfd:bind("/var/run/linkmeter.sock")
  ipcfd:setblocking(false)

  Server.register_pollfd({
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
      end -- while true
    end -- handler
  })

  lmdStartTime = os.time()
  registerTickListener(lmdTick)

  return lmdStart()
end

--
-- linkmeterd service control functions
---
function start()
  local state = uci.cursor(nil, "/var/state")
  state:revert("linkmeter", "daemon")

  prepare_daemon()
  if uci.cursor():get("linkmeter", "daemon", "daemonize") == "1" then
    Server.daemonize()
  end

  state:set("linkmeter", "daemon", "pid", nixio.getpid())
  state:save("linkmeter")

  Server.run()
end

function running()
  local pid = tonumber(uci.cursor(nil, "/var/state"):get("linkmeter", "daemon", "pid"))
  return pid and nixio.kill(pid, 0) and pid
end

function stop()
  local pid = tonumber(uci.cursor(nil, "/var/state"):get("linkmeter", "daemon", "pid"))
  if pid then
    return nixio.kill(pid, nixio.const.SIGTERM)
  end
  return false
end
