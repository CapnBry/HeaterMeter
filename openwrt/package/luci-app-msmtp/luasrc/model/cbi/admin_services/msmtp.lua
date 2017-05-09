require "nixio"
require "nixio.util"

local function OPT_SPLIT(o) return o:match("(%w+)\.(.+)") end

local conf = {}
local accountrenames = {}
local f = nixio.open("/etc/msmtprc", "r")
if f then
  local aname = "#"
  for line in f:linesource() do
    line = line:match("%s*(.+)")
    -- Make sure the line isn't a comment
    if line and line:sub(1, 1) ~= "#" then
      local option = line:match("%S+"):lower()
      local value = line:match("%S*%s*(.*)")
    
      if option == "account" then
        aname = value
      end
      if not conf[aname] then 
        conf[aname] = {}
      end
      conf[aname][option] = value
    end -- if line
  end -- for line
  f:close()
end

-- Associated tables can't be sorted so make a copy in an indexed table
-- so the accounts can be put in order
local orderedconf
local function sortAccounts()
  if not conf.default then
    local default = { account = "default" }
    conf['default'] = default
  end

  orderedconf = {}
  for _,v in pairs(conf) do
    orderedconf[#orderedconf+1] = v
  end
  table.sort(orderedconf, function (a,b)
    if a.account == "default" then
      return true
    elseif b.account == "default" then
      return false
    else
      return a.account<b.account
    end
  end)
end

local m = SimpleForm("msmtp", "SMTP (msmtp) Email Client",
  [[This will rewrite the /etc/msmtprc file. Any comments in the file will
  be removed when the file is written.
  ]])
  function m.del(self, section, option)
    local acct, opt = OPT_SPLIT(option)
    conf[acct][opt] = nil
  end
  function m.get(self, section, option)
    local acct, opt = OPT_SPLIT(option)
    return conf[acct][opt]
  end
  function m.set(self, section, option, value)
    local acct, opt = OPT_SPLIT(option)
    if opt == "account" then
      -- restrict msmtp account name to only alphanumeric
      value = value:gsub("%W", "")
      accountrenames[value] = acct
    end
    conf[acct][opt] = value
  end

local s = m:section(NamedSection, "Accounts")
function s.cfgvalue(self, section)
  return true
end

local function buildAccountTabs()
  s.tabs = {}
  s.tab_names = {}
  for _, acct in pairs(orderedconf) do
    local aname = acct.account
    s:tab(aname, aname)
    local fld, dep
    local depbase = s.config .. "." .. s.section .. "."

    fld = s:taboption(aname, Value, aname .. ".account", "Configuration name",
      "Use 'default' if this is the only configuration")
    fld.optional = true
    fld.rmempty = false

    fld = s:taboption(aname, Value, aname .. ".host", "Server host name or IP")
    fld.rmempty = false
    fld.placeholder = "mail.domain.com"
    fld.datatype = "host"

    fld = s:taboption(aname, Value, aname .. ".port", "Server port number")
    fld.default = "25"
    fld.datatype = "port"

    fld = s:taboption(aname, Value, aname .. ".from", "Email 'from' address")
    fld.placeholder = "user@domain.com"
    fld.optional = true
    fld.datatype = "minlength(3)"
 
    fld = s:taboption(aname, Flag, aname .. ".auth", "Requires authentication")
    fld.default = 0
    fld.disabled = "off"
    fld.enabled = "on"
    dep = {}
    dep[depbase .. fld.option] = "on"

    fld = s:taboption(aname, Value, aname .. ".user", "Account user name")
    fld.placeholder = "user"
    fld:depends(dep)

    fld = s:taboption(aname, Value, aname .. ".password", "Account password")
    fld.password = true
    fld:depends(dep)

    fld = s:taboption(aname, Flag, aname .. ".tls", "Enable TLS/SSL encryption")
    fld.default = "off"
    fld.disabled = "off"
    fld.enabled = "on"
    dep = {}
    dep[depbase .. fld.option] = "on"

    fld = s:taboption(aname, Flag, aname .. ".tls_starttls", "Use STARTTLS",
      "If connection is TLS this should be checked, if it is SSL leave unchecked")
    fld.default = "on"
    fld.disabled = "off"
    fld.enabled = "on"
    fld.rmempty = false
    fld:depends(dep)

    fld = s:taboption(aname, Flag, aname .. ".tls_certcheck", "Verify server certificate",
      "Requires TLS trust file")
    fld.default = "off"
    fld.disabled = "off"
    fld.enabled = "on"
    fld.rmempty = false
    fld:depends(dep)
    dep = {}
    dep[depbase .. fld.option] = "on"

    fld = s:taboption(aname, Value, aname .. ".tls_trust_file", "TLS trust file bundle")
    fld.default = "/etc/ssl/certs/ca-certificates.crt"
    fld:depends(dep)
  end -- for account
end -- buildAccountTabs()

sortAccounts()
buildAccountTabs()

function m.handle(self, state, data)
  if state == FORM_VALID then 
    -- Convert the conf table into a list of strings 
    local newconf = {} 
    local reconf = {}
    local formvalues = luci.http.formvalue()
    local selected_tab = formvalues["tab.msmtp.Accounts"]

    for _,v in pairs(conf) do
      -- This prevents writing two accounts with the same name
      v = conf[v.account] or v

      if accountrenames[v.account] == selected_tab then
        formvalues["tab.msmtp.Accounts"] = v.account or "default"
      end

      if v.account then
        -- Account has to be the first line of the config section
        newconf[#newconf+1] = "account " .. v.account
        for opt, val in pairs(v) do
          if opt ~= "account" then
            newconf[#newconf+1] = opt .. " " .. val
          end
        end -- for opt in account
        newconf[#newconf+1] = ""

        reconf[v.account] = v
     end -- if not blank account
    end -- for each account

    conf = reconf
    sortAccounts()
    buildAccountTabs()

    nixio.fs.writefile("/etc/msmtprc", table.concat(newconf, "\n"))
  end -- if form valid
end

return m
