module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = call("rootredirect") 

  local page = node("lm")
  page.target = template("linkmeter/index")
  page.order = 10
  page.sysauth = { "anon", "root" }
  page.sysauth_authenticator = require "luci.controller.linkmeter.lm".lmauth
  
  local page = node("lm", "json")
  page.target = call("json")
  page.order = 20

  local page = node("lm", "set")
  page.target = call("set")
  page.order = 30
  page.sysauth = "root"

  local page = node("lm", "login")
  page.target = call("rootredirect")
  page.order = 20
  page.sysauth = "root"
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

function json()
  -- luci.http.prepare_content("application/json")
  luci.http.prepare_content("text/plain")
  local f = io.open("/tmp/json", "rb")
  luci.ltn12.pump.all(luci.ltn12.source.file(f), luci.http.write)
  f:close()
end

function set()
  local dsp = require "luci.dispatcher"
  local http = require "luci.http"
  
  -- Make sure the user passed some values to set
  local vals = http.formvalue()
  local cnt = 0
  for _ in pairs(vals) do cnt = cnt + 1 end
  if cnt == 0 then
    return dsp.error500("No values specified")
  end
  
  local uci = luci.model.uci.cursor()
  local device = uci:get("linkmeter", "daemon", "serial_device")
  local f = nixio.open(device, "w")
  if f == nil then
    return dsp.error500("Can not open serial device "..device)
  end
 
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
    
    http.write("%s to %s\n" % {k,v})
    f:write("/set?%s=%s\n" % {k,v})
  end
  http.write("Done!")

  f:close()
end
