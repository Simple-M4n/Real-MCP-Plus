local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local INCLUDE_SOUNDS = __INCLUDE_SOUNDS__
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	playing_tracks = {},
	joints = {},
	animations = {},
	sounds = {},
}

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

if lp and lp.Character then
	local char = lp.Character
	local hum = char:FindFirstChildOfClass("Humanoid")
	local animator = hum and hum:FindFirstChildOfClass("Animator")
	if animator then
		local ok, tracks = pcall(function() return animator:GetPlayingAnimationTracks() end)
		if ok and type(tracks) == "table" then
			for _, tr in ipairs(tracks) do
				table.insert(out.playing_tracks, {
					name = tr.Name,
					animation_id = tr.Animation and tr.Animation.AnimationId or nil,
					speed = tr.Speed,
					priority = tostring(tr.Priority),
					looped = tr.Looped,
					length = tr.Length,
				})
			end
		end
	end

	for _, inst in ipairs(char:GetDescendants()) do
		if inst:IsA("Motor6D") then
			table.insert(out.joints, {
				name = inst.Name,
				part0 = inst.Part0 and inst.Part0.Name or nil,
				part1 = inst.Part1 and inst.Part1.Name or nil,
				path = full_path(inst),
			})
		end
	end
end

local seen_anim = {}
local seen_sound = {}
for _, svc_name in ipairs({"ReplicatedStorage", "StarterPack", "StarterGui", "Workspace"}) do
	pcall(function()
		local svc = game:GetService(svc_name)
		local list = svc:GetDescendants()
		for i, inst in ipairs(list) do
			if i % 4000 == 0 then task.wait() end
			if inst:IsA("Animation") and #out.animations < MAX_ITEMS then
				local id = inst.AnimationId
				if id ~= "" and not seen_anim[id] then
					seen_anim[id] = true
					table.insert(out.animations, {
						name = inst.Name,
						animation_id = id,
						path = full_path(inst),
					})
				end
			elseif INCLUDE_SOUNDS and inst:IsA("Sound") and #out.sounds < MAX_ITEMS then
				local id = inst.SoundId
				if id ~= "" and not seen_sound[id] then
					seen_sound[id] = true
					table.insert(out.sounds, {
						name = inst.Name,
						sound_id = id,
						playing = inst.IsPlaying,
						path = full_path(inst),
					})
				end
			end
		end
	end)
end

return hs:JSONEncode(out)
