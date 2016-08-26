local rrd = require "rrd" 
local lmfit = require "lmfit"
local uci = require "uci"

module("linkmeter.dph", package.seeall)

local lastDphUpdate = 0
local dphEstimates = { {0,0}, {0,0}, {0,0}, {0,0} }
local function updateState(now, _)
  -- Update the degrees per hour estimates once every minute
  if not lmfit or (now - lastDphUpdate) < 60 then return end
  lastDphUpdate = now

  local step = 10
  local soff = 3600
  local RRD_FILE = uci.cursor():get("linkmeter", "daemon", "rrd_file")
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

    local dph = nil
    if #x > 180 then
      local ok, p, status, evals = pcall(lmfit.linear, x, y, dphEstimates[probe])
      if ok and status >= 1 and status <= 3 then
        dph = tonumber(("%.2f"):format(p[1]))
        dphEstimates[probe] = p
        --print(("probe=%d m=%.3f b=%.3f evals=%d status=%d %s"):format(
        --  probe, p[1], p[2] or 0.0, evals, status, lmfit.message(status)))
      end
    end -- if #x

	linkmeterd.publishStatusVal("dph", dph, probe)
  end -- for probe
end

function init()
  linkmeterd.registerStatusListener(updateState)
end
