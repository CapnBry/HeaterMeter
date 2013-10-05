require "lmclient"
local json = require("luci.json")

local lmcf = json.decode(LmClient():query("$LMCF"))

local m, s, v
m = Map("linkmeter", "Alarm Settings",
  [[ Select the types of notifications to receive when the alarm is trigged.
    Enable an alarm by setting
    its threshold to a positive value, or using the button beside it.
    Inputting a 'Setpoint' will adjust the Pit setpoint.
    Test results can be seen in the <a href="]] ..
    luci.dispatcher.build_url("admin/status/syslog/") ..
    [[">System Log</a>.
    <strong>'Save &amp; Apply' before testing.</strong>
    ]])
local ESCAPE_HELP = "All special characters (e.g. parens) must be escaped"

if lmcf == nil then
  s = m:section(SimpleSection, "_noconfig")
  s.template = "linkmeter/noconfig"
  return m
end

--
-- Alarm Values
--

s = m:section(TypedSection, "_hmconfig")
s.template = "linkmeter/alarm-section"
s.cfgsections = function(a,b,c)
  local retVal = {}
  for probe = 1,4 do
    retVal[probe] = tostring(probe-1)
  end
  return retVal
end
s.field = function(self, name)
  for k,v in pairs(self.fields) do
    if k == name then return v end
  end
end

-- probe_lm_* values that are stored in HeaterMeter
local probe_lm_alvals = {}
local function probe_lm_value(self, section)
  local retVal = lmcf[self.option .. section]
  return retVal and tostring(retVal)
end
local function probe_lm_write(self, section, value)
  -- Alarm high/low
  if self.option:sub(1,3) == "pal" then
    local idx = self.option:sub(-1) == "l" and 1 or 2
    idx = idx + 2 * tonumber(section)
    while #probe_lm_alvals < idx do
      probe_lm_alvals[#probe_lm_alvals+1] = ""
    end
    probe_lm_alvals[idx] = value
  end

  lmcf[self.option .. section] = value
end

local PROBE_LM = { "pn", "pcurr", "pall", "palh" }
for _,kv in ipairs(PROBE_LM) do
  v = s:option(Value, kv, kv)
  v.value = probe_lm_value
  v.cfgvalue = probe_lm_value
  v.write = probe_lm_write
end

local function saveProbeLm()
  if #probe_lm_alvals > 0 then
    local alvals = "$LMST,al," .. table.concat(probe_lm_alvals, ",")
    LmClient():query(alvals)
  end
end

-- probe_conf_* values that are stored in uci
local function probe_conf_value(self, section)
  return m:get("alarms", self.option .. section)
end
local function probe_conf_write(self, section, value)
  return m:set("alarms", self.option .. section, value)
end
local function probe_conf_remove(self, section)
  return m:del("alarms", self.option .. section)
end

local PROBE_CONF = { "emaill", "smsl", "spl", "emailh", "smsh", "sph" }
for _,kv in ipairs(PROBE_CONF) do
  v = s:option(Value, kv, kv)
  v.cfgvalue = probe_conf_value
  v.write = probe_conf_write
  v.remove = probe_conf_remove
end

--
-- Email Notifications
--

s = m:section(NamedSection, "alarms", "email", "Email Notifications", 
  [[Email notifications only work if <a href="]] ..
  luci.dispatcher.build_url("admin/services/msmtp") ..
  [[">SMTP Client</a> is configured.]])

v = s:option(Value, "emailtoname", "Recipient name (optional)")
v = s:option(Value, "emailtoaddress", "To email address")
v = s:option(Value, "emailsubject", "Subject")

local msg = s:option(TextValue, "_msg", "Message")
msg.wrap    = "off"
msg.rows    = 5
msg.rmempty = false
msg.description = ESCAPE_HELP

local MSG_TEMPLATE = "/usr/share/linkmeter/email.txt"
function msg.cfgvalue()
  return nixio.fs.readfile(MSG_TEMPLATE) or ""
end
        
function msg.write(self, section, value)
  if value then
    value = value:gsub("\r\n", "\n")
    if value ~= msg.cfgvalue() then
      nixio.fs.writefile(MSG_TEMPLATE, value)
    end
  end
end

--
-- SMS Notifications
-- 

s = m:section(NamedSection, "alarms", "sms", "SMS Notifications",
  [[SMS notifications only work if <a href="]] ..
  luci.dispatcher.build_url("admin/services/msmtp") ..
  [[">SMTP Client</a> is configured.]])

local PROVIDERS = {
  { "AT&T", "txt.att.net", "ATT" },
  { "Nextel", "messaging.nextel.com" },
  { "Plateau", "smsx.plateaugsm.com" },
  { "Sprint", "messaging.sprintpcs.com" },
  { "T-Mobile", "tmomail.net" },
  { "Verizon", "vtext.com" },
  { "Virgin Mobile", "vmobl.com" },
}
local smsto = m:get("alarms", "smstoaddress")
local smsphone, smsprovider = smsto:match("^(%d+)@(.+)$")

v = s:option(Value, "_phone", "Phone number")
v.cfgvalue = function() return smsphone end
v.datatype = "phonedigit"

v = s:option(ListValue, "_provider", "Provider")
for i,p in ipairs(PROVIDERS) do
  local key = p[3] or p[1]
  v:value(key, p[1])
  -- convert the @addr to the provider name
  if p[2] == smsprovider then
    v.cfgvalue = function () return key end
  end
end

v = s:option(Value, "smsmessage", "Message")
v.description = ESCAPE_HELP

--
-- Map Functions
--

m.on_save = function (self) 
  saveProbeLm()
end

return m
