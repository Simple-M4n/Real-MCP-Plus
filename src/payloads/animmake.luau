local hs = game:GetService("HttpService")
local run = game:GetService("RunService")
local lp = game:GetService("Players").LocalPlayer

local ACTION = __ACTION__
local ANIM_ID = __ANIM_ID__
local LOOP = __LOOP__
local SPEED = __SPEED__
local KEYFRAMES_JSON = __KEYFRAMES_JSON__

local env = getgenv and getgenv() or _G

local function stop_existing()
	local st = env.__REALMCP_ANIM
	if type(st) == "table" then
		if st.conn then
			pcall(function() st.conn:Disconnect() end)
		end
		if st.track then
			pcall(function() st.track:Stop() end)
		end
		if type(st.joints) == "table" then
			for _, m in pairs(st.joints) do
				pcall(function() m.Transform = CFrame.identity end)
			end
		end
	end
	env.__REALMCP_ANIM = nil
end

if ACTION == "stop" then
	stop_existing()
	return hs:JSONEncode({ status = "stopped" })
end

if not lp or not lp.Character then
	return hs:JSONEncode({ error = "no_character" })
end

local char = lp.Character
local hum = char:FindFirstChildOfClass("Humanoid")

if ACTION == "play_id" then
	stop_existing()
	if not hum then
		return hs:JSONEncode({ error = "no_humanoid" })
	end
	local animator = hum:FindFirstChildOfClass("Animator") or Instance.new("Animator", hum)
	local anim = Instance.new("Animation")
	local id_str = tostring(ANIM_ID)
	if not string.find(id_str, "rbxassetid://", 1, true) then
		id_str = "rbxassetid://" .. id_str
	end
	anim.AnimationId = id_str
	local ok, track = pcall(function() return animator:LoadAnimation(anim) end)
	if not ok or not track then
		return hs:JSONEncode({ error = "load_animation_failed: " .. tostring(track) })
	end
	track.Looped = LOOP
	track:Play(0.1, 1, SPEED)
	env.__REALMCP_ANIM = { track = track }
	return hs:JSONEncode({
		status = "playing_id",
		animation_id = id_str,
		fe_replicated = true,
	})
elseif ACTION == "play" then
	stop_existing()
	local ok_kf, keyframes = pcall(function() return hs:JSONDecode(KEYFRAMES_JSON) end)
	if not ok_kf or type(keyframes) ~= "table" or #keyframes == 0 then
		return hs:JSONEncode({ error = "invalid_or_empty_keyframes" })
	end

	local joints = {}
	for _, inst in ipairs(char:GetDescendants()) do
		if inst:IsA("Motor6D") then
			joints[inst.Name] = inst
		end
	end

	local state = { joints = joints, conn = nil }
	local idx = 1
	local elapsed = 0
	local prev_pose = {}

	local function pose_cframe(arr)
		if type(arr) ~= "table" then return CFrame.identity end
		local rx = math.rad(tonumber(arr[1]) or 0)
		local ry = math.rad(tonumber(arr[2]) or 0)
		local rz = math.rad(tonumber(arr[3]) or 0)
		return CFrame.Angles(rx, ry, rz)
	end

	state.conn = run.Stepped:Connect(function(_, dt)
		local kf = keyframes[idx]
		if not kf then
			stop_existing()
			return
		end
		local dur = math.max(tonumber(kf.duration) or 0.3, 0.02)
		elapsed += dt * SPEED
		local alpha = math.clamp(elapsed / dur, 0, 1)

		local target_joints = kf.joints or {}
		for jname, motor in pairs(joints) do
			local from_cf = prev_pose[jname] or CFrame.identity
			local to_cf = target_joints[jname] and pose_cframe(target_joints[jname]) or CFrame.identity
			motor.Transform = from_cf:Lerp(to_cf, alpha)
		end

		if alpha >= 1 then
			for jname, _ in pairs(joints) do
				prev_pose[jname] = target_joints[jname] and pose_cframe(target_joints[jname]) or CFrame.identity
			end
			elapsed = 0
			idx += 1
			if idx > #keyframes then
				if LOOP then
					idx = 1
				else
					stop_existing()
				end
			end
		end
	end)

	env.__REALMCP_ANIM = state
	return hs:JSONEncode({
		status = "playing_keyframes",
		keyframe_count = #keyframes,
		joint_count = (function() local c = 0 for _ in pairs(joints) do c += 1 end return c end)(),
		fe_replicated = false,
		note = "Procedural Motor6D.Transform keyframes run client-side; use action='play_id' with a Roblox AnimationId for engine-replicated FE playback.",
	})
end

return hs:JSONEncode({ error = "unknown_action" })
