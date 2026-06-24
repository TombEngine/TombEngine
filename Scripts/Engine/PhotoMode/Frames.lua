-- ldignore
local PhotoModeSetup = require("PhotoModeSetup")

local Frames =
{
    { name = "None",    spriteID = -1 },
    { name = "Cinematic Bars", spriteID = 0 },
    { name = "Tomb Engine", spriteID = 1, scaleMode = TEN.View.ScaleMode.FIT },
    { name = "Polaroid", spriteID = 2, scaleMode = TEN.View.ScaleMode.FILL }
}

for _, frame in ipairs(PhotoModeSetup.Frames) do
    table.insert(Frames, frame)
end

return Frames