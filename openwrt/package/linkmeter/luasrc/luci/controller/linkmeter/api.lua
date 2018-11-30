local _M = {}
local API_VERSION = 1

function _M.index()
  entry({"lm", "api"}, alias({"lm", "api", "version"}))
  entry({"lm", "api", "version"}, call("action_api_version"))
  entry({"lm", "api", "status"}, call("action_api_status")).leaf = true
  entry({"lm", "api", "config"}, call("action_api_config")).leaf = true
  entry({"lm", "api", "fw"}, call("action_api_fw")).leaf = true
end

local function is_write()
  return luci.http.getenv("REQUEST_METHOD") == "POST"
end

local function auth(write_access)
  local http = require("luci.http")
  local uci = require("uci"):cursor()
  -- If API is disabled, both read and write is disabled
  if uci:get("linkmeter", "api", "disabled") == "1" then
    http.status(403, "Forbidden")
    http.prepare_content("text/plain")
    http.write("API disabled")
    return nil
  end

  if uci:get("linkmeter", "api", "allowcors") == "1" then
    http.header("Access-Control-Allow-Origin", "*")
  end

  -- Case 1: sysauth = api_read
  if not write_access then
    return true
  end

  -- Case 2: sysauth = api_write or { api_read, api_write } require a POST
  if write_access and http.getenv("REQUEST_METHOD") ~= "POST" then
    http.status(405, "Method Not Allowed")
    http.header("Allow", "POST")
    return nil
  end

  -- and check the supplied API key
  local key = http.formvalue("apikey")
  local apikey = uci:get("linkmeter", "api", "key")
  if apikey and apikey ~= "" and key and key:lower() == apikey:lower() then
    luci.dispatcher.context.authuser = "api_write"
    return true
  else
    http.status(403, "Forbidden")
    http.prepare_content("text/plain")
    http.write("Invalid or missing apikey")
    return nil
  end
end

local function quick_json(t)
  local http = require("luci.http")
  http.prepare_content("application/json")
  http.write('{')
  local c = ""
  for k,v in pairs(t) do
    if type(v) == "string" then
      if v == "null" then
        http.write(c .. '"' .. k .. '":null')
      else
        http.write(c .. '"' .. k .. '":"' .. v .. '"')
      end
    else
      http.write(c .. '"' .. k .. '":' .. v)
    end
    c = ","
  end
  http.write('}')
end

local function api_lmquery(query, tablefmt)
  lmclient = require("lmclient")
  local result, err = LmClient:query(query)
  result = result or "{}"
  -- Return the result in a parsed table instead of string
  if tablefmt then
    local jsonc = require("luci.jsonc")
    local o = jsonc.parse(result)
    return o, err
  end

  return result, err
end

function _M.action_api_version()
  if not auth() then return end
  local conf = api_lmquery("$LMCF", true)
  quick_json({api = API_VERSION, ucid = conf.ucid or "null"})
end

function _M.action_api_status()
  if not auth() then return end
  local ctx = luci.dispatcher.context
  local param = ctx.requestargs and #ctx.requestargs > 0

  local status = api_lmquery("$LMSU", param)
  local http = require("luci.http")
  if param then
    for _,k in ipairs(ctx.requestargs) do
      -- if k is an integer, use it as an index
      local kn = tonumber(k)
      if kn and kn == math.floor(kn) then k = kn+1 end
      status = status and status[k]
    end
  end
  
  if type(status) == "table" then
    http.prepare_content("application/json")
    http.write(luci.jsonc.stringify(status))
  elseif type(status) == "string" and status:sub(1,1) == "{" then
    http.prepare_content("application/json")
    http.write(status)
  else
    http.prepare_content("text/plain")
    http.write(tostring(status or "null")) -- content must be a string
  end
end

function _M.action_api_config()
  if not auth(is_write()) then return end
  -- See if just one parameter is specified, or this is a parent call  
  local param, value
  local ctx = luci.dispatcher.context
  if ctx.requestargs and #ctx.requestargs > 0 then
    param = ctx.requestargs[1]
    if #ctx.requestargs > 1 then
      value = ctx.requestargs[2]
    end
  end
  
  local http = require("luci.http")
  if is_write() then
    local lmadmin = require("luci.controller.linkmeter.lmadmin")
    if param then
      local vals = {}
      vals[param] = http.formvalue(param) or http.formvalue("value")
      return lmadmin.api_set(vals)
    else
      return lmadmin.api_set(http.formvalue())
    end
  else
    local result = api_lmquery("$LMCF", param)
    if param then
      http.prepare_content("text/plain")
      http.write(result[param] or "null")
    else
      http.prepare_content("application/json")
      http.write(result)
    end
  end  -- isRead
end

function _M.action_api_fw()
  if not auth(true) then return end
  if not luci.http.formvalue("hexfile") then
    luci.http.status(400, "Bad Request")
    luci.http.prepare_content("text/plain")
    luci.http.write("Missing hexfile POST parameter")
    return
  end

  local lmadmin = require("luci.controller.linkmeter.lmadmin")
  lmadmin.api_file_handler("/tmp/hm.hex")
  return lmadmin.api_post_fw("/tmp/hm.hex")
end

return _M
