local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer
local run = game:GetService("RunService")

local INCLUDE_CONNECTIONS = __INCLUDE_CONNECTIONS__
local CHECK_REMOTES = __CHECK_REMOTES__

local out = {
	suspicious_scripts = {},
	nil_items = {},
	running_scripts = {},
	actors = {},
	connections = {},
	suspicious_remotes = {},
}

local ac_keywords = {
	"ac", "anti", "cheat", "detect", "flag", "ban", "kick",
	"handshake", "ping", "verify", "security", "telemetry",
	"integrity", "check", "guard", "protect", "exploit", "bypass",
	"anticheat", "antitamper", "tamper", "hook", "dex", "spy"
}

local seen_paths = {}

local function matches_kw(name)
	local low = string.lower(tostring(name))
	for _, kw in ipairs(ac_keywords) do
		if string.find(low, kw, 1, true) then
			return kw
		end
	end
	return nil
end

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function is_weird_name(name)
	if #name == 0 or name:match("^%s+$") then
		return "blank_name"
	end
	if name:match("[\0-\31\127]") then
		return "control_chars"
	end
	if #name >= 24 and name:match("^[A-Za-z0-9_]+$") and not name:match("[aeiouAEIOU]") then
		return "obfuscated_name"
	end
	return nil
end

local function record_instance(inst, origin, is_nil)
	local path = full_path(inst)
	local key = origin .. ":" .. path
	if seen_paths[key] then return end

	if inst:IsA("LuaSourceContainer") then
		local kw = matches_kw(inst.Name)
		local weird = is_weird_name(inst.Name)
		local in_actor = inst:FindFirstAncestorOfClass("Actor") ~= nil
		local unusual_loc = origin == "nil"
			or origin == "ReplicatedFirst"
			or origin == "Lighting"
			or origin == "SoundService"
			or origin == "Workspace"
			or origin == "CoreGui"
			or in_actor

		if kw or weird or unusual_loc or is_nil then
			seen_paths[key] = true
			if #out.suspicious_scripts < 300 then
				table.insert(out.suspicious_scripts, {
					origin = origin,
					is_nil = is_nil,
					in_actor = in_actor,
					class = inst.ClassName,
					name = inst.Name,
					path = path,
					reason = weird or (kw and ("keyword:" .. kw)) or (is_nil and "nil_parent") or ("location:" .. origin),
				})
			end
		end
	elseif inst:IsA("Actor") then
		seen_paths[key] = true
		if #out.actors < 100 then
			table.insert(out.actors, { origin = origin, is_nil = is_nil, name = inst.Name, path = path })
		end
	elseif CHECK_REMOTES and (
		inst:IsA("RemoteEvent")
		or inst:IsA("RemoteFunction")
		or inst:IsA("UnreliableRemoteEvent")
		or inst:IsA("BindableEvent")
		or inst:IsA("BindableFunction")
	) then
		local kw = matches_kw(inst.Name)
		local weird = is_weird_name(inst.Name)
		if kw or weird or is_nil then
			seen_paths[key] = true
			if #out.suspicious_remotes < 250 then
				table.insert(out.suspicious_remotes, {
					origin = origin,
					is_nil = is_nil,
					class = inst.ClassName,
					name = inst.Name,
					path = path,
					reason = weird or (kw and ("keyword:" .. kw)) or "nil_parent",
				})
			end
		end
	end
end

local function scan_container(root, label, is_nil)
	if not root then return end
	record_instance(root, label, is_nil)
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for _, inst in ipairs(list) do
		record_instance(inst, label, is_nil)
	end
end

-- Scan every accessible service under game
local ok_services, services = pcall(function() return game:GetChildren() end)
if ok_services and type(services) == "table" then
	for _, svc in ipairs(services) do
		local sname = svc.Name
		pcall(function()
			scan_container(svc, sname, false)
		end)
	end
end

