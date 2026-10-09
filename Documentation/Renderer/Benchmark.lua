-- Renderer benchmark scenario: keeps the player alive and slowly turns her so the
-- chase camera sweeps the whole scene around the start position.

local frame = 0

LevelFuncs.OnStart = function() end
LevelFuncs.OnLoad = function() end
LevelFuncs.OnSave = function() end
LevelFuncs.OnEnd = function() end

LevelFuncs.OnLoop = function()
	frame = frame + 1
	Lara:SetHP(1000)
	Lara:SetRotation(Rotation.new(0, (frame * 0.75) % 360, 0))

	-- A shadow casting light near the player exercises the shadow map.
	TEN.Effects.EmitLight(TEN.View.GetCameraPosition() + Vec3(0, -768, 0), TEN.Color(255, 230, 200), 24, true, "BenchmarkShadowLight")

	-- A colored fog bulb in front of the player exercises per vertex fog.
	TEN.Effects.EmitFogBulb(Lara:GetPosition() + Vec3(0, -512, 0), 6, 40, TEN.Color(255, 80, 40), "BenchmarkFogBulb")

	-- Particles exercise the sprite paths: additive (instanced) and alpha blended (sorted).
	local pos = Lara:GetPosition()
	for i = 0, 3 do
		local angle = (frame * 7 + i * 90) % 360
		local offset = Vec3(math.sin(math.rad(angle)) * 768, -512 - i * 128, math.cos(math.rad(angle)) * 768)
		local blend = (i % 2 == 0) and TEN.Effects.BlendID.ADDITIVE or TEN.Effects.BlendID.ALPHA_BLEND
		TEN.Effects.EmitParticle(pos + offset, Vec3(0, -4, 0), 0, 2, 30, TEN.Color(255, 160, 64), TEN.Color(64, 64, 255), blend, 64, 16, 3)
	end

	-- A global lens flare slightly above the camera direction exercises the lens flare post-process.
	local cameraPos = TEN.View.GetCameraPosition()
	local cameraTarget = TEN.View.GetCameraTarget()
	local flare = TEN.Flow.GetCurrentLevel().lensFlare
	flare.enabled = true
	flare.pitch = 0
	flare.color = TEN.Color(255, 255, 255)
	flare.yaw = math.deg(math.atan(cameraTarget.x - cameraPos.x, cameraTarget.z - cameraPos.z)) + 15

	-- A large distortion particle in front of the player exercises the distortion pass.
	TEN.Effects.EmitParticle(pos + Vec3(0, -640, 0), Vec3(0, 0, 0), 0, 0, 0, TEN.Color(255, 255, 255), TEN.Color(255, 255, 255), TEN.Effects.BlendID.DISTORTION, 256, 256, 1)
end
