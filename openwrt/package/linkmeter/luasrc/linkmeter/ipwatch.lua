local uci = require "uci"
local sys = require "luci.sys"
local nixio = require "nixio"

module("linkmeter.ipwatch", package.seeall)

local IP_CHECK_INTERVAL = 60

local function postDeviceData(dd)
  if uci.cursor():get("linkmeter", "live", "optout") == "1" then
    return
  end
  cpuinfo = {}
  for line in io.lines("/proc/cpuinfo") do
    local iPos = line:find(":")
    if iPos then
      cpuinfo[line:sub(1, iPos-1):gsub("%s+$", "")] = line:sub(iPos+1):gsub("^%s+","")
    end
  end

  local hardware, revision, serial
  -- BCM2708 BCM2709 BCM2835
  if (cpuinfo['Hardware'] or ''):find('BCM2%d%d%d$') ~= nil then
    hardware = cpuinfo['Hardware']
    revision = cpuinfo['Revision']
    serial = cpuinfo['Serial']
  elseif cpuinfo['system type'] == 'Ralink SoC' then
    hardware = cpuinfo['system type']
    revision = cpuinfo['cpu model']
    serial = ""
  end

  local uptime = sys.uptime()
  local hostname = sys.hostname()
  dd = dd ..
    ('],"serial":"%s","revision":"%s","model":"%s","uptime":%s,"hostname":"%s"}'):format(
    serial, revision, hardware, uptime, hostname)

  os.execute(("(" ..
    "echo -1 > /tmp/hmlivereg ;" ..
    "curl --silent -o /dev/null -d devicedata='%s' %s ; " ..
    "echo $? > /tmp/hmlivereg" ..
    ")&"):format(dd, 'https://heatermeter.com/devices/'))
end

local lastIpCheck
local lastIfaceHash
local lastIfaceHashTime
local function checkIpUpdate(now)
  local newIp
  local ifaces = nixio.getifaddrs()
  local packets = {}
  -- First find out how many packets have been sent on each interface
  -- from the packet family instance of each adapter
  for _,v in ipairs(ifaces) do
    if not v.flags.loopback and v.family == "packet" then
      packets[v.name] = v.data.tx_packets
    end
  end

  -- Static interfaces are always 'up' just not 'running' but nixio does not
  -- have a flag for running so look for an interface that has sent packets
  local ifaceJson = '{"ifaces":['
  local ifaceHash = ""
  for _,v in ipairs(ifaces) do
    if not v.flags.loopback and v.flags.up and v.family == "inet" then
      if ifaceHash ~= "" then ifaceJson = ifaceJson .. "," end
      ifaceJson = ifaceJson ..('{"name":"%s","addr":"%s","cnt":%s}'):format(
        v.name, v.addr, packets[v.name])
      ifaceHash = ifaceHash .. v.name .. v.addr

      if packets[v.name] > 0 then
        newIp = v.addr
      end
    end
  end

  if newIp and newIp ~= lastIp then
    lastIp = newIp
    linkmeterd.toast("Network Address", newIp)
    linkmeterd.setConf("ip", newIp)
  end

  if ifaceHash ~= lastIfaceHash or now - lastIfaceHashTime > 3600 then
    lastIfaceHashTime = now
    lastIfaceHash = ifaceHash
    postDeviceData(ifaceJson)
  end
end

local function hmliveResultStr()
  local regResult = tonumber(nixio.fs.readfile("/tmp/hmlivereg") or "-2")
  if regResult == -2 then
    return "Not attempted"
  elseif regResult == -1 then
    return "In process..."
  elseif regResult == 0 then
    return "SUCCESS"
  elseif regResult == 6 then
    return "FAIL: No DNS"
  elseif regResult == 7 then
    return "FAIL: Timeout"
  elseif regResult == 51 then
    return "FAIL: SSL cert"
  else
    return "Error code " .. tostring(regResult)
  end
end

local function onTick(now)
  if now - lastIpCheck > IP_CHECK_INTERVAL then
    checkIpUpdate(now)
    lastIpCheck = now
  end
end

local HOSTBUTTON = { ENTER = 0x80, LEAVE = 0x40, TIMEOUT = 0x20, LEFT = 0x01, RIGHT = 0x02, UP = 0x04, DOWN = 0x08 }
local HOSTTOPIC = { NETINFO = 0 }
local NETINFO_MENU = { TITLE = 0, IPADDR = 1, LIVREG = 2, LAST = 2 }
local function hiNetInfo(topic, opaque, button)
  if topic ~= HOSTTOPIC.NETINFO then return end

  -- Menu transitions
  if button == HOSTBUTTON.DOWN then
    if opaque < NETINFO_MENU.LAST then
      opaque = opaque + 1
    end
  elseif button == HOSTBUTTON.UP then
    if opaque > NETINFO_MENU.TITLE then
      opaque = opaque - 1
    end
  end

  -- Menu handlers
  if opaque == NETINFO_MENU.TITLE then
    return linkmeterd.hostInteractiveReply(NETINFO_MENU.TITLE, "\002   Network   \002", "  Information  ")
  elseif opaque == NETINFO_MENU.IPADDR then
    return linkmeterd.hostInteractiveReply(opaque, "Network Address", lastIp or "Unknown")
  elseif opaque == NETINFO_MENU.LIVREG then
    return linkmeterd.hostInteractiveReply(opaque, "Device register", hmliveResultStr())
  end
end

function init()
  lastIp = nil
  -- Delay checking IP for 2 seconds
  lastIpCheck = os.time() + 2 - IP_CHECK_INTERVAL

  linkmeterd.registerTickListener(onTick)
  linkmeterd.registerHostInteractiveListener(HOSTTOPIC.NETINFO, hiNetInfo) -- HmMenuInteractiveTopic::NETINFO
end
