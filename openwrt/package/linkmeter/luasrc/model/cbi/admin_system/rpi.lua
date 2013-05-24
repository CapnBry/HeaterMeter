require "nixio"
require "nixio.util"

local function OPT_SPLIT(o) return o:match("(%w+)\.(.+)") end

local conf = {}
local f = nixio.open("/boot/config.txt", "r")
if f then
  for line in f:linesource() do
    line = line:match("%s*(.+)")
    -- Make sure the line isn't a comment
    if line and line:sub(1, 1) ~= "#" and line:sub(1, 1) ~= ";" then
      local option = line:match("[%w_]+"):lower()
      local value = line:match(".+=(.*)")
      conf[option] = value
    end -- if line
  end -- for line
  f:close()
end
f = nixio.open("/proc/cpuinfo", "r")
if f then
  for line in f:linesource() do
    line = line:match("%s*(.+)")
    if line then
      local option = line:match("%S+"):lower()
      local value = line:match(".+:(.*)")
      if option == "serial" or option == "revision" then
        conf['cpu_' .. option] = value
      end
    end
  end
  f:close()
end

local t = nixio.fs.readfile("/sys/class/thermal/thermal_zone0/temp")
conf['cpu_temp'] = tonumber(t) and (t / 1000) .. " C" 
conf['governor'] = nixio.fs.readfile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")

local m = SimpleForm("rpi", "Raspberry Pi Configuration",
  [[Read-only for now. From /boot/config.txt
  ]])

local s = m:section(NamedSection, "Boot")
function s.cfgvalue(self, section)
  return true
end

local function getconf(k) return conf[k.option] end
for k,v in pairs(conf) do
  s:option(DummyValue, k, k).cfgvalue = getconf
  
end -- for account

table.sort(s.children, function (a,b) return a.title < b.title end)
  
return m
