require "luci.fs"
require "luci.sys"
require "lmclient"

local SCRIPT_PATH = "/usr/share/linkmeter/alarm-"
local function isexec_cfgvalue(self)
  return luci.fs.access(SCRIPT_PATH .. self.config, "x") and "1"
end

local function isexec_write(self, section, value)
  self.value = value
  local curmode = luci.fs.stat(SCRIPT_PATH .. self.config)
  value = value == "1" and 755 or 644
  if curmode and curmode.modedec ~= value then
    luci.fs.chmod(SCRIPT_PATH .. self.config, value)
  end
end

local function script_cfgvalue(self)
  return luci.fs.readfile(SCRIPT_PATH .. self.config) or ""
end

local function script_write(self, section, value)
  -- BRY: Note to self. If you make one big form out of the page,
  -- value becomes an indexed table with a string entry for each textbox
  local old = self:cfgvalue()
  value = value:gsub("\r\n", "\n")
  if old ~= value then
    luci.fs.writefile(SCRIPT_PATH .. self.config, value)
    -- If there was no file previously re-call the isexec handler
    -- as it executes before this handler and there was not a file then
    if old == "" then self.isexec:write(section, self.isexec.value) end
  end
end

local scriptitems = {
  { fname = "all", title = "All Alarm Script", desc =
    [[This script is run when HeaterMeter signals any alarm before any
    specific script below is executed. If the 'All' script returns non-zero
    the alarm-specific script will not be run.
    <a href="https://github.com/CapnBry/HeaterMeter/wiki/Alarm-Script-Recipes"
     target="wiki">
    Example scripts</a>
    can be found in the HeaterMeter wiki.
    ]] },
  } 

--local lm = LmClient() 
for i = 0, 3 do
  local pname = "" -- lm:query("$LMGT,pn"..i)
  if pname ~= "" then pname = " (" .. pname .. ")" end
  
  for _, j in pairs({ "Low", "High" }) do
    scriptitems[#scriptitems+1] = { fname = i..j:sub(1,1), 
      title = "Probe " .. i .. " " .. j .. pname }
  end
end
--lm:close()
 
local retVal = {}
for i, item in pairs(scriptitems) do
  local f = SimpleForm(item.fname, item.title, item.desc)

  local isexec = f:field(Flag, "isexec", "Execute on alarm")
    isexec.default = "0"
    isexec.rmempty = nil
    isexec.cfgvalue = isexec_cfgvalue
    isexec.write = isexec_write

  local fld = f:field(TextValue, "script")
    fld.isexec = isexec
    fld.rmempty = nil
    fld.optional = true
    fld.rows = 10
    fld.cfgvalue = script_cfgvalue
    fld.write = script_write
        
  retVal[#retVal+1] = f
end -- for file

return unpack(retVal)
                                                                                   
