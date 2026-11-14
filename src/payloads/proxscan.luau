local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local INCLUDE_PROMPTS = __INCLUDE_PROMPTS__
local INCLUDE_CLICKS = __INCLUDE_CLICKS__
local INCLUDE_TOUCH = __INCLUDE_TOUCH__
local INCLUDE_SEATS = __INCLUDE_SEATS__
local CHECK_CONNECTIONS = __CHECK_CONNECTIONS__
local SORT_BY_DISTANCE = __SORT_BY_DISTANCE__
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	items = {},
	total_found = 0,
	truncated = false,
	player_pos = nil,
}

local root_pos = nil
if lp and lp.Character then
	local hrp = lp.Character:FindFirstChild("HumanoidRootPart")
	if hrp and hrp:IsA("BasePart") then
		root_pos = hrp.Position
		out.player_pos = {
			x = math.floor(root_pos.X * 100 + 0.5) / 100,
			y = math.floor(root_pos.Y * 100 + 0.5) / 100,
			z = math.floor(root_pos.Z * 100 + 0.5) / 100,
		}
	end
end

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function get_world_pos(inst)
	local target = inst
	if inst:IsA("TouchTransmitter") or inst:IsA("ClickDetector") or inst:IsA("ProximityPrompt") then
		target = inst.Parent
	end
	if target then
		if target:IsA("Attachment") then
			local ok, wp = pcall(function() return target.WorldPosition end)
			if ok then return wp end
		elseif target:IsA("BasePart") then
			return target.Position
		elseif target:IsA("Model") then
			local ok, pivot = pcall(function() return target:GetPivot().Position end)
			if ok then return pivot end
		end
	end
	return nil
end

local function sig_count(inst, sig_name)
	if not CHECK_CONNECTIONS or typeof(getconnections) ~= "function" or not inst then
		return nil
	end
	local ok_s, sig = pcall(function() return inst[sig_name] end)
	if not ok_s or typeof(sig) ~= "RBXScriptSignal" then return nil end
	local ok_c, conns = pcall(getconnections, sig)
	if ok_c and type(conns) == "table" then
		return #conns
	end
	return nil
end

local selectors = {}
if INCLUDE_PROMPTS then table.insert(selectors, "ProximityPrompt") end
if INCLUDE_CLICKS then table.insert(selectors, "ClickDetector") end
if INCLUDE_TOUCH then table.insert(selectors, "TouchTransmitter") end
if INCLUDE_SEATS then
	table.insert(selectors, "Seat")
	table.insert(selectors, "VehicleSeat")
end

local matched = {}
if #selectors > 0 then
	local selector = table.concat(selectors, ", ")
	local ok_q, qres = pcall(function()
		return workspace:QueryDescendants(selector)
	end)
	if ok_q and type(qres) == "table" then
		matched = qres
	else
		local ok_d, desc = pcall(function() return workspace:GetDescendants() end)
		if ok_d and type(desc) == "table" then
			for i, inst in ipairs(desc) do
				if i % 4000 == 0 then
					task.wait()
				end
				if (INCLUDE_PROMPTS and inst:IsA("ProximityPrompt"))
					or (INCLUDE_CLICKS and inst:IsA("ClickDetector"))
					or (INCLUDE_TOUCH and inst:IsA("TouchTransmitter"))
					or (INCLUDE_SEATS and (inst:IsA("Seat") or inst:IsA("VehicleSeat")))
				then
					table.insert(matched, inst)
				end
			end
		end
	end
end

local candidates = {}
for _, inst in ipairs(matched) do
	local pos = get_world_pos(inst)
	local dist = nil
	if pos and root_pos then
		dist = math.floor((pos - root_pos).Magnitude * 100 + 0.5) / 100
	end
	table.insert(candidates, {
		inst = inst,
		pos = pos,
		distance = dist,
	})
end

out.total_found = #candidates

if SORT_BY_DISTANCE and root_pos then
	table.sort(candidates, function(a, b)
		local da = a.distance or math.huge
		local db = b.distance or math.huge
		return da < db
	end)
end

local limit = math.min(#candidates, MAX_ITEMS)
if #candidates > MAX_ITEMS then
	out.truncated = true
end

for i = 1, limit do
	local c = candidates[i]
	local inst = c.inst
	local entry = nil

	if inst:IsA("ProximityPrompt") then
		entry = {
			kind = "ProximityPrompt",
			name = inst.Name,
			path = full_path(inst),
			enabled = inst.Enabled,
			action_text = inst.ActionText,
			object_text = inst.ObjectText,
			hold_duration = inst.HoldDuration,
			max_distance = inst.MaxActivationDistance,
			requires_los = inst.RequiresLineOfSight,
			conns = sig_count(inst, "Triggered"),
		}
	elseif inst:IsA("ClickDetector") then
		entry = {
			kind = "ClickDetector",
			name = inst.Name,
			path = full_path(inst),
			max_distance = inst.MaxActivationDistance,
			conns = sig_count(inst, "MouseClick"),
		}
	elseif inst:IsA("TouchTransmitter") then
		local parent = inst.Parent
		entry = {
			kind = "TouchTransmitter",
			name = parent and parent.Name or inst.Name,
			path = parent and full_path(parent) or full_path(inst),
			can_touch = (parent and parent:IsA("BasePart")) and parent.CanTouch or nil,
			can_collide = (parent and parent:IsA("BasePart")) and parent.CanCollide or nil,
			conns = parent and sig_count(parent, "Touched") or nil,
		}
	elseif inst:IsA("Seat") or inst:IsA("VehicleSeat") then
		entry = {
			kind = inst.ClassName,
			name = inst.Name,
			path = full_path(inst),
			disabled = inst.Disabled,
			occupied = inst.Occupant ~= nil,
		}
	end

	if entry then
		if c.pos then
			entry.pos = {
				x = math.floor(c.pos.X * 100 + 0.5) / 100,
				y = math.floor(c.pos.Y * 100 + 0.5) / 100,
				z = math.floor(c.pos.Z * 100 + 0.5) / 100,
			}
		end
		entry.distance = c.distance
		table.insert(out.items, entry)
	end
end

return hs:JSONEncode(out)