-- Scan nil instances and all their descendants
if typeof(getnilinstances) == "function" then
	local ok_nil, nils = pcall(getnilinstances)
	if ok_nil and type(nils) == "table" then
		for _, inst in ipairs(nils) do
			if #out.nil_items < 200 and (inst:IsA("LuaSourceContainer") or inst:IsA("Actor") or inst:IsA("RemoteEvent") or inst:IsA("RemoteFunction") or inst:IsA("Folder") or inst:IsA("ScreenGui")) then
				table.insert(out.nil_items, {
					class = inst.ClassName,
					name = inst.Name,
				})
			end
			pcall(function()
				scan_container(inst, "nil", true)
			end)
		end
	end
end

-- Scan active running scripts (catches scripts without normal ancestry)
if typeof(getrunningscripts) == "function" then
	local ok_run, scripts = pcall(getrunningscripts)
	if ok_run and type(scripts) == "table" then
		for _, scr in ipairs(scripts) do
			local kw = matches_kw(scr.Name)
			local weird = is_weird_name(scr.Name)
			local no_parent = (scr.Parent == nil)
			if kw or weird or no_parent then
				if #out.running_scripts < 200 then
					table.insert(out.running_scripts, {
						class = scr.ClassName,
						name = scr.Name,
						path = full_path(scr),
						nil_parent = no_parent,
						reason = weird or (kw and ("keyword:" .. kw)) or "nil_parent",
					})
				end
			end
		end
	end
end

if INCLUDE_CONNECTIONS and typeof(getconnections) == "function" then
	local function inspect_signal(label, sig)
		if not sig then return end
		local ok, conns = pcall(getconnections, sig)
		if not ok or type(conns) ~= "table" then return end
		local total = #conns
		local lua_count = 0
		local no_source = 0
		local sources = {}
		for _, c in ipairs(conns) do
			local fn = c.Function
			if fn then
				local ok_info, src, line = pcall(debug.info, fn, "sl")
				if ok_info and src then
					lua_count += 1
					if src == "" or src == "[C]" then
						no_source += 1
					elseif #sources < 6 then
						table.insert(sources, tostring(src) .. ":" .. tostring(line))
					end
				end
			end
		end
		if total > 0 then
			table.insert(out.connections, {
				target = label,
				total = total,
				lua = lua_count,
				no_source = no_source,
				sources = sources,
			})
		end
	end

	pcall(function() inspect_signal("RunService.Heartbeat", run.Heartbeat) end)
	pcall(function() inspect_signal("RunService.Stepped", run.Stepped) end)
	pcall(function() inspect_signal("RunService.RenderStepped", run.RenderStepped) end)
	pcall(function() inspect_signal("LogService.MessageOut", game:GetService("LogService").MessageOut) end)
	pcall(function() inspect_signal("ScriptContext.Error", game:GetService("ScriptContext").Error) end)
	pcall(function() inspect_signal("CoreGui.ChildAdded", game:GetService("CoreGui").ChildAdded) end)
	pcall(function() inspect_signal("CoreGui.DescendantAdded", game:GetService("CoreGui").DescendantAdded) end)
	pcall(function() inspect_signal("Workspace.DescendantAdded", workspace.DescendantAdded) end)

	if lp then
		pcall(function() inspect_signal("LocalPlayer.Kick", lp.Kick) end)
		pcall(function() inspect_signal("LocalPlayer.CharacterAdded", lp.CharacterAdded) end)
		local char = lp.Character
		if char then
			local hum = char:FindFirstChildOfClass("Humanoid")
			local root = char:FindFirstChild("HumanoidRootPart")
			if hum then
				for _, prop in ipairs({"WalkSpeed", "JumpPower", "JumpHeight", "HipHeight", "PlatformStand", "Health"}) do
					local ok, sig = pcall(function() return hum:GetPropertyChangedSignal(prop) end)
					if ok then inspect_signal("Humanoid." .. prop, sig) end
				end
				inspect_signal("Humanoid.StateChanged", hum.StateChanged)
			end
			if root then
				for _, prop in ipairs({"CFrame", "Velocity", "AssemblyLinearVelocity"}) do
					local ok, sig = pcall(function() return root:GetPropertyChangedSignal(prop) end)
					if ok then inspect_signal("HumanoidRootPart." .. prop, sig) end
				end
			end
			inspect_signal("Character.ChildAdded", char.ChildAdded)
			inspect_signal("Character.DescendantAdded", char.DescendantAdded)
		end
	end
end

return hs:JSONEncode(out)
