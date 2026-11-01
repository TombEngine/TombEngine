-- ldignore
local PhotoModeSetup = require("PhotoModeSetup")

local Expressions =
{
    { name = "Default", objID = nil, meshIndices = {} },
    { name = "Scream", objID = TEN.Objects.ObjID.LARA_SCREAM, meshIndices = {14} }
}

for _, expression in ipairs(PhotoModeSetup.Expressions) do
    table.insert(Expressions, expression)
end

return Expressions