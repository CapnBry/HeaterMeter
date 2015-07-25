module("luci.lucid.lmstats", package.seeall)    

local HMMODE_UNPLUG  = 1
local HMMODE_STARTUP = 2
local HMMODE_NORMAL  = 3
local HMMODE_LID     = 4
local HMMODE_RECOVER = 5

-- Current detected mode, one of HMMODE_xxx
local hmMode
-- Number of updates seen in each mode
local hmUpdateCount
-- Last known setpoint, used to detect setpoint change
local hmSetPoint
-- Histogram of PID output percentage (NORMAL mode only) [1-101] = 0%-100%
local hmOutputHistogram
-- Last n pit temperature measurements
local pitLog = {} 

local peakTrend = 0
local peakCurr = {}
local peakLastHigh = {}
local peakLastLow = {}

local function log(...)
  nixio.syslog("warning", ...)
  --print(...)
end

local function resetUpdateCount()
  hmUpdateCount = {}
  for i = HMMODE_UNPLUG, HMMODE_RECOVER do
    hmUpdateCount[i] = 0
  end
end

local function hmModeChange(newMode)
  hmMode = newMode
  log("newMode=" .. hmMode)
end

local function updateHmMode(vals)
  local newMode = hmMode
  
  if vals[2] == "U" then
    newMode = HMMODE_UNPLUG
    
  -- new SetPoint? back to startup mode
  elseif vals[1] ~= hmSetPoint then
    newMode = HMMODE_STARTUP

  -- If the lid timer is going then it is lid mode
  elseif vals[8] ~= "0" then
    newMode = HMMODE_LID

  -- if Probe0 is above setpoint that is normal operation
  elseif vals[1] <= vals[2] then
    newMode = HMMODE_NORMAL

  -- if was lid mode and the timer expired, recover
  elseif hmMode == HMMODE_LID then
    newMode = HMMODE_RECOVER
  end
  
  if hmMode ~= newMode then hmModeChange(newMode) end
end

local function updateOutputHist(output)
  output = tonumber(output)
  if not hmOutputHistogram then
    hmOutputHistogram = {}
    -- presize array
    for i = 1, 101 do
      hmOutputHistogram[i] = 0
    end
  end
  
  output = output + 1
  hmOutputHistogram[output] = hmOutputHistogram[output] + 1
end

local function setPeakCurr(t)
  peakCurr.time = os.time()
  peakCurr.val = t
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
  if peakTrend > 0 then
    halfref = peakLastLow 
    fullref = peakLastHigh
  elseif peakTrend < 0 then
    halfref = peakLastHigh
    fullref = peakLastLow
  end
  
  if halfref.time then
    halfPeriod = peakCurr.time - halfref.time
    -- amplitude of the low to high or high to low
    amp = peakCurr.val - halfref.val
  end
  if fullref.time then
    period = peakCurr.time - fullref.time
    -- gain, how much the peak has changed from the last peak
    gain = peakCurr.val - fullref.val
  end
  
  -- Save the new peak values
  fullref.time = peakCurr.time
  fullref.val = peakCurr.val
  fullref.half = halfPeriod
  fullref.period = period
  fullref.amp = amp
  fullref.gain = gain
  
  pitLog = { fullref.val }

  log(("New %d peak %s"):format(peakTrend, peakStr(fullref)))
  local Ku = tonumber(luci.lucid.linkmeterd.getConf('pidp') or 1)
  if period then
    local p = 0.6 * Ku
    local i, d = 2 * p / period, p * period / 8 / 30
    log(("  Ziegler P=%.1f I=%.3f D=%.1f"):format(p, i, d))
    --p, i, d = Ku / 2.2, Ku / (2.2 * period), Ku * (period / 6.3) / 30
    --log(("  TLC Rules P=%.1f I=%.3f D=%.1f"):format(p, i, d))
  end
  
  peakTrend = newTrend
end

local function peakDetect(t)
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
  
  --log(("Trend=%d newtrend=%d t=%.1f"):format(peakTrend, newTrend, t))
  if peakTrend == 0 then
    if newTrend ~= 0 then
      peakTrend = newTrend
      setPeakCurr(t)
    end
  elseif peakTrend > 0 then
    if t > peakCurr.val then
      setPeakCurr(t)
    elseif newTrend < 0 then
      peakChange(newTrend)
    end
  elseif peakTrend < 0 then
    if t < peakCurr.val then
      setPeakCurr(t)
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
  else
    pitLogPeriodCnt = pitLogPeriodCnt - 1
  end
end

function updateState(now, vals)
  if not hmUpdateCount then resetUpdateCount() end
  -- SetPoint, Probe0, Probe1, Probe2, Probe3, Output, OutputAvg, Lid, Fan
  -- 1         2       3       4       5       6       7          8    9
  updateHmMode(vals)
  if hmMode == HMMODE_UNPLUG then return end
  
  hmUpdateCount[hmMode] = hmUpdateCount[hmMode] + 1
  updatePitLog(tonumber(vals[2]))
  peakDetect(tonumber(vals[2]))

  if hmMode == HMMODE_NORMAL then
    updateOutputHist(vals[6])
  end
  
  hmSetPoint = vals[1]
end

function dumpState()
  local f = {}
  f[#f+1] = 'Histogram=' .. table.concat(hmOutputHistogram or {}, ',')
  f[#f+1] = 'hmMode=' .. hmMode
  f[#f+1] = 'hmUpdateCount=' .. table.concat(hmUpdateCount, ',')
  if peakCurr.time then f[#f+1] = ("peakCurr (%02d) %s"):format(peakTrend, peakStr(peakCurr)) end
  if peakLastHigh.time then f[#f+1] = ("peakLastHigh  %s"):format(peakStr(peakLastHigh)) end
  if peakLastLow.time then f[#f+1] = ("peakLastLow   %s"):format(peakStr(peakLastLow)) end
  return table.concat(f, '\n')
end
-- P=4,I=0.02,D=5 - Servo 1000-2000
