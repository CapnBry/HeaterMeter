module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = alias("lm")

  local page = node("lm")
  page.target = alias("lm", "index")
  page.order = 10
  
  local page = node("lm", "index")
  page.target = template("linkmeter/index")
  page.order = 10
  page.sysauth = nil

  local page = node("lm", "json")
  page.target = call("json")
  page.order = 20
  page.sysauth = nil
end

function json()
  luci.http.prepare_content("text/plain")
  local f = io.open("/tmp/json", "rb")
  luci.ltn12.pump.all(luci.ltn12.source.file(f), luci.http.write)
  f:close()
end

