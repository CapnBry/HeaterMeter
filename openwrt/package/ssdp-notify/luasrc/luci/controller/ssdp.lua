local M = {}

function M.index()
  entry({"ssdp"}, call("action_discover"))
end

function M.action_discover()
  local uci = require("uci").cursor()
  local uuid
  uci:foreach("system", "system", function(s) uuid = s.uuid end)

  luci.http.prepare_content("application/xml")
  return luci.template.render("discover", {
    presentationUrl='http://' .. luci.http.getenv('HTTP_HOST') .. '/luci/lm',
    friendlyName=luci.sys.hostname() .. ' (HeaterMeter)',
    manufacturer='HeaterMeter',
    manufacturerUrl='https://heatermeter.com/',
    modelName='4.x',
    modelNumber='v14',
    uuid=uuid
  })
end

return M

