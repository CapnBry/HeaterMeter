require "lmclient"
local json = require("luci.json")

local lmcf = json.decode(LmClient():query("$LMCF"))

local m, s, v
m = Map("linkmeter", "Alarm Settings",
  [[Alarm trigger points and alert options. Enable an alarm by setting
    its threshold to a positive value, or checking the box beside it.
    Select the types of notifications to receive when the alarm is trigged.
    Inputting a 'Setpoint' will adjust the Pit setpoint.]])

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
local function probe_lm_value(self, section)
  -- self.option will be the "palh" section will be 0,1,2,3
  return tostring(lmcf[self.option .. section])
end
local function probe_lm_write(self, section, value)
  print("lmwrite",self.option,section,value)
end

local PROBE_LM = { "pn", "pcurr", "pall", "palh" }
for _,kv in ipairs(PROBE_LM) do
  v = s:option(Value, kv, kv)
  v.value = probe_lm_value
  v.cfgvalue = probe_lm_value
  v.write = probe_lm_write
end

-- probe_conf_* values that are stored in uci
local function probe_conf_value(self, section)
  return m:get("alarms", self.option .. section)
end
local function probe_conf_write(self, section, value)
  print("confwrite",self.option,section,value)
  return m:set("alarms", self.option .. section, value)
end

local PROBE_CONF = { "emaill", "smsl", "spl", "emailh", "smsh", "sph" }
for _,kv in ipairs(PROBE_CONF) do
  v = s:option(Value, kv, kv)
  v.cfgvalue = probe_conf_value
  v.write = probe_conf_write
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

--
-- Map Functions
--

m.on_save = function (self) 
  print("on_save",self.changed) 
end

return m
