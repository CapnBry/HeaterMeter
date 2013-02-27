#!/usr/bin/lua
require("nixio")
require("luci.util")

LmClient = luci.util.class()

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
  local r = {self:_connect()}
  if not r[1] then return unpack(r) end
  
  if self.sock:send(qry) == 0 then
    return nil, "send"
  end
  
  local polle = { fd = self.sock, events = nixio.poll_flags("in") }
  if nixio.poll({polle}, 1500) then
    r = self.sock:recv(8192)
  else
    r = { nil, "poll" }
  end
  
  if not keepopen then self:close() end
  if type(r) == "table" then
    return unpack(r)
  else
    return r
  end
end

function LmClient.stream(self, qry, fn)
  local r = {self:_connect()}
  if not r[1] then return unpack(r) end

  if self.sock:send(qry) == 0 then
    return nil, "send"
  end
  
  local polle = { fd = self.sock, events = nixio.poll_flags("in") }
  while nixio.poll({polle}, 7500) do
    r = self.sock:recv(8192)
    if r == "ERR" then break end
    fn(r)
  end
 
  self:close() 
  return nil, "eof"
end
          
-- Command line execution
if arg then
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
