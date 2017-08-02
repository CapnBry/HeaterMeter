module("linkmeter.peaks", package.seeall)

local HMMODE_UNPLUG  = 1
local HMMODE_STARTUP = 2
local HMMODE_NORMAL  = 3
local HMMODE_LID     = 4
local HMMODE_RECOVER = 5

-- Last known setpoint, used to detect setpoint change
local hmSetPoint
-- Last n pit temperature measurements
local pitLog
-- Peaks state (published)
local peaks

local function resetUpdateCount()
  peaks.updatecnt = {}
  for i = HMMODE_UNPLUG, HMMODE_RECOVER do
    peaks.updatecnt[i] = 0
  end
end

local function initState()
  peaks = {
    C = { trend = 0 },
    H = { trend = 1 },
    L = { trend = -1 },

    -- Current detected mode, one of HMMODE_xxx
    mode = nil,

    -- Histogram of PID output percentage (NORMAL mode only) [1-26] = 0%-100%
    outhist = nil,

    -- Number of updates seen in each mode
    updatecnt = nil
  }
  resetUpdateCount()

  hmSetPoint = nil
  pitLog = {}
end

local function log(...)
  nixio.syslog("info", ...)
  --print(...)
end

local function hmModeChange(newMode)
  peaks.mode = newMode
  --log("newMode=" .. peaks.mode)
end

local function updateHmMode(vals)
  local newMode = peaks.mode
  
  -- If pidOutput(1) is off or Pit probe(2) unplugged
  if vals[1] == "U" or vals[2] == "U" then
    newMode = HMMODE_UNPLUG
    
  -- new SetPoint? back to startup mode
  elseif vals[1] ~= hmSetPoint then
    newMode = HMMODE_STARTUP

  -- If the lid timer is going then it is lid mode
  elseif vals[8] ~= "0" then
    newMode = HMMODE_LID

  -- If setpoint is manual mode ('-')
  -- or if Probe0 is above setpoint that is normal operation
  elseif vals[1] == "-" or vals[1] <= vals[2] then
    newMode = HMMODE_NORMAL

  -- if was lid mode and the timer expired, recover
  elseif peaks.mode == HMMODE_LID then
    newMode = HMMODE_RECOVER
  end
  
  if peaks.mode ~= newMode then hmModeChange(newMode) end
end

local function updateOutputHist(output)
  output = tonumber(output)
  if not peaks.outhist then
    peaks.outhist = {}
    -- presize array
    for i = 1, 26 do
      peaks.outhist[i] = 0
    end
  end
  
  output = math.ceil(output / 4) + 1
  peaks.outhist[output] = peaks.outhist[output] + 1
end

local function setPeakCurr(now, t)
  peaks.C.time = now
  peaks.C.val = t
end

local function peakStr(self)
  return string.format("@%u=%.1f half=%d per=%d amp=%.1f gain=%.1f",
    self.time, self.val, self.half or 0, self.period or 0, self.amp or 0, self.gain or 0)
end

local function peakChange(newTrend)
  local halfPeriod, amp, period, gain
  local halfref, fullref
  -- Halfref is the last opposite peak (high to low, or low to high)
  -- Fullref is the last peak of the same type (high to high, low to low)
  if peaks.C.trend > 0 then
    halfref = peaks.L
    fullref = peaks.H
  elseif peaks.C.trend < 0 then
    halfref = peaks.H
    fullref = peaks.L
  end
  
  if halfref.time then
    halfPeriod = peaks.C.time - halfref.time
    -- amplitude of the low to high or high to low
    amp = peaks.C.val - halfref.val
  end
  if fullref.time then
    period = peaks.C.time - fullref.time
    -- gain, how much the peak has changed from the last peak
    gain = peaks.C.val - fullref.val
  end
  
  -- Save the new peak values
  fullref.time = peaks.C.time
  fullref.val = peaks.C.val
  fullref.half = halfPeriod
  fullref.period = period
  fullref.amp = amp
  fullref.gain = gain
  
  pitLog = { fullref.val }

  log(("New %d peak %s"):format(peaks.C.trend, peakStr(fullref)))
  local Ku = tonumber(linkmeterd.getConf('pidp') or 1)
  if period then
    local p = 0.6 * Ku
    local i, d = 2 * p / period, p * period / 8 / 30
    log(("  Ziegler P=%.1f I=%.3f D=%.1f"):format(p, i, d))
    --p, i, d = Ku / 2.2, Ku / (2.2 * period), Ku * (period / 6.3) / 30
    --log(("  TLC Rules P=%.1f I=%.3f D=%.1f"):format(p, i, d))
  end
  
  peaks.C.trend = newTrend
end

local function peakDetect(now, t)
  local newTrend = 0
  for i = #pitLog,1,-1 do
    local diff = t - pitLog[i]
    if diff > 1.0 then
      newTrend = 1
      --log(("Trend found at %d/%d"):format(i, #pitLog))
      break
    end
    if diff < -1.0 then
      newTrend = -1
      --log(("Trend found at %d/%d"):format(i, #pitLog))
      break
    end
  end
  
  --log(("Trend=%d newtrend=%d t=%.1f"):format(peaks.C.trend, newTrend, t))
  if peaks.C.trend == 0 then
    if newTrend ~= 0 then
      peaks.C.trend = newTrend
      setPeakCurr(now, t)
    end
  elseif peaks.C.trend > 0 then
    if t > peaks.C.val then
      setPeakCurr(now, t)
    elseif newTrend < 0 then
      peakChange(newTrend)
    end
  elseif peaks.C.trend < 0 then
    if t < peaks.C.val then
      setPeakCurr(now, t)
    elseif newTrend > 0 then
      peakChange(newTrend)
    end
  end
end

local pitLogPeriodCnt = 0
function updatePitLog(t)
  if pitLogPeriodCnt == 0 then
    pitLog[#pitLog+1] = t
    if #pitLog > 180 then table.remove(pitLog, 1) end
    pitLogPeriodCnt = 10

    linkmeterd.publishBroadcastMessage("peaks", peaks)
  else
    pitLogPeriodCnt = pitLogPeriodCnt - 1
  end
end

local function updateState(now, vals)
  -- SetPoint, Probe0, Probe1, Probe2, Probe3, Output, OutputAvg, Lid, Fan
  -- 1         2       3       4       5       6       7          8    9
  updateHmMode(vals)
  if peaks.mode == HMMODE_UNPLUG then return end
  
  peaks.updatecnt[peaks.mode] = peaks.updatecnt[peaks.mode] + 1
  updatePitLog(tonumber(vals[2]))
  peakDetect(now, tonumber(vals[2]))

  if peaks.mode == HMMODE_NORMAL then
    updateOutputHist(vals[6])
  end
  
  hmSetPoint = vals[1]
end

local function dumpState(line)
  local f = {}
  f[#f+1] = 'Histogram=' .. table.concat(peaks.outhist or {}, ',')
  f[#f+1] = 'peaks.mode=' .. peaks.mode
  f[#f+1] = 'peaks.updatecnt=' .. table.concat(peaks.updatecnt, ',')
  if peaks.C.time then f[#f+1] = ("peaks.C (%02d) %s"):format(peaks.C.trend, peakStr(peaks.C)) end
  if peaks.H.time then f[#f+1] = ("peaks.H  %s"):format(peakStr(peaks.H)) end
  if peaks.L.time then f[#f+1] = ("peaks.L  %s"):format(peakStr(peaks.L)) end
  return table.concat(f, '\n')
end

function init()
  initState()

  linkmeterd.registerStatusListener(updateState)
  linkmeterd.registerSegmentListener("$LMDS", dumpState)
end
