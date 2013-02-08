module("luci.controller.msmtp", package.seeall)

function index()
  if not nixio.fs.access("/etc/msmtprc") then
    return
  end

  entry({"admin", "services", "msmtp"}, cbi("admin_services/msmtp"), _("SMTP Client"), 60)
end
