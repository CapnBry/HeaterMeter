module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = alias("lm")

  entry({"lm"}, template("linkmeter/index"), nil, 10)
  entry({"lm", "light"}, call("action_light_index"), nil, 20)
end

function action_light_index()
  require "lmclient"
  local json = require("luci.json")
  local result, err = LmClient():query("$LMSU")
  if result then
    local lm = json.decode(result)
    luci.template.render("linkmeter/light", {lm=lm, lmraw=result})
  else
    luci.dispatcher.error500("Stauts read failed: " .. err or "Unknown")
  end
end

