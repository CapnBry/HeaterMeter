require "luci.fs"
require "luci.sys"
require "luci.util"
require "lmclient"

local SCRIPT_PATH = "/usr/share/linkmeter/alarm-"
local function isexec_cfgvalue(self)
  return luci.fs.access(SCRIPT_PATH .. self.fname, "x") and "1"
end

local function isexec_write(self, section, value)
  print("Write " .. self.fname .. " " .. value)
  local curmode = luci.fs.stat(SCRIPT_PATH .. self.fname).modedec
  value = value == "1" and "755" or "644"
  if value ~= mode then
    luci.fs.chmod(SCRIPT_PATH .. self.fname, value)
  end
end

local function script_cfgvalue(self)
  return luci.fs.readfile(SCRIPT_PATH .. self.fname) or ""
end

local function script_write(self, section, value)
  -- BRY: Note to self. If you make one big form out of the page,
  -- value becomes an indexed table with a string entry for each textbox
  local old = self:cfgvalue()
  value = value:gsub("\r\n", "\n")
  if old ~= value then
    luci.fs.writefile(SCRIPT_PATH .. self.fname, value)
  end
end

local scriptitems = {
  { fname = "all", title = "All Alarm Script", desc =
    [[This script is run when HeaterMeter signals any alarm before any
    specific script below is executed. If the 'All' script returns non-zero
    the alarm-specific script will not be run.]] },
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

  local fld = f:field(Flag, "isexec", "Execute on alarm")
    fld.fname = item.fname
    fld.default = "0"
    fld.rmempty = nil
    fld.cfgvalue = isexec_cfgvalue
    fld.write = isexec_write

  fld = f:field(TextValue, "script")
    fld.fname = item.fname
    fld.rmempty = nil
    fld.optional = true
    fld.rows = 10
    fld.cfgvalue = script_cfgvalue
    fld.write = script_write
        
  retVal[#retVal+1] = f
end -- for file

return unpack(retVal)
                                                                                   
