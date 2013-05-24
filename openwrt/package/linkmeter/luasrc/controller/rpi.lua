module("luci.controller.rpi", package.seeall)

function index()
  if not nixio.fs.access("/boot/config.txt") then
    return
  end

  entry({"admin", "system", "rpi"}, cbi("admin_system/rpi"), "RaspberryPi", 65)
end
