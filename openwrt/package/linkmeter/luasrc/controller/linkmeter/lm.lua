module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = call("rootredirect") 

  local page = entry({"lm"}, template("linkmeter/index"), nil, 10)
end

function rootredirect()
  luci.http.redirect(luci.dispatcher.build_url("lm/"))
end
