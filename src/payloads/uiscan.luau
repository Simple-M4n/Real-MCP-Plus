local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer
local sg = game:GetService("StarterGui")
local cg = game:GetService("CoreGui")

local INCLUDE_NIL = __INCLUDE_NIL__
local INCLUDE_COREGUI = __INCLUDE_COREGUI__
local ONLY_SUSPICIOUS = __ONLY_SUSPICIOUS__
local CHECK_CONNECTIONS = __CHECK_CONNECTIONS__
local MAX_ITEMS = __MAX_ITEMS__

local keywords = {
	"admin", "mod", "moderator", "debug", "dev", "panel", "cmd",
	"command", "ban", "kick", "staff", "secret", "test", "console",
	"giver", "spawn", "owner", "whitelist", "bypass", "freecam", "noclip"
}

local out = {
	items = {},
	total_scanned = 0,
	truncated = false,
}

local function match_kw(text)
	if not text or text == "" then return nil end
	local low = string.lower(tostring(text))
	for _, kw in ipairs(keywords) do
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

local function count_conns(inst)
	if not CHECK_CONNECTIONS or typeof(getconnections) ~= "function" then
		return nil
	end
	local res = {}
	for _, sig_name in ipairs({"Activated", "MouseButton1Click", "FocusLost", "Changed"}) do
		local ok_sig, sig = pcall(function() return inst[sig_name] end)
		if ok_sig and typeof(sig) == "RBXScriptSignal" then
			local ok_c, conns = pcall(getconnections, sig)
			if ok_c and type(conns) == "table" and #conns > 0 then
				res[sig_name] = #conns
			end
		end
	end
	return next(res) ~= nil and res or nil
end

local function is_offscreen(inst)
	if not inst:IsA("GuiObject") then return false end
	local ok, pos = pcall(function() return inst.AbsolutePosition end)
	local ok2, sz = pcall(function() return inst.AbsoluteSize end)
	if not ok or not ok2 then return false end
	if sz.X <= 1 or sz.Y <= 1 then return true end
	if pos.X < -2000 or pos.Y < -2000 or pos.X > 5000 or pos.Y > 5000 then
		return true
	end
	return false
end

local function inspect_ui(inst, origin, is_nil)
	if #out.items >= MAX_ITEMS then
		out.truncated = true
		return
	end
	if not (inst:IsA("LayerCollector") or inst:IsA("GuiObject")) then
		return
	end
	out.total_scanned += 1

	local hidden = false
	local enabled_val = nil
	local visible_val = nil
	if inst:IsA("LayerCollector") then
		local ok, en = pcall(function() return inst.Enabled end)
		if ok then
			enabled_val = en
			if en == false then hidden = true end
		end
	elseif inst:IsA("GuiObject") then
		local ok, vis = pcall(function() return inst.Visible end)
		if ok then
			visible_val = vis
			if vis == false then hidden = true end
		end
		if is_offscreen(inst) then
			hidden = true
		end
	end

	local text_val = nil
	if inst:IsA("TextLabel") or inst:IsA("TextButton") or inst:IsA("TextBox") then
		local ok, t = pcall(function() return inst.Text end)
		if ok and type(t) == "string" and #t > 0 then
			text_val = string.sub(t, 1, 80)
		end
	end

	local kw = match_kw(inst.Name) or match_kw(text_val)
	local conns = nil
	if inst:IsA("GuiButton") or inst:IsA("TextBox") then
		conns = count_conns(inst)
	end

	local keep = (not ONLY_SUSPICIOUS) or is_nil or (kw ~= nil) or (hidden and (inst:IsA("LayerCollector") or conns ~= nil or kw ~= nil))
	if not keep then return end

	table.insert(out.items, {
		origin = origin,
		is_nil = is_nil,
		class = inst.ClassName,
		name = inst.Name,
		path = full_path(inst),
		enabled = enabled_val,
		visible = visible_val,
		hidden = hidden,
		text = text_val,
		keyword = kw,
		connections = conns,
	})
end

local function scan_root(root, origin, is_nil)
	if not root then return end
	inspect_ui(root, origin, is_nil)
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for _, inst in ipairs(list) do
		if #out.items >= MAX_ITEMS then
			out.truncated = true
			break
		end
		inspect_ui(inst, origin, is_nil)
	end
end

if lp then
	pcall(function() scan_root(lp:FindFirstChild("PlayerGui"), "PlayerGui", false) end)
end
pcall(function() scan_root(sg, "StarterGui", false) end)
if INCLUDE_COREGUI then
	pcall(function() scan_root(cg, "CoreGui", false) end)
end

if INCLUDE_NIL and typeof(getnilinstances) == "function" then
	local ok_nil, nils = pcall(getnilinstances)
	if ok_nil and type(nils) == "table" then
		for _, inst in ipairs(nils) do
			if #out.items >= MAX_ITEMS then
				out.truncated = true
				break
			end
			pcall(function()
				if inst:IsA("LayerCollector") or inst:IsA("GuiObject") then
					scan_root(inst, "nil", true)
				end
			end)
		end
	end
end

return hs:JSONEncode(out)
