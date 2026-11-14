local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local INCLUDE_STORAGE = __INCLUDE_STORAGE__
local CHECK_CONNECTIONS = __CHECK_CONNECTIONS__
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	tools = {},
	total_found = 0,
	truncated = false,
}

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function sig_count(inst, sig_name)
	if not CHECK_CONNECTIONS or typeof(getconnections) ~= "function" then return nil end
	local ok_s, sig = pcall(function() return inst[sig_name] end)
	if not ok_s or typeof(sig) ~= "RBXScriptSignal" then return nil end
	local ok_c, conns = pcall(getconnections, sig)
	if ok_c and type(conns) == "table" then return #conns end
	return nil
end

local seen = {}
local function collect_tools(root, origin)
	if not root then return end
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for _, inst in ipairs(list) do
		if inst:IsA("Tool") or inst:IsA("HopperBin") then
			local path = full_path(inst)
			if not seen[path] then
				seen[path] = true
				out.total_found += 1
				if #out.tools < MAX_ITEMS then
					local remotes = {}
					local scripts = {}
					local values = {}
					for _, sub in ipairs(inst:GetDescendants()) do
						if sub:IsA("RemoteEvent") or sub:IsA("RemoteFunction") then
							table.insert(remotes, sub.Name .. " (" .. sub.ClassName .. ")")
						elseif sub:IsA("LuaSourceContainer") then
							table.insert(scripts, sub.Name .. " (" .. sub.ClassName .. ")")
						elseif sub:IsA("ValueBase") then
							local ok_v, val = pcall(function() return sub.Value end)
							table.insert(values, sub.Name .. "=" .. tostring(ok_v and val or "?"))
						end
					end

					local entry = {
						origin = origin,
						name = inst.Name,
						class = inst.ClassName,
						path = path,
						remotes = remotes,
						scripts = scripts,
						values = values,
					}
					if inst:IsA("Tool") then
						entry.requires_handle = inst.RequiresHandle
						entry.can_be_dropped = inst.CanBeDropped
						entry.tooltip = inst.ToolTip
						entry.has_handle = inst:FindFirstChild("Handle") ~= nil
						entry.conns = {
							Activated = sig_count(inst, "Activated"),
							Equipped = sig_count(inst, "Equipped"),
							Unequipped = sig_count(inst, "Unequipped"),
						}
					end
					table.insert(out.tools, entry)
				else
					out.truncated = true
				end
			end
		end
	end
end

if lp then
	pcall(function() collect_tools(lp:FindFirstChild("Backpack"), "Backpack") end)
	if lp.Character then
		pcall(function() collect_tools(lp.Character, "Character") end)
	end
end
pcall(function() collect_tools(game:GetService("StarterPack"), "StarterPack") end)
if INCLUDE_STORAGE then
	pcall(function() collect_tools(game:GetService("ReplicatedStorage"), "ReplicatedStorage") end)
end

return hs:JSONEncode(out)
