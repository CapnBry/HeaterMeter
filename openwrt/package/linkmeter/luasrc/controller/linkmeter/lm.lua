module("luci.controller.linkmeter.lm", package.seeall)

function index()
  local root = node()
  root.target = alias("lm")

  entry({"lm"}, template("linkmeter/index"), nil, 10)
end

