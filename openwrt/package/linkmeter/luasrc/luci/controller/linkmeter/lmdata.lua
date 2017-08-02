module("luci.controller.linkmeter.lmdata", package.seeall)

function index()
  entry({"lm", "hist"}, call("action_hist")).notemplate = true
  entry({"lm", "hmstatus"}, call("action_hmstatus")).notemplate = true
  entry({"lm", "rfstatus"}, call("action_rfstatus")).notemplate = true
  entry({"lm", "stream"}, call("action_stream")).notemplate = true
  entry({"lm", "conf"}, call("action_conf")).notemplate = true
  entry({"lm", "dluri"}, call("action_downloaduri")).notemplate = true
end

function lmclient_json(query, default)
  require "lmclient"
  local result, err = LmClient():query(query)
  result = result or default
  if result then
    luci.http.prepare_content("application/json")
    luci.http.header("Access-Control-Allow-Origin", "*")
    luci.http.write(result)
    return true
  else
    luci.dispatcher.error500("JSON read failed " .. query .. " error in " .. err)
  end
end

function action_hmstatus()
  return lmclient_json("$LMSU")
end

function action_rfstatus()
  return lmclient_json("$LMRF")
end

function action_conf()
  return lmclient_json("$LMCF", "{}")
end

function action_hist()
  local http = require "luci.http"
  local rrd = require "rrd"
  local uci = luci.model.uci.cursor()

  local RRD_FILE = http.formvalue("rrd") or uci:get("linkmeter", "daemon", "rrd_file")
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
      -- Output (dp[6]) should always be valid if the DB was capturing
      -- If val ~= val then val is a NaN, LUA doesn't have isnan()
      -- and NaN ~= NaN by C definition (platform-specific)
      if dp[6] == dp[6] then break end
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

  if http.formvalue("dl") == "1" then
    local csvfile = nixio.fs.basename(RRD_FILE:sub(1, -5)) .. ".csv"
    http.prepare_content("text/csv")
    http.header("Content-Disposition", 'attachment; filename="' .. csvfile ..'"')
  else
    http.prepare_content("text/plain")
  end
  http.header("Cache-Control", "max-age="..step)

  local seenData
  now = now - step
  for _, dp in ipairs(data) do
    -- Skip the first NaN rows until we actually have data and keep
    -- sending until we get to the 1 or 2 rows at the end that are NaN
    if (dp[6] == dp[6]) or (seenData and (start < now)) then
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

function action_downloaduri()
  -- Takes a data URI input and converts it to a file download
  -- It is dumb that most browsers do not support saving a local data URI to file
  -- but this works around it by having the client generate the image and the server
  -- just sends it back to them.
  -- LuCI only supports POST data larger than 8KB in a multipart/form-data, be
  -- sure to use a form with encoding set to this.
  -- Field "uri" the data URI (including data: and metadata)
  -- Field "filename" (optional) filename to set as the attachment name
  local nixio = require "nixio"
  local http = require "luci.http"
  local mime = require "luci.http.protocol.mime"
  local uri = http.formvalue("uri")
  if (not uri or uri:sub(1, 5) ~= "data:") then
    http.status(400, "URI must begin with 'data:'")
    return
  end

  local comma = uri:find(',')
  if (not comma) then
    http.status(400, "URI missing metadata")
    return
  end

  local metadata = uri:sub(6, comma-1)
  local mimetype = "text/plain"
  local encoding = ""
  local charset = "US-ASCII"
  local matchcnt = 0
  for meta in metadata:gmatch("[^;]+") do
    if matchcnt == 0 then
      mimetype = meta
    elseif meta:sub(1,8) == "charset=" then
      charset = meta:sub(9)
    else
      encoding = meta
    end
    matchcnt = matchcnt + 1
  end

  local filename = http.formvalue("filename") or
    (os.date("%Y%m%d_%H%M%S.") .. (mime.to_ext(mimetype) or "txt"))
  http.prepare_content(mimetype)
  http.header('Content-Disposition', 'attachment; filename="' .. filename .. '"')
  if encoding == "base64" then
    uri = nixio.bin.b64decode(uri:sub(comma+1))
  else
    uri = uri:sub(comma+1)
  end
  http.write(uri)
end
