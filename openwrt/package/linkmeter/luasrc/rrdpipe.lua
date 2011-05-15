local nixio = require("nixio")
local getfenv, setmetatable, print = getfenv, setmetatable, print

module(...)

function new(db)
  if db == nil then return nil end
  local fdr, fdw = nixio.pipe()
  local pid = nixio.fork()
  
  if pid > 0 then -- parent
    fdr:close();
    return setmetatable({pid=pid, fdw=fdw, db=db}, {__index=getfenv()})
  elseif pid == 0 then -- child
    devnul = nixio.open("/dev/null", nixio.open_flags("wronly"))
    nixio.dup(devnul, nixio.stdout)
    nixio.dup(devnul, nixio.stderr)
    nixio.dup(fdr, nixio.stdin)
    devnul:close()
    fdw:close()
    fdr:close() 
    nixio.execp('rrdtool', '-')
  end
end    

local rrdCmd = function(obj, verb, spec)
  local cmd = verb.." "..obj.db.." "..spec.."\n"
  obj.fdw:write(cmd)
end

function create(obj, spec)
  rrdCmd(obj, "create", spec)
end

function update(obj, spec)
  rrdCmd(obj, "update", spec)
end

function close(obj)
  obj.fdw:close()
  return nixio.waitpid(obj.pid)
end
