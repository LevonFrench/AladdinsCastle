-- SPDX-License-Identifier: GPL-3.0-only
-- MAME 0.289, OutRun only. Register/state observations, no ROM/pixel reads.
-- Prepare .local/mame-probe first; run from the repository worktree.
-- ACVR_PROBE_FRAMES (default 3000), ACVR_PROBE_OUTPUT (optional exact path).
-- All taps return nil: emulated reads/writes are never changed.
local machine = manager.machine
assert(emu.app_version():match("^0%.289"), "This decoder is verified only on MAME 0.289")
assert(machine.system.name == "outrun", "This decoder is only verified for outrun")
local total = tonumber(os.getenv("ACVR_PROBE_FRAMES") or "3000")
assert(total and total > 0 and total % 1 == 0, "Invalid ACVR_PROBE_FRAMES")
local output = os.getenv("ACVR_PROBE_OUTPUT") or
    (".local/mame-probe/outrun-" .. os.date("%Y%m%d-%H%M%S") .. ".jsonl")
local stream = assert(io.open(output, "w"), "Create .local/mame-probe before running")

local function json(value)
    local kind = type(value)
    if kind == "boolean" or kind == "number" then return tostring(value) end
    if kind == "string" then
        return '"' .. value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r') .. '"'
    end
    if kind == "table" then
        local result = {}
        if #value > 0 then
            for i = 1, #value do result[#result + 1] = json(value[i]) end
            return "[" .. table.concat(result, ",") .. "]"
        end
        for k,v in pairs(value) do result[#result + 1] = json(k) .. ":" .. json(v) end
        table.sort(result)
        return "{" .. table.concat(result, ",") .. "}"
    end
    return "null"
end
local function write(record) stream:write(json(record), "\n") end
local spritedev = assert(machine.devices[":sprites"])
local bufferindex
for name,index in pairs(spritedev.items) do
    if name:match("/m_buffer$") then assert(not bufferindex, "Ambiguous sprite buffer");bufferindex=index end
end
assert(bufferindex, "Rendered sprite m_buffer save item is not exposed")
local spritebuffer = emu.item(bufferindex)
assert(spritebuffer.size == 2 and spritebuffer.count == 2048, "Unexpected sprite buffer layout")
local main = assert(machine.devices[":maincpu"].spaces["program"])
local sub = assert(machine.devices[":subcpu"].spaces["program"])
local roadshare = assert(machine.memory.shares[":segaic16road:roadram"])
local roadshadow, active_road = {}, nil
for i=0,2047 do roadshadow[i+1] = roadshare:read_u16(i*2) end
local road_latches, control_writes, control = 0, 0, 0
local taps = {} -- retain handlers so they remain installed
-- Road RAM is mirrored at 080000..08ffff in subcpu. Mirror normalization is mandatory.
taps[#taps+1] = sub:install_write_tap(0x080000,0x08ffff,"acvr_road_ram",
    function(address,data,mask)
        local i=((address-0x080000) & 0x0fff)//2+1
        roadshadow[i] = ((roadshadow[i] or 0) & (~mask & 0xffff)) | (data & mask)
    end)
-- This read handler swaps CPU road RAM and the unexposed rendering buffer.
-- Read taps execute AFTER the underlying handler. The write shadow still holds
-- the pre-swap CPU values, while share reads now expose the retired render bank.
taps[#taps+1] = sub:install_read_tap(0x090000,0x09ffff,"acvr_road_latch",
    function(address,data,mask)
        active_road = roadshadow
        roadshadow = {}
        for i=0,2047 do roadshadow[i+1] = roadshare:read_u16(i*2) end
        road_latches = road_latches+1
    end)
taps[#taps+1] = sub:install_write_tap(0x090000,0x09ffff,"acvr_road_control",
    function(address,data,mask)
        if (mask & 0xff) ~= 0 then control=data & 3;control_writes=control_writes+1 end
    end)
write({kind="metadata",schema=1,mame=emu.app_version(),system=machine.system.name,
    requested_frames=total,sprite_source="sprites/0/m_buffer",sprite_origin_x=189,
    sprite_origin_y=0,road_source="subcpu write shadow at road read-latch",
    road_depth="flat-ground proxy only; no world Z or ROM graphics",
    zoom_convention="effective_scale=512/max(raw_zoom,64)",screen_width=320,screen_height=224})

local function sprites()
    local result={}
    for slot=0,255 do
        local w={}
        for word=0,6 do w[word+1]=spritebuffer:read(slot*8+word) end
        local ending=(w[1] & 0x8000)~=0
        local hidden=(w[1] & 0x5000)~=0
        local y=(w[1] & 0x1ff)-256
        local yd=(w[5] & 0x8000)~=0 and 1 or -1
        local xd=(w[5] & 0x2000)~=0 and 1 or -1
        local x=w[3] & 0x1ff
        if x<0x80 and xd<0 then x=x+512 end
        local height=(w[6] >> 8)+1
        local last=y+yd*(height-1)
        local pitchword=((w[3] >> 1) | ((w[5] & 0x1000) << 3))
        if pitchword>=32768 then pitchword=pitchword-65536 end
        result[#result+1]={slot=slot,ending=ending,hidden=hidden,bank=(w[1] >> 9) & 7,
            offset=w[2],x=x-189,y_start=y,top=math.min(y,last),bottom=math.max(y,last),
            height=height,hzoom=w[5] & 0x7ff,vzoom=w[4] & 0x7ff,
            effective_hscale=512/math.max(w[5] & 0x7ff,64),
            effective_vscale=512/math.max(w[4] & 0x7ff,64),
            priority=(w[4] >> 12) & 3,shadow=(w[4] & 0x4000)~=0,
            flip=(w[5] & 0x4000)==0,x_direction=xd,y_direction=yd,
            palette=w[6] & 0x7f,pitch=math.floor(pitchword/256)}
        if ending then break end
    end
    return result
end
local function roads()
    local rows={}
    local first=-1
    if not active_road then return rows,first end
    for y=0,223 do
        local d0,d1=active_road[y+1],active_road[0x100+y+1]
        local i0,i1=d0 & 0x1ff,d1 & 0x1ff
        local valid0,valid1=(d0 & 0x800)==0,(d1 & 0x800)==0
        local visible=(control==0 and valid0) or (control==3 and valid1) or
            ((control==1 or control==2) and (valid0 or valid1))
        if visible and first<0 then first=y end
        rows[#rows+1]={y=y,data0=d0,data1=d1,solid0=not valid0,solid1=not valid1,
            visible=visible,line0=(d0 >> 1) & 0xff,line1=(d1 >> 1) & 0xff,
            hpos0=active_road[0x200+i0+1] & 0xfff,hpos1=active_road[0x400+i1+1] & 0xfff,
            colour0=active_road[0x600+i0+1],colour1=active_road[0x600+i1+1]}
    end
    return rows,first
end
local frame,closed=0,false
local stop_subscription
local function close(reason)
    if closed then return end
    closed=true
    if stop_subscription then stop_subscription=nil end
    write({kind="end",frames=frame,reason=reason,road_latches=road_latches})
    stream:close()
    print(string.format("ACVR_PROBE_DONE frames=%d road_latches=%d output=%s",frame,road_latches,output))
end
local function capture()
    assert(#taps==3, "Tap handlers lost") -- captured upvalue keeps handlers alive
    frame=frame+1
    local rows,horizon=roads()
    write({kind="frame",frame=frame,road_latches=road_latches,road_control=control,
        road_control_known=control_writes>0,first_road_y=horizon,road=rows,sprites=sprites(),
        -- Diagnostic only: mapped RAM is NOT the rendered list after draw_write.
        cpu_sprite_first_word=main:read_u16(0x100000)})
    if frame%100==0 then stream:flush() end
    if frame>=total then close("frame_limit");machine:exit() end
end
emu.register_frame_done(function()
    if closed then return end
    local ok,err=pcall(capture)
    if not ok then close("error: "..tostring(err));machine:exit();error(err) end
end)
-- Stop notification also closes a partial capture on an external/safety timeout.
stop_subscription=emu.add_machine_stop_notifier(function() close("machine_stop") end)
print("ACVR_PROBE_STARTED "..output)
