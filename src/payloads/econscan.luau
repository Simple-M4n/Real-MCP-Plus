local hs = game:GetService("HttpService")
local players = game:GetService("Players")
local lp = players.LocalPlayer

local MAX_ITEMS = __MAX_ITEMS__

local econ_keywords = {
	"cash", "coin", "money", "gem", "gold", "diamond", "token", "point",
	"credit", "balance", "bank", "shop", "store", "market", "buy", "sell",
	"purchase", "price", "cost", "reward", "daily", "redeem", "code",
	"trade", "rebirth", "prestige", "gamepass", "product", "robux", "vip"
}

local out = {
	leaderstats = {},
	economy_values = {},
	economy_ui = {},
	total_found = 0,
}

local function match_kw(s)
	if not s then return nil end
	local low = string.lower(tostring(s))
	for _, kw in ipairs(econ_keywords) do
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

if lp then
	local ls = lp:FindFirstChild("leaderstats")
	if ls then
		for _, child in ipairs(ls:GetChildren()) do
			local ok_v, val = pcall(function() return child.Value end)
			table.insert(out.leaderstats, {
				name = child.Name,
				class = child.ClassName,
				value = ok_v and tostring(val) or nil,
			})
		end
	end

	for _, child in ipairs(lp:GetChildren()) do
		if child:IsA("Folder") or child:IsA("Configuration") then
			for _, sub in ipairs(child:GetDescendants()) do
				if sub:IsA("ValueBase") and match_kw(sub.Name) and #out.economy_values < MAX_ITEMS then
					local ok_v, val = pcall(function() return sub.Value end)
					out.total_found += 1
					table.insert(out.economy_values, {
						name = sub.Name,
						class = sub.ClassName,
						value = ok_v and tostring(val) or nil,
						path = full_path(sub),
					})
				end
			end
		end
	end

	local pgui = lp:FindFirstChild("PlayerGui")
	if pgui then
		local ok_g, desc = pcall(function() return pgui:GetDescendants() end)
		if ok_g and type(desc) == "table" then
			for i, inst in ipairs(desc) do
				if i % 4000 == 0 then task.wait() end
				if (inst:IsA("GuiButton") or inst:IsA("TextBox")) and #out.economy_ui < MAX_ITEMS then
					local txt = nil
					pcall(function() txt = inst.Text end)
					local kw = match_kw(inst.Name) or match_kw(txt)
					if kw then
						out.total_found += 1
						table.insert(out.economy_ui, {
							class = inst.ClassName,
							name = inst.Name,
							text = txt and string.sub(txt, 1, 80) or nil,
							keyword = kw,
							path = full_path(inst),
						})
					end
				end
			end
		end
	end
end

return hs:JSONEncode(out)
