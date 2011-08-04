module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = call("rootredirect") 

  local page = entry({"lm"}, template("linkmeter/index"), nil, 10)
  page.sysauth = { "anon", "root" }
  page.sysauth_authenticator = require "luci.controller.linkmeter.lm".lmauth

  entry({"lm", "set"}, call("set"))
  entry({"lm", "login"}, call("rootredirect")).sysauth = "root"
end

function lmauth(validator, accs, default)
  local user = "anon"
  local sess = luci.http.getcookie("sysauth")
  sess = sess and sess:match("^[a-f0-9]*$")
  local sdat = luci.sauth.read(sess)
  if sdat then
    sdat = loadstring(sdat)
    setfenv(sdat, {})
    sdat = sdat()
    if sdat.token == luci.dispatcher.context.urltoken then
      user = sdat.user
    end
  end
  
  -- If the page requested does not allow anon acces and we're using the
  -- anon token, reutrn no session to get luci to prompt the user to escalate            
  local needsRoot = not luci.util.contains(accs, "anon")
  if needsRoot and user == "anon" then
    return luci.dispatcher.authenticator.htmlauth(validator, accs, default)
  else
    return user, sess
  end
end

function rootredirect()
  luci.http.redirect(luci.dispatcher.build_url("lm/"))
end

function set()
  local dsp = require "luci.dispatcher"
  local http = require "luci.http"
  
  local vals = http.formvalue()
  
  -- If there's a rawset, explode the rawset into individual items
  local rawset = vals.rawset
  if rawset then
    -- remove /set? or set? if supplied
    rawset = rawset:gsub("^/?set%?","")
    vals = require "luci.http.protocol".urldecode_params(rawset)
  end

  -- Make sure the user passed some values to set
  local cnt = 0
  -- Can't use #vals because the table is actually a metatable with an indexer
  for _ in pairs(vals) do cnt = cnt + 1 end
  if cnt == 0 then
    return dsp.error500("No values specified")
  end

  require("lmclient")
  local lm = LmClient()

  http.prepare_content("text/plain")
  http.write("User %s setting %d values...\n" % {dsp.context.authuser, cnt})
  local firstTime = true
  for k,v in pairs(vals) do
    -- Pause 100ms between commands to allow HeaterMeter to work
    if firstTime then
      firstTime = nil
    else
      nixio.nanosleep(0, 100000000)
    end

    local result, err = lm:query("$LMST,%s,%s" % {k,v})
    http.write("%s to %s = %s\n" % {k,v, result or err})
    if err then break end
  end
  lm:close()
  http.write("Done!")
end
