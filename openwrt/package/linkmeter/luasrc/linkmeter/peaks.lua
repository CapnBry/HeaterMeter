module("linkmeter.peaks", package.seeall)

local HMMODE_UNPLUG  = 1
local HMMODE_STARTUP = 2
local HMMODE_NORMAL  = 3
local HMMODE_LID     = 4
local HMMODE_RECOVER = 5

local LIDMODE_AWAITING_LOW  = 1
local LIDMODE_AWAITING_HIGH = 2

-- Last known setpoint used to detect setpoint change
local hmSetpoint
-- History of pit temperature measurements (every 10th measurement, reset on peak)
local pitLog
-- Peaks state (published)
local peaks
local peaksUnitsScale
local peaksSetpointScale
local peakTrendThreshold
-- Extended lid mode tracking
local lidTrack

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

  hmSetpoint = nil
  pitLog = {}
  lidTrack = nil
  peaksUnitsScale = 1.0
  peaksSetpointScale = 0.1
  peakTrendThreshold = 1.0
end

local function log(...)
  nixio.syslog("info", ...)
  --print(...)
end

local function isLidTrackEnabled()
  local uci = require "uci"
  return uci.cursor():get("linkmeter", "daemon", "lidtrack_enabled") == "1"
end

local function peakStr(self)
  return string.format("@%u=%.1f half=%d per=%d amp=%.1f gain=%.1f",
    self.time, self.val, self.half or 0, self.period or 0, self.amp or 0, self.gain or 0)
end

local function trendStr(trend)
  if trend == 1 then
    return "HIGH"
  elseif trend == -1 then
    return "LOW"
  else
    return "NONE"
  end
end

local function modeStr(mode)
  if mode == HMMODE_UNPLUG then
    return "UNPLUG"
  elseif mode == HMMODE_STARTUP then
    return "STARTUP"
  elseif mode == HMMODE_NORMAL then
    return "NORMAL"
  elseif mode == HMMODE_LID then
    return "LID"
  elseif mode == HMMODE_RECOVER then
    return "RECOVER"
  else
    return "NONE"
  end
end

local function updateSetpointScaling()
  -- Adjust all thresholds by a Unit scale for Celsius users
  local units = linkmeterd.getConf("u") or "F"
  local setpoint = tonumber(hmSetpoint)
  if units == "C" or units == "R" then
    peaksUnitsScale = 5.0/9.0
    setpoint = (setpoint * 9.0/5.0) + 32
  else
    peaksUnitsScale = 1.0
  end

  -- Thesholds also need to be increased at higher temperatures
  if setpoint < 250 then
    peaksSetpointScale = 0.1
  elseif setpoint > 500 then
    peaksSetpointScale = 0.4
  else
    peaksSetpointScale = (0.3 * (setpoint - 250.0) / 250.0) + 0.1
  end
end

local function hmModeChange(newMode)
  log(("PeakDetect mode change %s=>%s"):format(modeStr(peaks.mode), modeStr(newMode)))

  if newMode == HMMODE_UNPLUG then
    -- Clear the entire state
    initState()

  elseif newMode == HMMODE_STARTUP then
    -- Reset the output histogram if the setpoint changes
    peaks.outhist = nil
    pitLog = {}
    updateSetpointScaling()

  -- Stop extended lid mode tracking when returning to normal mode or recovery
  elseif newMode == HMMODE_STARTUP or newMode == HMMODE_NORMAL or newMode == HMMODE_RECOVER then
    if lidTrack then
      log("Lid mode ended normally, disabling LidTrack")
      lidTrack = nil
    end

  elseif newMode == HMMODE_LID then
    lidTrack = LIDMODE_AWAITING_LOW
  end

  -- In lid mode, use a higher threshold for trend detection due to
  -- larger tempertature changes, targetting 1.0F at 225F
  if newMode == HMMODE_LID then
    peakTrendThreshold = 50.0 * peaksUnitsScale * peaksSetpointScale
  else
    peakTrendThreshold = 10.0 * peaksUnitsScale * peaksSetpointScale
  end

  peaks.mode = newMode
end

