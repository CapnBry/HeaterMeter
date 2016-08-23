#!/usr/bin/lua
require("nixio")
require("luci.util")

LmClient = luci.util.class()

-- Must match send size if messages exceed this size
local RECVSIZE = 8192

function LmClient._connect(self)
  if self.sock then return true end
 
  local sock = nixio.socket("unix", "dgram")
  -- autobind to an abstract address, optionally I could explicitly specify
  -- an abstract name such as "\0linkmeter"..pid
  -- Abstract socket namespace is only supported on Linux
  if not sock:bind("") then
    sock:close()
    return nil, "bind"
  end
  if not sock:connect("/var/run/linkmeter.sock") then
    sock:close()
    return nil, "connect"
  end
  
  self.sock = sock
  return true
end

function LmClient.close(self)
  if self.sock then
    self.sock:close()
    self.sock = nil
  end
end

function LmClient.query(self, qry, keepopen)
  local rc = {self:_connect()}
  if not rc[1] then return unpack(rc) end
  
  if self.sock:send(qry) == 0 then
    return nil, "send"
  end
  
  local polle = { { fd = self.sock, events = nixio.poll_flags("in") } }
  local r
  while true do
    if nixio.poll(polle, 1500) then
      local l = self.sock:recv(RECVSIZE)
      r = (r or "") .. l
      if #l < RECVSIZE then break end
    else
      r = r or { nil, "poll" }
      break
    end
  end
  
  if not keepopen then self:close() end
  if type(r) == "table" then
    return unpack(r)
  else
    return r
  end
end

function LmClient.stream(self, qry, fn)
  local rc = {self:_connect()}
  if not rc[1] then return unpack(rc) end

  if self.sock:send(qry) == 0 then
    return nil, "send"
  end
  
  local polle = { { fd = self.sock, events = nixio.poll_flags("in") } }
  while nixio.poll(polle, 7500) do
    local r = self.sock:recv(RECVSIZE)
    if r == "ERR" then break end
    fn(r)
  end
 
  self:close() 
  return nil, "eof"
end
          
-- Command line execution
if arg and arg[0]:find("lmclient") then
  local qry = arg[1] or "$LMSU"
  local strm
  if qry:sub(1,1) == "@" then
    strm = true
    qry = qry:sub(2)
  end
  if qry:sub(1,1) ~= "$" then 
    qry = "$" .. qry
  end
  
  if strm then
    print(LmClient:stream(qry, function (r) print(r) end))
  else
    print(LmClient():query(qry))
  end
end
