local hs = game:GetService("HttpService")
local target = "__TARGET__"
local max_depth = __MAX_DEPTH__

local total_instances = 0

local function serialize_instance(inst, depth)
    if not inst or depth > max_depth then return nil end
    total_instances = total_instances + 1

    local item = {
        name = inst.Name,
        className = inst.ClassName,
        path = inst:GetFullName()
    }

    local props = {}
    if inst:IsA("ValueBase") then
        pcall(function() props.Value = tostring(inst.Value) end)
    elseif inst:IsA("RemoteEvent") or inst:IsA("UnreliableRemoteEvent") then
        props.isRemoteEvent = true
    elseif inst:IsA("RemoteFunction") then
        props.isRemoteFunction = true
    elseif inst:IsA("ModuleScript") then
        props.isModule = true
    end

    local attr = inst:GetAttributes()
    if next(attr) then
        props.attributes = attr
    end
    item.properties = props

    local children = {}
    for _, child in ipairs(inst:GetChildren()) do
        local ser = serialize_instance(child, depth + 1)
        if ser then
            table.insert(children, ser)
        end
    end
    if #children > 0 then
        item.children = children
    end
    return item
end

local dump_data = {}

if target == "all" or target == "ReplicatedStorage" then
    local rep = game:GetService("ReplicatedStorage")
    dump_data.ReplicatedStorage = serialize_instance(rep, 1)
end

if target == "all" or target == "StarterGui" then
    local sg = game:GetService("StarterGui")
    dump_data.StarterGui = serialize_instance(sg, 1)
end

if target == "all" or target == "StarterPlayer" then
    local sp = game:GetService("StarterPlayer")
    dump_data.StarterPlayer = serialize_instance(sp, 1)
end

if target == "all" or target == "Workspace" then
    local ws = workspace
    local ws_dump = {
        name = "Workspace",
        className = "Workspace",
        children = {}
    }
    for _, child in ipairs(ws:GetChildren()) do
        if not child:IsA("Terrain") and not child:IsA("Camera") then
            local s = serialize_instance(child, 1)
            if s then table.insert(ws_dump.children, s) end
        end
    end
    dump_data.Workspace = ws_dump
end

return hs:JSONEncode({
    timestamp = os.time(),
    target = target,
    maxDepth = max_depth,
    totalInstances = total_instances,
    dump = dump_data
})
