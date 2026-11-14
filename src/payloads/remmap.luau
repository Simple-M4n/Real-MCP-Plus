local hs = game:GetService("HttpService")

local INCLUDE_NIL = __INCLUDE_NIL__
local INCLUDE_BINDABLES = __INCLUDE_BINDABLES__
local FILTER = string.lower(__FILTER__)
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	items = {},
	total_found = 0,
	truncated = false,
}

local seen = {}

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function is_target_remote(inst)
	if inst:IsA("RemoteEvent") or inst:IsA("RemoteFunction") or inst:IsA("UnreliableRemoteEvent") then
		return true
	end
	if INCLUDE_BINDABLES and (inst:IsA("BindableEvent") or inst:IsA("BindableFunction")) then
		return true
	end
	return false
end

local function on_client_count(inst)
	if typeof(getconnections) ~= "function" then return nil end
	if inst:IsA("RemoteEvent") or inst:IsA("UnreliableRemoteEvent") then
		local ok, conns = pcall(getconnections, inst.OnClientEvent)
		if ok and type(conns) == "table" then
			return #conns
		end
	end
	return nil
end

local function record(inst, origin, is_nil)
	if not is_target_remote(inst) then return end
	local path = full_path(inst)
	local key = (is_nil and "nil:" or "g:") .. path
	if seen[key] then return end
	seen[key] = true

	if #FILTER > 0 then
		local hay = string.lower(inst.Name .. " " .. path)
		if not string.find(hay, FILTER, 1, true) then
			return
		end
	end

	out.total_found += 1
	if #out.items >= MAX_ITEMS then
		out.truncated = true
		return
	end

	table.insert(out.items, {
		origin = origin,
		is_nil = is_nil,
		class = inst.ClassName,
		name = inst.Name,
		path = path,
		on_client_conns = on_client_count(inst),
	})
end

local function scan_root(root, origin, is_nil)
	if not root then return end
	record(root, origin, is_nil)
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for _, inst in ipairs(list) do
		record(inst, origin, is_nil)
	end
end

local ok_svcs, svcs = pcall(function() return game:GetChildren() end)
if ok_svcs and type(svcs) == "table" then
	for _, svc in ipairs(svcs) do
		pcall(function() scan_root(svc, svc.Name, false) end)
	end
end

if INCLUDE_NIL and typeof(getnilinstances) == "function" then
	local ok_nil, nils = pcall(getnilinstances)
	if ok_nil and type(nils) == "table" then
		for _, inst in ipairs(nils) do
			pcall(function() scan_root(inst, "nil", true) end)
		end
	end
end

return hs:JSONEncode(out)
