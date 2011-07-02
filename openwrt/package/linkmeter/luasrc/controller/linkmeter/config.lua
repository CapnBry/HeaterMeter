module("luci.controller.linkmeter.config", package.seeall)

function index()
  entry({"admin", "linkmeter"}, alias("admin", "linkmeter", "conf"), "LinkMeter", 50).index = true
  entry({"admin", "linkmeter", "home"}, alias("lm", "login"), "Home", 10)
  entry({"admin", "linkmeter", "conf"}, template("linkmeter/conf"), "Configuration", 10)
  entry({"admin", "linkmeter", "fw"}, call("action_fw"), "AVR Firmware", 20)
end

function action_fw()
  local hex = "/tmp/hm.hex"
  
  local file
  luci.http.setfilehandler(
    function(meta, chunk, eof)
      if not nixio.fs.access(hex) and not file and chunk and #chunk > 0 then
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
      "/www/cgi-bin/avrupdate %q" % hex)
    return luci.ltn12.pump.all(pipe, luci.http.write)
  end 
end

