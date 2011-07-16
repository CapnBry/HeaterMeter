require("nixio")

local client = nixio.socket("unix", "dgram")
  -- autobind to an abstract address, optionally I could explicitly specify
  -- and abstract name such as "\0linkmeter"..pid 
  -- Abstract socket namespace is only supported on Linux
client:bind("")
if not client:connect("/var/run/linkmeter.sock") then
  print("No connect")
  client:close()
  return false
end

local bytes = client:send(#arg > 0 and "$"..arg[1] or "$LMSU")

local polle = { fd = client, events = nixio.poll_flags("in"), revents = 0 }
if nixio.poll({polle}, 1000) then
  local inmsg = client:recv(1024)
  print(inmsg)
else
  print("timeout")
end
client:close()
