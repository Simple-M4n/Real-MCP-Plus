local args = {...}
local input = args[1] or ""
if input == "" then
    return '{"error":"Input script is empty"}'
end

local function decode_wearedevs(src)
    local header = src:match("%-%-%[%[[^\n]*wearedevs%.net/obfuscator[^\n]*%]%]")
    if not header and not src:find("wearedevs", 1, true) then
        return nil
    end

    local string_matches = {}
    for str in src:gmatch('"([^"\\]*(\\.[^"\\]*)*)"') do
        if #str > 1 and str:sub(1,1) == "1" then
            table.insert(string_matches, str)
        end
    end

    local call_sources = {}
    for load_call in src:gmatch('loadstring%(([^%)]+)%)') do
        table.insert(call_sources, "loadstring(" .. load_call .. ")")
    end
    for http_call in src:gmatch('HttpGet%(([^%)]+)%)') do
        table.insert(call_sources, "HttpGet(" .. http_call .. ")")
    end

    return {
        detected = "WeAreDevs Luau Obfuscator v1.0.0",
        encryptedStringsCount = #string_matches,
        extractedCalls = call_sources,
        header = header or "wearedevs"
    }
end

local function decode_prometheus(src)
    local is_prometheus = false
    local signatures = {
        "Prometheus", "bit32", "bit.bxor", "getfenv", "setmetatable", "newproxy"
    }
    local score = 0
    for _, sig in ipairs(signatures) do
        if src:find(sig, 1, true) then score = score + 1 end
    end
    if score >= 3 or src:find("Prometheus", 1, true) then
        is_prometheus = true
    end
    if not is_prometheus then return nil end

    local const_arrays = 0
    for _ in src:gmatch('%{%s*%-?%d+[%s,;%-?%d+]+%}') do
        const_arrays = const_arrays + 1
    end

    return {
        detected = "Prometheus V2 / Pipeline",
        confidence = score >= 4 and "high" or "medium",
        constantArraysDetected = const_arrays
    }
end

local function decode_ironbrew(src)
    local is_ib = false
    if src:find("IronBrew", 1, true) or src:find("Ironbrew", 1, true) or src:find("ironbrew", 1, true) then
        is_ib = true
    elseif (src:find("gBit", 1, true) or src:find("gBits", 1, true)) and src:find("bit32", 1, true) then
        is_ib = true
    end
    if not is_ib then return nil end

    return {
        detected = "IronBrew 2 / Luraph Legacy",
        confidence = "high",
        vmLoop = src:find("while true do", 1, true) ~= nil
    }
end

local function decode_moonsec(src)
    local is_moon = false
    if src:find("MoonSec", 1, true) or src:find("Moonsec", 1, true) or src:find("moonsec", 1, true) then
        is_moon = true
    elseif src:find("bxor", 1, true) and src:find("\\", 1, true) and src:find("return(", 1, true) then
        is_moon = true
    end
    if not is_moon then return nil end

    local ver = "MoonSec V2/V3"
    if src:find("V3", 1, true) or src:find("v3", 1, true) then ver = "MoonSec V3" end
    return {
        detected = ver,
        confidence = "high"
    }
end

local function decode_generic(src)
    local char_arrays = {}
    for match in src:gmatch("string%.char%s*%(%s*([%d%s,]+)%s*%)") do
        local decoded = ""
        for num in match:gmatch("%d+") do
            local n = tonumber(num)
            if n and n >= 0 and n <= 255 then
                decoded = decoded .. string.char(n)
            end
        end
        if #decoded > 0 then
            table.insert(char_arrays, decoded)
        end
    end

    local urls = {}
    for url in src:gmatch('https?://[%w-_%.%?%.:/%+=&]+') do
        table.insert(urls, url)
    end

    return {
        stringCharDecoded = char_arrays,
        extractedUrls = urls
    }
end

local res = {
    wearedevs = decode_wearedevs(input),
    prometheus = decode_prometheus(input),
    ironbrew = decode_ironbrew(input),
    moonsec = decode_moonsec(input),
    generic = decode_generic(input)
}

local summary_detected = "Generic/Unknown"
if res.wearedevs then
    summary_detected = res.wearedevs.detected
elseif res.prometheus then
    summary_detected = res.prometheus.detected
elseif res.ironbrew then
    summary_detected = res.ironbrew.detected
elseif res.moonsec then
    summary_detected = res.moonsec.detected
end
res.detectedType = summary_detected

local HttpService = game:GetService("HttpService")
return HttpService:JSONEncode(res)
