require "lmclient"
local json = require("luci.jsonc")

local lmcf = json.parse(LmClient():query("$LMCF") or "{}")

local m, s, v
m = Map("linkmeter", "Alarm Settings",
  [[ Select the types of notifications to receive when the alarm is trigged.
    Enable an alarm by setting
    its threshold to a positive value, or using the button beside it.
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
  if self.default == value then
    self:remove(section)
  else
    return m:set("alarms", self.option .. section, value)
  end
end
local function probe_conf_remove(self, section)
  return m:del("alarms", self.option .. section)
end

local PROBE_CONF = { "emailL", "smsL", "pushL", "spL", "raL", "emailH", "smsH", "pushH", "spH", "raH" }
for _,kv in ipairs(PROBE_CONF) do
  if kv == "raL" or kv == "raH" then
    v = s:option(ListValue, kv, kv)
    v.default = "0"
    v:value(0, "Ring")
    v:value(1, "Silence")
    v:value(2, "Disable")
  else
    v = s:option(Value, kv, kv)
  end
  v.cfgvalue = probe_conf_value
  v.write = probe_conf_write
  v.remove = probe_conf_remove
end

---
--- Ramp Mode
---

s = m:section(NamedSection, "ramp", nil, "Ramp Mode",
  [[Lowers the setpoint between a food probe's trigger and target temperatures
  until both the watched probe and the setpoint meet at the target tempeature.
  Manually changing the setpoint will disable ramp mode.
  ]])

local rampValueChanged
local function rampValueChangeNotify(self, section, value)
  rampValueChanged = true
  m:set(section, self.option, value)

  -- if switching to disabled clear any start setpoint
  if self.option == "watch" and value == "0" then
    m:del(section, "startsetpoint")
  end
end

local function notifyRampChanged()
  LmClient():query("$LMRA")
end

v = s:option(ListValue, "watch", "Watch probe")
v.write = rampValueChangeNotify
v:value(0, "Disabled")
for probe = 1,3 do
  v:value(probe, lmcf["pn" .. probe])
end
v = s:option(Value, "trigger", "Trigger temperature")
v.write = rampValueChangeNotify
v.default = "180"
v = s:option(Value, "target", "Target temperature")
v.write = rampValueChangeNotify
v.default = "200"

--
-- Email Notifications
--

s = m:section(NamedSection, "alarms_email", nil, "Email Notifications",
  [[Email notifications only work if <a href="]] ..
  luci.dispatcher.build_url("admin/services/msmtp") ..
  [[">SMTP Client</a> is configured.]])

v = s:option(Value, "toname", "Recipient name (optional)")
v = s:option(Value, "toaddress", "To email address")
v = s:option(Value, "subject", "Subject")

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

s = m:section(NamedSection, "alarms_sms", nil, "SMS Notifications",
  [[SMS notifications only work if <a href="]] ..
  luci.dispatcher.build_url("admin/services/msmtp") ..
  [[">SMTP Client</a> is configured.]])

local PROVIDERS = {
  { "AT&T", "txt.att.net" },
  { "Bell / Solo", "txt.bellmobility.ca" },
  { "Fido", "sms.fido.ca" },
  { "MTS", "text.mtsmobility.com" },
  { "Nextel", "messaging.nextel.com" },
  { "Plateau", "smsx.plateaugsm.com" },
  { "Project Fi", "msg.fi.google.com" },
  { "Rogers", "sms.rogers.com" },
  { "Sprint", "messaging.sprintpcs.com" },
  { "T-Mobile", "tmomail.net" },
  { "Telus / Koodo", "msg.telus.com" },
  { "Verizon", "vtext.com" },
  { "Virgin US", "vmobl.com" },
  { "Virgin Canada", "vmobl.ca" },
  { "Wind Mobile", "txt.windmobile.ca" },
  { "Other", "other" }
}

local function split_smsto()
  local smsto = m:get("alarms_sms", "toaddress")
  return smsto:match("^(%d+)@(.+)$")
end

local smsprovider_write = function(self, section, value)
  if value ~= "other" then
    local smsphone, _ = split_smsto()
    m:set("alarms_sms", "toaddress", smsphone .. "@" .. value)
  end
end

v = s:option(Value, "_phone", "Phone number")
v.datatype = "phonedigit"
v.cfgvalue = function()
  local smsphone, _ = split_smsto()
  return smsphone
end
v.write =  function(self, section, value)
  local _, smsprovider = split_smsto()
  m:set("alarms_sms", "toaddress", value .. "@" .. smsprovider)
end

v = s:option(ListValue, "_provider", "Provider")
for _,p in ipairs(PROVIDERS) do
  v:value(p[2], p[1])
end
v.cfgvalue = function ()
  local _, smsprovider = split_smsto()
  for _,p in ipairs(PROVIDERS) do
    if p[2] == smsprovider then
      return smsprovider
    end
  end
  return "other"
end
v.write = smsprovider_write

v = s:option(Value, "_provider_other", "Other Provider")
v:depends("linkmeter.alarms_sms._provider", "other")
v.cfgvalue = function ()
  local _, smsprovider = split_smsto()
  return smsprovider
end
v.write = smsprovider_write

v = s:option(Value, "message", "Message")
v.description = ESCAPE_HELP

--
-- Push Notifications
--

s = m:section(NamedSection, "alarms_push", nil, "Push Notifications",
  [[<ul><li><a href="https://www.pushbullet.com">Pushbullet</a> is a unified notification system for mobile and desktop devices.
    Find your Access Token in your account settings.</li><li><a href="https://www.pushover.net">Pushover</a>
    makes it easy to get real-time notifications on your Android, iPhone, iPad, and Desktop (Pebble, Android Wear, and Apple watches, too!)</li></ul>
  ]])

-- Providers Array
local PUSHPROVIDERS = {
  { "Pushbullet", "pushbullet" },
  { "Pushover", "pushover" }
}

-- Create dropdown list of providers
v = s:option(ListValue, "provider", "Push Provider")
for _,p in ipairs(PUSHPROVIDERS) do
  v:value(p[2], p[1])
end

-- Pushbullet Settings
v = s:option(Value, "pushbulletkey", "Access token")
v:depends("linkmeter.alarms_push.provider", "pushbullet")
v = s:option(Value, "pushbullettitle", "Message title")
v:depends("linkmeter.alarms_push.provider", "pushbullet")

-- Pushover Settings
local PUSHOVERSOUNDS = {
  { "Default" , "pushover" },
  { "Bike" , "bike" },
  { "Bugle" , "bugle" },
  { "Cash register" , "cashregister" },
  { "Classical" , "classical" },
  { "Cosmic" , "cosmic" },
  { "Falling" , "falling" },
  { "Gamelan" , "gamelan" },
  { "Incoming" , "incoming" },
  { "Intermission" , "intermission" },
  { "Magic" , "magic" },
  { "Mechanical" , "mechanical" },
  { "Piano bar" , "pianobar" },
  { "Siren" , "siren" },
  { "Space alarm" , "spacealarm" },
  { "Tug boat" , "tugboat" },
  { "Alien" , "alien" },
  { "Climb" , "climb" },
  { "Persistent" , "persistent" },
  { "Pushover Echo" , "echo" },
  { "Up down" , "updown" },
  { "None Silent" , "none" }
}

v = s:option(Value, "pushoveruser", "User key")
v:depends("linkmeter.alarms_push.provider", "pushover")
v = s:option(Value, "pushovertoken", "Application API token/key")
v:depends("linkmeter.alarms_push.provider", "pushover")
v = s:option(ListValue, "pushoversound", "Alert sound")
for _,p in ipairs(PUSHOVERSOUNDS) do
  v:value(p[2], p[1])
end
v:depends("linkmeter.alarms_push.provider", "pushover")

-- Push Message Body
v = s:option(Value, "message", "Message body")

--
-- Map Functions
--

m.apply_on_parse = true
m.on_after_save = function (self)
  saveProbeLm()
  if rampValueChanged then notifyRampChanged() end
end

return m
