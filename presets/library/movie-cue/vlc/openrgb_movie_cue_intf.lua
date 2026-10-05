-- OpenRGB 3D Spatial — Movie Cue poller (VLC Lua INTERFACE)
-- Install: copy to %APPDATA%/vlc/lua/intf/openrgb_movie_cue.lua
-- vlcrc: extraintf=luaintf
--         lua-intf=openrgb_movie_cue
--
-- Sidecar only: put Movie.movie_cue.json next to Movie.mkv / .mp4.
-- OpenRGB Movie Cue effect must be running with Script API enabled.

local DEFAULT_PORT = 27180

local cached = {
  media_key = "",
  armed = false,
  port = DEFAULT_PORT,
  offset_ms = 0,
  next_idx = 1,
  cues = {},
  duration_ms = 0,
  duration_tol = 5000,
  warned_mismatch = false
}

local function http_post(port, body)
  local ok = pcall(function()
    local sock = vlc.net.connect_tcp("127.0.0.1", port)
    if not sock then error("connect failed") end
    local req = "POST /v1/effect HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: " .. #body .. "\r\nConnection: close\r\n\r\n" .. body
    vlc.net.send(sock, req)
    vlc.net.close(sock)
  end)
  return ok
end

local function fire(port, effect_id, edge, active)
  local body = string.format(
    '{"effect_id":"%s","edge":"%s","active":%s}',
    effect_id, edge or "pulse", active and "true" or "false")
  return http_post(port, body)
end

local function read_file(path)
  local f = io.open(path, "rb")
  if not f then return nil end
  local data = f:read("*a")
  f:close()
  return data
end

local function parse_cues(data)
  local list = {}
  if not data then return list end
  for t, tend, id, edge in data:gmatch('"t_ms"%s*:%s*(%d+).-"t_end_ms"%s*:%s*(%d+).-"effect_id"%s*:%s*"(.-)".-"edge"%s*:%s*"(.-)"') do
    table.insert(list, { t_ms = tonumber(t), t_end_ms = tonumber(tend), effect_id = id, edge = edge })
  end
  for t, id, edge in data:gmatch('"t_ms"%s*:%s*(%d+).-"effect_id"%s*:%s*"(.-)".-"edge"%s*:%s*"(.-)"') do
    local exists = false
    for _, c in ipairs(list) do
      if c.effect_id == id and c.t_ms == tonumber(t) then exists = true break end
    end
    if not exists then
      table.insert(list, { t_ms = tonumber(t), effect_id = id, edge = edge })
    end
  end
  table.sort(list, function(a, b) return a.t_ms < b.t_ms end)
  return list
end

local function current_media_path()
  local item = vlc.input.item()
  if not item then return nil end
  local uri = item:uri()
  if not uri then return nil end
  local path = vlc.strings.decode_uri(uri)
  -- file:///C:/...  file://localhost/C:/...  file:///c:/...
  path = path:gsub("^file://localhost/", "")
  path = path:gsub("^file:///", "")
  path = path:gsub("^file://", "")
  if path:match("^/%a:/") then
    path = path:sub(2)
  end
  return path
end

local function sidecar_candidates(media)
  if not media or media == "" then return {} end
  local norm = media:gsub("\\", "/")
  local base = norm:gsub("%.[^%./]+$", "")
  local list = {
    base .. ".movie_cue.json",
    base .. ".movie_cues.json",
  }
  local wbase = base:gsub("/", "\\")
  if wbase ~= base then
    table.insert(list, wbase .. ".movie_cue.json")
    table.insert(list, wbase .. ".movie_cues.json")
  end
  return list
end

local function input_ms()
  local input = vlc.object.input()
  if not input then return nil end
  local t = vlc.var.get(input, "time")
  if t == nil then return nil end
  return math.floor(t / 1000)
end

local function media_duration_ms()
  local input = vlc.object.input()
  if not input then return nil end
  local len = vlc.var.get(input, "length")
  if len == nil then return nil end
  return math.floor(len / 1000)
end

local function try_arm_sidecar(media)
  local tried = {}
  for _, p in ipairs(sidecar_candidates(media)) do
    table.insert(tried, p)
    local data = read_file(p)
    if data and #data > 0 then
      local list = parse_cues(data)
      if #list > 0 then
        cached.armed = true
        cached.port = tonumber(data:match('"port"%s*:%s*(%d+)')) or DEFAULT_PORT
        cached.offset_ms = tonumber(data:match('"offset_ms"%s*:%s*(-?%d+)')) or 0
        cached.next_idx = 1
        cached.cues = list
        cached.duration_ms = tonumber(data:match('"duration_ms"%s*:%s*(%d+)')) or 0
        cached.duration_tol = tonumber(data:match('"duration_tolerance_ms"%s*:%s*(%d+)')) or 5000
        cached.warned_mismatch = false
        vlc.msg.info(string.format("[OpenRGB Movie Cue] armed sidecar %s (%d cues)", p, #list))
        return true
      end
      vlc.msg.warn(string.format("[OpenRGB Movie Cue] sidecar found but no cues parsed: %s", p))
    end
  end
  vlc.msg.info(string.format("[OpenRGB Movie Cue] media=%s tried=%s",
    tostring(media), table.concat(tried, " | ")))
  return false
end

local function ensure_media()
  local media = current_media_path()
  local key = media or ""
  if key == cached.media_key then return end
  cached.media_key = key
  cached.armed = false
  cached.cues = {}
  cached.next_idx = 1
  if media and try_arm_sidecar(media) then
    return
  end
  vlc.msg.info("[OpenRGB Movie Cue] no .movie_cue.json / .movie_cues.json sidecar next to this video")
end

local function check_duration()
  if cached.warned_mismatch or cached.duration_ms <= 0 then return end
  local actual = media_duration_ms()
  if not actual or actual <= 0 then return end
  local delta = math.abs(actual - cached.duration_ms)
  if delta > cached.duration_tol then
    cached.warned_mismatch = true
    vlc.msg.warn(string.format(
      "[OpenRGB Movie Cue] duration mismatch: cue expects %dms, media is %dms (±%dms)",
      cached.duration_ms, actual, cached.duration_tol))
  end
end

local function process()
  ensure_media()
  if not cached.armed or #cached.cues == 0 then return end
  check_duration()
  local now = input_ms()
  if not now then return end
  now = now + (cached.offset_ms or 0)
  while cached.next_idx <= #cached.cues and cached.cues[cached.next_idx].t_ms <= now do
    local c = cached.cues[cached.next_idx]
    if c.edge == "level" then
      fire(cached.port, c.effect_id, "level", true)
      if c.t_end_ms then c._level_on = true end
    else
      fire(cached.port, c.effect_id, c.edge or "pulse", true)
    end
    vlc.msg.info(string.format("[OpenRGB Movie Cue] fire %s @ %dms", c.effect_id, c.t_ms))
    cached.next_idx = cached.next_idx + 1
  end
  for _, c in ipairs(cached.cues) do
    if c._level_on and c.t_end_ms and now >= c.t_end_ms then
      fire(cached.port, c.effect_id, "level", false)
      c._level_on = false
    end
  end
end

while true do
  local ok, err = pcall(process)
  if not ok then
    vlc.msg.warn("[OpenRGB Movie Cue] " .. tostring(err))
  end
  vlc.misc.mwait(vlc.misc.mdate() + 100000)
end
