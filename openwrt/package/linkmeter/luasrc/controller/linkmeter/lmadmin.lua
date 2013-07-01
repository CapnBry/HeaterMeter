module("luci.controller.linkmeter.lmadmin", package.seeall)

function index()
  local node = entry({"admin", "lm"}, alias("admin", "lm", "conf"), "LinkMeter",60)
  node.index = true
  entry({"admin", "lm", "home"}, template("linkmeter/index"), "Home", 10)
  entry({"admin", "lm", "conf"}, template("linkmeter/conf"), "Configuration", 20)
  entry({"admin", "lm", "archive"}, template("linkmeter/archive"), "Archive", 40)
  entry({"admin", "lm", "fw"}, call("action_fw"), "AVR Firmware", 50)
  entry({"admin", "lm", "credits"}, template("linkmeter/credits"), "Credits", 60)

  entry({"admin", "lm", "stashdb"}, call("action_stashdb"))
  entry({"admin", "lm", "reboot"}, call("action_reboot"))
  entry({"admin", "lm", "set"}, call("action_set"))
  
  if node.inreq and nixio.fs.access("/usr/share/linkmeter/alarm") then
    entry({"admin", "lm", "alarms"}, form("linkmeter/alarms"), "Alarm Scripts", 30)
  end
end

function action_fw()
  local hex = "/tmp/hm.hex"
  
  local file
  luci.http.setfilehandler(
    function(meta, chunk, eof)
      if not file and chunk and #chunk > 0 then
        file = io.open(hex, "w")
      end
      if file and chunk then
        file:write(chunk)
      end
      if file and eof then
        file:close()
      end
    end
  )
  local step = tonumber(luci.http.formvalue("step") or 1)
  local has_upload = luci.http.formvalue("hexfile")
  local hexpath = luci.http.formvalue("hexpath")
  local web_update = hexpath and hexpath:find("^http://")
  
  if step == 1 then
    if has_upload and nixio.fs.access(hex) then
      step = 2
    elseif hexpath and (web_update or nixio.fs.access(hexpath)) then
      step = 2
      hex = hexpath
    else
      nixio.fs.unlink(hex)
    end
    return luci.template.render("linkmeter/fw", {step=step, hex=hex})
  end
  if step == 3 then
    luci.http.prepare_content("text/plain")
    local pipe = require "luci.controller.admin.system".ltn12_popen(
      "/usr/bin/avrupdate %q" % luci.http.formvalue("hex"))
    return luci.ltn12.pump.all(pipe, luci.http.write)
  end 
end

function action_stashdb()
  local http = require "luci.http"
  local uci = luci.model.uci.cursor()

  local RRD_FILE = uci:get("lucid", "linkmeter", "rrd_file")
  local STASH_PATH = uci:get("lucid", "linkmeter", "stashpath") or "/root"
  local restoring = http.formvalue("restore")
  local backup = http.formvalue("backup")
  local resetting = http.formvalue("reset")
  local deleting = http.formvalue("delete")
  local stashfile = http.formvalue("rrd") or "hm.rrd"

  -- directory traversal
  if stashfile:find("[/\\]+") then
    http.status(400, "Bad Request")
    http.prepare_content("text/plain")
    return http.write("Invalid stashfile specified: "..stashfile)
  end

  -- the stashfile should start with a slash
  if stashfile:sub(1,1) ~= "/" then stashfile = "/"..stashfile end
  -- and end with .rrd
  if stashfile:sub(-4) ~= ".rrd" then stashfile = stashfile..".rrd" end

  stashfile = STASH_PATH..stashfile
  
  if backup == "1" then
    local backup_cmd = "cd %q && tar cz *.rrd" % STASH_PATH
    local reader = require "luci.controller.admin.system".ltn12_popen(backup_cmd)
    http.header("Content-Disposition",
      'attachment; filename="lmstash-%s-%s.tar.gz"' % {
      luci.sys.hostname(), os.date("%Y-%m-%d")})
    http.prepare_content("application/x-targz")
    return luci.ltn12.pump.all(reader, luci.http.write)
  end

  local result
  http.prepare_content("text/plain")
  if deleting == "1" then
    result = nixio.fs.unlink(stashfile)
    http.write("Deleting "..stashfile)
    stashfile = stashfile:gsub("\.rrd$", ".txt")
    if nixio.fs.access(stashfile) then
      nixio.fs.unlink(stashfile)
      http.write("\nDeleting "..stashfile)
    end
  elseif restoring == "1" or resetting == "1" then
    require "lmclient"
    local lm = LmClient()
    lm:query("$LMDC,0", true) -- stop serial process
    if resetting == "1" then
      nixio.fs.unlink("/root/autobackup.rrd")
      result = nixio.fs.unlink(RRD_FILE)
      http.write("Removing autobackup\nResetting "..RRD_FILE)
    else
      result = nixio.fs.copy(stashfile, RRD_FILE)
      http.write("Restoring "..stashfile.." to "..RRD_FILE)
    end
    lm:query("$LMDC,1") -- start serial process and close connection
  else
    if not nixio.fs.stat(STASH_PATH) then
      nixio.fs.mkdir(STASH_PATH)
    end
    result = nixio.fs.copy(RRD_FILE, stashfile)
    http.write("Stashing "..RRD_FILE.." to "..stashfile)
  end

  if result then
    http.write("\nOK")
  else
    http.write("\nERR")
  end
end

function action_reboot()
  local http = require "luci.http"
  http.prepare_content("text/plain")
  
  http.write("Rebooting AVR... ")
  require "lmclient"
  http.write(LmClient():query("$LMRB") or "FAILED")
end

function action_set()
  local dsp = require "luci.dispatcher"
  local http = require "luci.http"
  
  local vals = http.formvalue()
  
  -- If there's a rawset, explode the rawset into individual items
  local rawset = vals.rawset
  if rawset then
    -- remove /set? or set? if supplied
    rawset = rawset:gsub("^/?set%?","")
    vals = {}
    for pair in rawset:gmatch( "[^&;]+" ) do
      local key = pair:match("^([^=]+)")
      local val = pair:match("^[^=]+=(.+)$")
      if key and val then
        vals[key] = val
      end
    end
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

    local result, err = lm:query("$LMST,%s,%s" % {k,v}, true)
    http.write("%s to %s = %s\n" % {k,v, result or err})
    if err then break end
  end
  lm:close()
  http.write("Done!")
end