local function updateHmMode(vals)
  local newMode = peaks.mode
  local forceChange = false

  -- If pidOutput(1) is off or Pit probe(2) unplugged
  if vals[1] == "U" or vals[2] == "U" then
    newMode = HMMODE_UNPLUG

  -- new SetPoint? back to startup mode, always force a modeChange to update targets
  elseif vals[1] ~= hmSetpoint then
    newMode = HMMODE_STARTUP
    hmSetpoint = vals[1]
    forceChange = true

  -- If the lid timer is going then it is lid mode
  elseif vals[8] ~= "0" then
    newMode = HMMODE_LID

  -- If setpoint is manual mode ('-')
  -- or if Probe0 is above setpoint that is normal operation
  elseif vals[1] == "-" or tonumber(vals[1]) <= tonumber(vals[2]) then
    newMode = HMMODE_NORMAL

  -- if was lid mode and the timer expired, recover
  elseif peaks.mode == HMMODE_LID then
    newMode = HMMODE_RECOVER
  end

  if peaks.mode ~= newMode or forceChange then
    hmModeChange(newMode)
  end
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

  pitLog = { pitLog[#pitLog], fullref.val }

  log(("New %s peak %s"):format(trendStr(peaks.C.trend), peakStr(fullref)))
  local KidZeroed = tonumber(linkmeterd.getConf('pidi')) == 0.0 and tonumber(linkmeterd.getConf('pidd')) == 0.0
  if period and KidZeroed then
    local Ku = tonumber(linkmeterd.getConf('pidp') or 1)
    local p = 0.6 * Ku
    local i, d = 2 * p / period, p * period / 8 / 30
    log(("  Ziegler P=%.1f I=%.3f D=%.1f"):format(p, i, d))
    --p, i, d = Ku / 2.2, Ku / (2.2 * period), Ku * (period / 6.3) / 30
    --log(("  TLC Rules P=%.1f I=%.3f D=%.1f"):format(p, i, d))
  end

  -- Extended Lid Mode tracking
  if lidTrack == LIDMODE_AWAITING_LOW and peaks.C.trend == -1 then
    -- print(("High at %d is %.1f curr %.1f"):format(peaks.H.time or 0, peaks.H.val or 0.0, peaks.C.val or 0.0))
    log(("LidTrack detected low peak %.1f, dropped %.1f"):format(peaks.C.val, peaks.H.val - peaks.C.val))
    lidTrack = LIDMODE_AWAITING_HIGH
  end

  peaks.C.trend = newTrend
end

local function peakDetect(now, t)
  local newTrend = 0
  local i
  for i = #pitLog,1,-1 do
    local diff = t - pitLog[i]
    if diff > peakTrendThreshold then
      newTrend = 1
      --log(("Trend %d found at %d/%d"):format(newTrend, i, #pitLog))
      break
    end
    if diff < -peakTrendThreshold then
      newTrend = -1
      --log(("Trend %d found at %d/%d"):format(newTrend, i, #pitLog))
      break
    end
  end

  -- Initialize H and L if this is the first point
  if not peaks.H.time then
    peaks.H.time = now
    peaks.H.val = t
  end
  if not peaks.L.time then
    peaks.L.time = now
    peaks.L.val = t
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

local function updatePitLog(now, t)
  if (now % 10) == 0 then
    --print(("Insert new pitLog %d=%.1f"):format(now, t))
    pitLog[#pitLog+1] = t
    if #pitLog > 180 then table.remove(pitLog, 1) end

    linkmeterd.publishBroadcastMessage("peaks", peaks)
  end
end

local function updateLidTrack(now, t)
  local dps = 0
  -- Use a weighted average of the last two pitLog entries to look back 10 seconds
  if #pitLog > 1 then
    local nowPhase = now % 10 -- how long since the last pitLog entry
    local t10 = ((pitLog[#pitLog-1] * (10 - nowPhase)) + (pitLog[#pitLog] * nowPhase)) / 10
    dps = (t - t10) / 10
    --print(("LidTrack %.2f=>%.1f dps=%.2f"):format(t10, t, dps))
  end

  -- If Normal mode and pit temperature drops more than 0.5F/sec over 10 seconds activate lid mode
  if peaks.mode == HMMODE_NORMAL then
    if (dps < (-5.0 * peaksUnitsScale * peaksSetpointScale)) and (t < tonumber(hmSetpoint)) then
      log(("LidTrack detected %.2fdeg/s drop, triggering Lid Mode"):format(dps))
      if isLidTrackEnabled() then
        -- Enable lid mode
        linkmeterd.hmWrite("/set?ld=,,1\n")
      end
    end
  --end if mode NORMAL

  elseif lidTrack == LIDMODE_AWAITING_HIGH then
    -- If temperature change from the low peak is >20F and 0 < dps < 0.1F
    -- These thresholds aren't scaled by peaksSetpointScale because they occur not at Setpoint
    if (t - peaks.L.val) > (20 * peaksUnitsScale) and dps > 0 and dps < (0.1 * peaksUnitsScale) then
      lidTrack = nil
      log(("LidTrack detected high plateau at %.1f (%.2fdeg/s), self-recovered %.1f"):format(t, dps, t - peaks.L.val))
      if isLidTrackEnabled() then
        -- Disable lid mode
        linkmeterd.hmWrite("/set?ld=,,0\n")
      end
    end -- if temperature stabilized
  end -- end if LIDMODE_AWAITING_HIGH
end

local function updateState(now, vals)
  -- SetPoint, Probe0, Probe1, Probe2, Probe3, Output, OutputAvg, Lid, Fan, Servo
  -- 1         2       3       4       5       6       7          8    9,   10
  updateHmMode(vals)
  if peaks.mode == HMMODE_UNPLUG then return end

  peaks.updatecnt[peaks.mode] = peaks.updatecnt[peaks.mode] + 1
  local pitTemp = tonumber(vals[2])
  updatePitLog(now, pitTemp)
  peakDetect(now, pitTemp)
  updateLidTrack(now, pitTemp)

  if peaks.mode == HMMODE_NORMAL then
    updateOutputHist(vals[6])
  end
end

local function peaksState(line)
  local f = {}
  f[#f+1] = 'Histogram=' .. table.concat(peaks.outhist or {}, ',')
  f[#f+1] = 'peaks.mode=' .. peaks.mode .. ' (' .. modeStr(peaks.mode) .. ')'
  f[#f+1] = 'peaks.updatecnt=' .. table.concat(peaks.updatecnt, ',')
  f[#f+1] = ('peaksUnitsScale=%.2f peaksSetpointScale=%.2f peakTrendThreshold=%.2f')
    :format(peaksUnitsScale, peaksSetpointScale, peakTrendThreshold)
  if peaks.C.time then f[#f+1] = ("peaks.C (%02d) %s"):format(peaks.C.trend, peakStr(peaks.C)) end
  if peaks.H.time then f[#f+1] = ("peaks.H  %s"):format(peakStr(peaks.H)) end
  if peaks.L.time then f[#f+1] = ("peaks.L  %s"):format(peakStr(peaks.L)) end
  return table.concat(f, '\n')
end

function init()
  initState()

  linkmeterd.registerStatusListener(updateState)
  linkmeterd.registerSegmentListener("$LMPS", peaksState)
end
