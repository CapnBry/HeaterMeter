local CSS_PATH = "/usr/share/linkmeter/user.css"
local f = SimpleForm("lmuser", "User Style",
  "Override the look of the Home page by adding your own CSS here")
  
local fld = f:field(TextValue, "lmuser")
  fld.optional = true
  fld.rows = 24
  fld.cfgvalue = function (self)
    return nixio.fs.readfile(CSS_PATH) or ""
  end
  fld.write = function(self, section, value)
    local old = self:cfgvalue()
    value = value:gsub("\r\n", "\n")
    if old ~= value then
      nixio.fs.writefile(CSS_PATH, value)
    end
  end
  fld.remove = function(self, section)
    nixio.fs.unlink(CSS_PATH)
  end
  
return f