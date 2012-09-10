module("luci.controller.linkmeter.lmadmin", package.seeall)

function index()
  entry({"admin", "lm"}, alias("admin", "lm", "conf"), "LinkMeter",60).index = true
  entry({"admin", "lm", "home"}, alias("lm", "login"), "Home", 10)
  entry({"admin", "lm", "conf"}, template("linkmeter/conf"), "Configuration", 20)
  entry({"admin", "lm", "archive"}, template("linkmeter/archive"), "Archive", 30)
  entry({"admin", "lm", "fw"}, call("action_fw"), "AVR Firmware", 40)
  entry({"admin", "lm", "credits"}, template("linkmeter/credits"), "Credits", 50)

  entry({"admin", "lm", "stashdb"}, call("action_stashdb"))
  entry({"admin", "lm", "reboot"}, call("action_reboot"))
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
  local web_update = has_upload and has_upload:find("^http://")
  
  if web_update then
    hex = has_upload
    step = 3
  end
  if step == 1 then
    if has_upload and nixio.fs.access(hex) then
      step = 2
    else
      nixio.fs.unlink(hex)
    end
    return luci.template.render("linkmeter/fw", { step=step })
  end
  if step == 3 then
    luci.http.prepare_content("text/plain")
    local pipe = require "luci.controller.admin.system".ltn12_popen(
      "/usr/bin/avrupdate %q" % hex)
    return luci.ltn12.pump.all(pipe, luci.http.write)
  end 
end

function action_stashdb()
  local http = require "luci.http"
  local uci = luci.model.uci.cursor()

  local RRD_FILE = uci:get("lucid", "linkmeter", "rrd_file")
  local STASH_PATH = uci:get("lucid", "linkmeter", "stashpath") or "/root"
  local restoring = http.formvalue("restore")
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
      result = nixio.fs.unlink(RRD_FILE)
      http.write("Resetting "..RRD_FILE)
    else
      result = nixio.fs.copy(stashfile, RRD_FILE)
      http.write("Restoring "..stashfile.." to "..RRD_FILE)
    end
    lm:query("$LMDC,1") -- start serial process and close connection
  else
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
