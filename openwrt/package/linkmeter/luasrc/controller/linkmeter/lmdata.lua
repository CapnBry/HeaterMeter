module("luci.controller.linkmeter.lmdata", package.seeall)

function index()
  local page = entry({"lm", "qry"}, call("action_qry"))
  if not page.inreq then return end
  page.notemplate = true
  page.leaf = true
  
  entry({"lm", "hist"}, call("action_hist")).notemplate = true
  entry({"lm", "stream"}, call("action_stream")).notemplate = true
end

function lmclient_json(query)
  require "lmclient"
  local result, err = LmClient():query(query) 
  if result then
    luci.http.prepare_content("application/json")
    luci.http.write(result)
    return true
  else
    luci.dispatcher.error500("JSON read failed " .. query .. " error in " .. err)
  end
end

function action_qry()
  local qry = luci.http.getenv("QUERY_STRING")
  if not qry or qry == "" then
    luci.dispatcher.error500("No query specified")
  else
    return lmclient_json("$"..qry)
  end
end

function action_hist()
  local http = require "luci.http"
  local rrd = require "rrd"
  local uci = luci.model.uci.cursor()

  local RRD_FILE = http.formvalue("rrd") or uci:get("lucid", "linkmeter", "rrd_file") 
  local nancnt = tonumber(http.formvalue("nancnt"))
  local start, step, data, soff
  
  if not nixio.fs.access(RRD_FILE) then
    http.status(503, "Database Unavailable")  
    http.prepare_content("text/plain")
    http.write("No database: %q" % RRD_FILE)
    return
  end
  
  local now = rrd.last(RRD_FILE)
  
  if not nancnt then
    -- scroll through the data and find the first line that has data
    -- this should indicate the start of data recording on the largest
    -- step data.  Then use that to determine the smallest step that
    -- includes all the data
    start, step, _, data = rrd.fetch(RRD_FILE, "AVERAGE", "--end", now)
    nancnt = 0
    for _, dp in ipairs(data) do
      -- SetPoint (dp[1]) should always be valid if the DB was capturing
      -- If val ~= val then val is a NaN, LUA doesn't have isnan()
      -- and NaN ~= NaN by C definition (platform-specific)
      if dp[1] == dp[1] then break end
      nancnt = nancnt + 1
    end
  end
    
  if nancnt >= 460 then
    step = 10
    soff = 3600
  elseif nancnt >= 360 then
    step = 60
    soff = 21600
  elseif nancnt >= 240 then
    step = 120
    soff = 43200
  else
    step = 180
    soff = 86400
  end

  -- Make sure our end time falls on an exact previous or now time boundary
  now = math.floor(now/step) * step  

  -- Only pull new data if the nancnt probe data isn't what we're looking for 
  if step ~= 180 or not data then
    start, step, _, data = rrd.fetch(RRD_FILE, "AVERAGE",
      "--end", now, "--start", now - soff, "-r", step)
  end
  
  http.prepare_content("text/plain")
  http.header("Cache-Control", "max-age="..step)

  local seenData 
  now = now - step
  for _, dp in ipairs(data) do
    -- Skip the first NaN rows until we actually have data and keep
    -- sending until we get to the 1 or 2 rows at the end that are NaN
    if (dp[1] == dp[1]) or (seenData and (start < now)) then
      http.write(("%u,%s\n"):format(start, table.concat(dp, ",")))
      seenData = true
    end
    
    start = start + step
  end 
end

function action_stream()
  local http = require "luci.http"
  http.prepare_content("text/event-stream")
  require "lmclient"
  LmClient:stream("$LMSS", function (o)
    http.write(o)
    collectgarbage("collect")
  end)
end
