module("linkmeter.unkprobe", package.seeall)

local lmfit
local unkProbe
local unkProbeDiscard

local function updateState(_, vals)
  if unkProbe == nil then return end
  if vals[2] == 'U' or vals[3] == 'U' then return end
  
  if unkProbeDiscard ~= nil then 
    unkProbeDiscard = unkProbeDiscard - 1
    if unkProbeDiscard >= 0 then return end
	unkProbeDiscard = nil
  end
  
  local t = tonumber(vals[2])
  local r = math.floor(vals[3])
  unkProbe[t] = r
end

local function unkProbeCurveFit()
  lmfit = lmfit or require "lmfit"
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
  local vals = linkmeterd.segSplit(line)
  if vals[1] == "start" then
    unkProbe = {}
	unkProbeDiscard = 1
    linkmeterd.registerStatusListener(updateState)
    linkmeterd.hmWrite("/set?sp=0R\n")
    return "OK"
  elseif vals[1] == "fit" and unkProbe then
    return unkProbeCurveFit()
  elseif vals[1] == "csv" and unkProbe then
    return unkProbeCsv()
  elseif vals[1] == "stop" and unkProbe then
    unkProbe = nil
    linkmeterd.hmWrite("/reboot\n")
    linkmeterd.unregisterStatusListener(updateState)
    return "OK"
  else
    return "ERR"
  end
end

function init()
  unkProbe = nil
  linkmeterd.unregisterStatusListener(updateState)
  linkmeterd.registerSegmentListener("$LMUP", segLmUnknownProbe)
end
