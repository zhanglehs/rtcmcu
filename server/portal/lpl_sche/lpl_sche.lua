--@version $Id$

-- for xml parser
package.path = package.path .. ';/usr/local/share/lua/5.1/?.lua'
package.cpath = package.cpath .. ';/usr/local/lib/lua/5.1/?.so'
require("lxp.lom")
require("LuaXml")

module("lpl_sche", package.seeall)

local lpl_sche_ctx, lpl_rules_ctx = lpl_co.lpl_sche_ctx, lpl_co.lpl_rules_ctx
local lpl_rtp_sche_ctx = lpl_co.lpl_rtp_sche_ctx
local CDN_INFO = lpl_co.CDNS_INFO
local CDN_OUR = lpl_co.CDN_OUR
local CDN_LX = lpl_co.CDN_LX
local CDN_WS = lpl_co.CDN_WS
local CDN_WS_DAILY = lpl_co.CDN_WS_DAILY

local USE_P2P = 1
local NOT_USE_P2P = 0
local NOT_DEFINATION_USE_P2P = nil

local io = require("io")
local zlib = require 'zlib'
local linfo = print
local lwarn = print
local ldebug = print
local lerror = print
if nil ~= ngx and nil ~= ngx.log then
	linfo = function (...) ngx.log(ngx.INFO, ...) end
	lwarn = function (...) ngx.log(ngx.WARN, ...) end
	ldebug = function (...) ngx.log(ngx.DEBUG, ...) end
	lerror = function (...) ngx.log(ngx.ERR, ...) end
end


local LAN_ISP = 88888888
local LAN_REGION = 88888888

local DEFAULT_ISP = 0
local DEFAULT_REGION = 0

local DEFAULT_GROUP = 'commercial_cdn'
local DEFAULT_PORT = 443
local DEFAULT_RTMP_PORT = 1935 
local DEFAULT_FORWARD_IP = '10.31.91.62'
local DEFAULT_FORWARD_PORT = 8101

local MAX_LOAD_DIFF = 10
local MAX_LOAD = 4000

local UPDATE_INTERVAL = 30

local function dump(o)
	local function indent(level)
		local idt = ''
		local cnt = 0
		while level > 0 do
			idt = idt .. ' '
			level = level - 1
		end

		return idt
	end
	local function dump_impl(o, level)
		if type(o) == 'table' then
			local s = '{\n'
			for k,v in pairs(o) do
				if type(k) ~= 'number' then k = '"'..k..'"' end
				s = s .. indent(level) .. '['..k..'] = ' .. dump_impl(v, level + 1) .. ',\n'
			end
			return s .. '}\n'
		else
			return tostring(o)
		end
	end
	return dump_impl(o, 1)
end

function path_join(patha, pathb)
	local ahas = (nil ~= string.find(patha, '.*/$'))
	local bhas = (nil ~= string.find(pathb, '^/.*'))

	if ahas and bhas then
		return patha .. string.sub(pathb, 2)
	elseif ahas or bhas then
		return patha .. pathb
	else
		return patha .. '/' .. pathb
	end
end

function unique_array(array)
	local num = 0
	local new_array = {}
	local k, v, last_v
	for k, v in pairs(array) do
		if 0 == num or v ~= last_v then
			num = num + 1
			new_array[num] = v
			last_v = v
		end
	end

	return num, new_array
end

function to_ip_str(ip)
	local a, b, c, d
	if nil == ip then
		return nil
	end

	a = math.floor(ip / (2 ^ 24))
	b = math.floor((ip / (2 ^ 16)) % 256)
	c = math.floor((ip / 256) % 256)
	d = math.floor(ip % 256)

	if a >= 256 or b >= 256 or c >= 256 or d >= 256 then
		return nil
	end

	return a .. '.' .. b .. '.' .. c .. '.' .. d
end

function to_ip_number(ip_str)
    local ret, _, a, b, c, d
	if nil == ip_str then
		return nil
	end
    ret, _, a, b, c, d = string.find(ip_str, '(%d+)%.(%d+)%.(%d+)%.(%d+)')
    if nil == ret then
	   return nil
    end

	a = tonumber(a)
	b = tonumber(b)
	c = tonumber(c)
	d = tonumber(d)
	if a >= 256 or b >= 256 or c >= 256 or d >= 256 then
		return nil
	end

	return a * 256 * 256 * 256 + b * 256 * 256 + c * 256 + d
end

function load_from_file(filepath, line_func)
	local f, msg
	f, msg = io.open(filepath, "r")
	if nil == f then
		lwarn("can't open file.error:", msg)
		return false
	end

	for line in f:lines() do
		if nil == string.find(line, '^%s*#.*') and nil == string.find(line, '^%s*$') then
			line_func(line)
		end
	end

	f:close()
	f = nil
	return true
end

function load_xml_from_file(filepath)
	local f, msg
	f, msg = io.open(filepath, "r")
	if nil == f then
		lwarn("can't open file.error:", msg)
		return nil
	end

	local jc = f:read("*all")
	f:close()
	f = nil

	local ret = lxp.lom.parse(jc)
	return ret
end

function find_xml_node(xml_obj, node_name)
	local k, v

	if type(xml_obj) ~= 'table' then
		return nil
	end

	for k, v in pairs(xml_obj) do
		if type(v) == 'table' then
			if node_name == v['tag'] then
				return v
			end
		end
	end

	return nil
end

function process_ip_region(ip_seg, isp, region, operator)
	local ip_region = {}
	ip_region.ip_s = 0
	ip_region.ip_e = 0
	ip_region.isp = 0
	ip_region.region = 0
	ip_region.operator = 0
	local ret, _, ips, ipw
	ret, _, ips, ipw = string.find(ip_seg, '^%s*(%d+%.%d+%.%d+%.%d+)%s*/%s*(%d+)%s*$')
	if nil == ret then
		lwarn("can't parse ip", ip_seg)
		return nil
	end

	ipw = tonumber(ipw)
	ip_region.ip_s = to_ip_number(ips)
	if nil == ip_region.ip_s or nil == ipw or ipw > 32 then
		lwarn("invalid ip seg", ips, ipw)
		return nil
	end

	ip_region.ip_e = ip_region.ip_s + (2 ^ (32 - ipw) - 1)
	ip_region.isp = tonumber(isp)
	ip_region.region = tonumber(region)
	ip_region.operator = tonumber(operator)
	if nil == ip_region.isp or nil == ip_region.region or nil == ip_region.operator then
		lwarn("can't parse isp or region", isp, region)
		return nil
	end

	return ip_region
end

local function load_server_and_group(net_nodes)
	local ctx_groups = {}
	local ctx_group_num = 0
	--	local ctx_server = {}
	--	local ctx_server_num = 0

	local k, v
	for k, v in pairs(net_nodes) do
		if type(v) == 'table' and 'ketama' == v['tag']
		and nil ~= v.attr and nil ~= v.attr.name then
			local group = {}
			group.servers = {}
			group.server_num = 0
			group.enabled_server_num = 0
			group.disabled_server_num = 0
			group.stream_num = 0
			group.streams= {}
			group.tip=v.attr.tip
			group.tport=v.attr.tport
			group.group_id=v.attr.group_id

			local node, value
			for node, value in pairs(v) do
				if type(value) == 'table' and 'host' == value['tag']
				and nil ~= value.attr and nil ~= value.attr.ip then
					if nil == to_ip_number(value.attr.ip) then
						lwarn('error ip format:',value.attr.ip)
					else
						local server = {}
						server.name = value.attr.name
						server.weight = value.attr.weight
						server.online_num = 0
						server.max_online = 0
						server.curr_outbound = 0
						server.max_outbound = 0
						server.ip = value.attr.ip
						server.port = tonumber(value['attr']['port'])
						if nil == server.port then
							server.port = DEFAULT_PORT
						end

						local enabled = tonumber(value['attr']['enabled'])
						if nil == enabled  or 1 == enabled then
							server.enabled = true
						else
							server.enabled = false
						end
						if server.enabled then
							group.enabled_server_num = group.enabled_server_num + 1
						else
							group.disabled_server_num = group.disabled_server_num + 1
						end
						group.server_num = group.server_num + 1
						--group.data[group.num] = value.attr.ip
						group.servers[server.ip .. server.port] = server
					end
				end
			end

			if group.server_num > 0 then
				if nil == ctx_groups[v.attr.name] then
					ctx_group_num = ctx_group_num + 1
				end
				ctx_groups[v.attr.name] = group
			end
		end
	end

	return ctx_group_num, ctx_groups
	--	return ctx_group_num, ctx_groups, ctx_server_num, ctx_server
end

local function load_region_group(cdn_nodes)
	local region_group = {}
	local region_group_num = 0

	local k, v
	for k, v in pairs(cdn_nodes) do
		local asn = {}
		local asn_num = 0
		local area = {}
		local area_num = 0
		if type(v) == 'table' and 'rule' == v['tag'] then
			local ketama = ''
			local node, value
			for node, value in pairs(v) do
				if 'asn' == value['tag'] and nil ~= value[1] then
					asn_num = asn_num + 1
					asn[asn_num] = tonumber(value[1])
				elseif 'area' == value['tag'] and nil ~= value[1] then
					area_num = area_num + 1
					area[area_num] = tonumber(value[1])
				elseif 'ketama' == value['tag'] and nil ~= value[1] then
					ketama = value[1]
				end
			end

			local _, isp
			for _, isp in pairs(asn) do
				local region
				for _, region in pairs(area) do
					if nil == region_group[isp] then
						region_group[isp] = {}
					end
					region_group[isp][region] = ketama
					region_group_num = region_group_num + 1
				end
			end
		end
	end

	return region_group_num, region_group
end

function ctx_load_region_group(ctx, filen)
	local ctx_groups = {}
	local ctx_group_num = 0
	--	local ctx_server = {}
	--	local ctx_server_num = 0
	local ctx_region_group = {}
	local ctx_region_group_num = 0

	local xo = load_xml_from_file(filen)
	if nil == xo then
		lwarn("can't load region group", filen)
		return false
	end

	if 'config' ~= xo['tag'] then
		lwarn("can't find config node.")
		return false
	end

	local net_nodes = find_xml_node(xo, 'net')
--	ctx_group_num, ctx_group, ctx_server_num, ctx_server = load_server_and_group(net_nodes)
	ctx_group_num, ctx_groups = load_server_and_group(net_nodes)
	if ctx_group_num <= 0 then
		lwarn("can't load group or server ")
		return false
	end

	local cdn_nodes = find_xml_node(xo, 'cdn')
	ctx_region_group_num, ctx_region_group = load_region_group(cdn_nodes)
	if ctx_region_group_num <= 0 then
		lwarn("can't load region_group")
		return false
	end

	ctx.groups = ctx_groups
	ctx.group_num = ctx_group_num
	ctx.region_group = ctx_region_group
	ctx.region_group_num = ctx_region_group_num

	--	ctx_create_group_server(ctx)

	return true
end

function ip_seg_comp(ta, tb)
	return ta.ip_s < tb.ip_s
end

function ctx_load_ip_region(ctx, path)
	local ip_regions = {}
	local num = 0
	local function process_line(line)
		--sample line:1.0.1.0/24,65533,990000,13
		local ret, _, ip_seg, isp, region, operator
		ret, _, ip_seg, isp, region, operator = string.find(line, '(.+),(%d+),(%d+),(%d+)')
		if nil == ret then
		    lwarn("can't parse ", line)
		else
			local ip_region = process_ip_region(ip_seg, isp, region, operator)
			if nil ~= ip_region then
			   num = num + 1
				ip_regions[num] = ip_region
			end
		end
	end

	local result
	result = load_from_file(path, process_line)
	if result and num > 0 then
		ctx.ip_regions = ip_regions
		ctx.ip_region_num = num

		table.sort(ctx.ip_regions, ip_seg_comp)
		return true
	end

	return false
end

local function load_protocol_rule_node(rule_nodes)
	local pro_rule_node = {}
	pro_rule_node.alias_rules = {}
	pro_rule_node.stream_rules = {}
	pro_rule_node.region_rules = {}
	pro_rule_node.asn_rules = {}
	pro_rule_node.operator_rules = {}
	pro_rule_node.default_rule = {}

	local alias_rules = find_xml_node(rule_nodes, 'alias_rule')
	local k, v
	for k, v in pairs(alias_rules) do
		if 'alias' == v['tag'] then
			local node = {}
			if nil == v.attr.id or nil == v.attr.w1 or nil == v.attr.w2 or nil == v.attr.w3 or nil == v.attr.w4 then
				return nil
			end
			node.w1 = tonumber(v.attr.w1)
			node.w2 = tonumber(v.attr.w2)
			node.w3 = tonumber(v.attr.w3)
			node.w4 = tonumber(v.attr.w4)
			if nil ~= v.attr.p2p then
				node.p2p = tonumber(v.attr.p2p)
			end
			pro_rule_node.alias_rules[v.attr.id] = node
		end
	end

	local stream_rules = find_xml_node(rule_nodes, 'stream_rule')
	local k, v
	for k, v in pairs(stream_rules) do
		if 'stream' == v['tag'] then
			local node = {}
			if nil == v.attr.id or nil == v.attr.w1 or nil == v.attr.w2 or nil == v.attr.w3 or nil == v.attr.w4 then
				return nil
			end
			node.w1 = tonumber(v.attr.w1)
			node.w2 = tonumber(v.attr.w2)
			node.w3 = tonumber(v.attr.w3)
			node.w4 = tonumber(v.attr.w4)
			if nil ~= v.attr.p2p then
				node.p2p = tonumber(v.attr.p2p)
			end
			pro_rule_node.stream_rules[v.attr.id] = node
		end
	end

	local region_rules = find_xml_node(rule_nodes, 'region_rule')
	local k, v
	for k, v in pairs(region_rules) do
		if 'region' == v['tag'] then
			local node = {}
			if nil == v.attr.id or nil == v.attr.w1 or nil == v.attr.w2 or nil == v.attr.w3 or nil == v.attr.w4 then
				return nil
			end
			node.w1 = tonumber(v.attr.w1)
			node.w2 = tonumber(v.attr.w2)
			node.w3 = tonumber(v.attr.w3)
			node.w4 = tonumber(v.attr.w4)
			if nil ~= v.attr.p2p then
				node.p2p = tonumber(v.attr.p2p)
			end
			pro_rule_node.region_rules[v.attr.id] = node
		end
	end

	local asn_rules = find_xml_node(rule_nodes, 'asn_rule')
	local k, v
	for k, v in pairs(asn_rules) do
		if 'asn' == v['tag'] then
			local node = {}
			if nil == v.attr.id or nil == v.attr.w1 or nil == v.attr.w2 or nil == v.attr.w3 or nil == v.attr.w4 then
				return nil
			end
			node.w1 = tonumber(v.attr.w1)
			node.w2 = tonumber(v.attr.w2)
			node.w3 = tonumber(v.attr.w3)
			node.w4 = tonumber(v.attr.w4)
			if nil ~= v.attr.p2p then
				node.p2p = tonumber(v.attr.p2p)
			end
			pro_rule_node.asn_rules[v.attr.id] = node
		end
	end

	local operator_rules = find_xml_node(rule_nodes, 'operator_rule')
	local k, v
	for k, v in pairs(operator_rules) do
		if 'operator' == v['tag'] then
			local node = {}
			if nil == v.attr.id or nil == v.attr.w1 or nil == v.attr.w2 or nil == v.attr.w3 or nil == v.attr.w4 then
				return nil
			end
			node.w1 = tonumber(v.attr.w1)
			node.w2 = tonumber(v.attr.w2)
			node.w3 = tonumber(v.attr.w3)
			node.w4 = tonumber(v.attr.w4)
			if nil ~= v.attr.p2p then
				node.p2p = tonumber(v.attr.p2p)
			end
			pro_rule_node.operator_rules[v.attr.id] = node
		end
	end

	local default_rule = find_xml_node(rule_nodes, 'default')
	if nil == default_rule then
		return nil
	end
	local node = {}
	if nil == default_rule.attr.w1 or nil == default_rule.attr.w2 or nil == default_rule.attr.w3 or nil == default_rule.attr.w4 then
		return nil
	end
	node.w1 = tonumber(default_rule.attr.w1)
	node.w2 = tonumber(default_rule.attr.w2)
	node.w3 = tonumber(default_rule.attr.w3)
	node.w4 = tonumber(default_rule.attr.w4)
	if nil ~= default_rule.attr.p2p then
		node.p2p = tonumber(default_rule.attr.p2p)
	end
	pro_rule_node.default_rule = node
	return pro_rule_node
end

function ctx_load_rules(ctx, file_name)
	local xo = load_xml_from_file(file_name)
	if nil == xo or "config" ~= xo["tag"] then
		lwarn("can't load self-definition rule", filen)
		return false
	end

	for key, value in pairs(xo) do
		if nil ~= value["tag"] then
			local protocol_rule = load_protocol_rule_node(value)
			if nil == protocol_rule then
				lwarn("can't load self-definition rule", filen)
				return false
			end
			ctx[value["tag"]] = protocol_rule
		end
	end
	return true
end

function ctx_load_all(sche_ctx, rules_ctx, base_path)
	local filen
	filen = path_join(base_path, 'ip_region.ini')
	if not ctx_load_ip_region(sche_ctx, filen) then
		lwarn("can't update ip_region")
		return false
	end
	
	filen = path_join(base_path, 'region_group.xml')
	if not ctx_load_region_group(sche_ctx, filen) then
		lwarn("can't load region group")
		return false
	end

	filen = path_join(base_path, 'self_rules.xml')
	if not ctx_load_rules(rules_ctx, filen) then
		lwarn("can't load self-definition rule")
		return false
	end
	return true
end

function ctx_rtp_load_all(sche_ctx, base_path)
	local filen
	filen = path_join(base_path, 'ip_region.ini')
	if not ctx_load_ip_region(sche_ctx, filen) then
		lwarn("can't update ip_region")
		return false
	end
	
	filen = path_join(base_path, 'rtp_region_group.xml')
	if not ctx_load_region_group(sche_ctx, filen) then
		lwarn("can't load region group")
		return false
	end

	return true
end

function ctx_init_streams(ctx, stream_tab)
	for stream_id, info in pairs(stream_tab) do
		local server_stream_dict = info.server_stream_dict
		if nil ~= server_stream_dict then
			for addr,_ in pairs(server_stream_dict) do
				local ret, _, ip, port = string.find(addr, '(.+):(.+)')
				if nil ~=ip then
					local group, server = ctx_find_server(ctx, ip, port)
					if server ~= nil then
						local stream = group.streams[stream_id]
						if nil == stream then
							stream = {}
							stream.server_num = 0
							stream.servers = {}
							group.streams[stream_id] = stream
						end
						stream.servers[ip .. port] =  server
						stream.server_num = stream.server_num + 1
					end
				end
			end
		end
	end
end

function ctx_find_server(ctx, ip, port)
	for _,group in pairs(ctx.groups) do
		local server = group.servers[ip .. port]
		if server ~= nil then
			return group,server
		end
	end
	return nil,nil
end


local ctx_update_server
ctx_update_server = function(ctx, server_tab)
	local key, val
	--	local update_enabled = false
	local current_servers = {}
	for key, val in pairs(server_tab) do
		local ip = val["ip"]
		if nil ~= ip then
			current_servers[ip] = true
			local group,server = ctx_find_server(ctx,ip,port)
			if nil ~= server then
				if nil ~= val["can_serve"] then
					local new_val = val["can_serve"]
					if server.enabled ~= new_val then
						linfo("change server(", ip, ") enabled to ", new_val)
						if server.enabled then
							group.enabled_server_num = group.enabled_server_num - 1
							group.disabled_server_num = group.disabled_server_num + 1
						else
							group.disabled_server_num = group.disabled_server_num - 1
							group.enabled_server_num = group.enabled_server_num + 1
						end
					end
					server.enabled = new_val
					--					update_enabled = true
				end

				if nil ~= val["online_num"] then
					server.online_num = val["online_num"]
				end

				if nil ~= val["outbound"] then
					server.curr_outbound = val["outbound"]
				end
				--				linfo(ip,",",server.enabled,",",server.online_num)
			else
				ldebug("cannot find server for ip ", ip)
			end
		end
	end

	for _,group in pairs(ctx.groups) do
		for ip,server in pairs(group.servers) do
			if current_servers[ip] == nil then
				if server.enabled then
					server.enabled = false
					group.enabled_server_num = group.enabled_server_num - 1
					group.disabled_server_num = group.disabled_server_num + 1
				end
			end
		end
	end

	--	if update_enabled then
	--		ctx_create_group_server(ctx)
	--	end
end


-- return isp_code, region_code when success or else nil
function ctx_find_region(ctx, ip)
	local low = 1
	local high = ctx.ip_region_num + 1
	local rpt;

	local server_addr = ngx.var.server_addr
	local server_port = ngx.var.server_port
	local client_ip = ngx.var.remote_addr

	local ip_regions = ctx.ip_regions
	while low < high do
		if high - low <= 20 then
			local last_item = nil
			for idx = low, high - 1 do
				local t = ip_regions[idx]
				--print(to_ip_str(t.ip_s), to_ip_str(t.ip_e))
				if t.ip_s <= ip and ip <= t.ip_e then
					if nil ~= last_item then
						if t.ip_e - t.ip_s < last_item.ip_e - last_item.ip_s then
							last_item = t
						end
					else
						last_item = t
					end
				end
			end
			if nil ~= last_item then
				--print(last_item.isp, last_item.region)
				return last_item.isp, last_item.region, last_item.operator
			end
		end

		local mid = math.floor((low + high) / 2)
		local t = ip_regions[mid]
		if t.ip_s > ip then
			high = mid
		elseif t.ip_e < ip then
			low = mid + 1
		else
			return t.isp, t.region, t.operator
		end
	end


	rpt = {server_addr..":"..server_port,"\tRPT\tlpl_sche\t",ngx.localtime(),"\tstat\tfail\tclient_ip:",client_ip,"\tnot exist\n"}
	lreporter.report(rpt)
	return 0, 0, 0 
end

--function ctx_get_one_addr_random(ctx, stream_id, group_name)
--	local group_server = ctx.group_server[group_name]
--	local rank = math.random(group_server.total_rank)
--	local k, v
--	for k, v in pairs(group_server.data) do
--		if rank <= v.rank then
--			return v.ip, v.port
--		end
--	end
--
--	lerror("can't find server in ", group_name,
--	" for stream ", stream_id, ", total_rank:",
--	group_server.total_rank)
--
--	return nil, nil
--end

function get_min_load_server(servers)
	local min_load_server = nil
	for _,server in pairs(servers) do
		if server.enabled and (min_load_server == nil or server.online_num < min_load_server.online_num) then
			min_load_server = server
		end
	end
	return min_load_server
end


local http_query = function(host,port,path)
	local resp = nil
	local sock = ngx.socket.tcp()
	local ok, err = sock:connect(host, port)
	if not ok then
		lwarn("failed to connect to ",host,":",port,",error:",err)
	else
		sock:send({"GET ",path," HTTP/1.1\r\n","Host: ",host,"\r\n","Accept: */*\r\n","Accept-Encoding:deflate\r\n","User-Agent: lpl-schedule\r\n","Connection: Close \r\n\r\n"})
		sock:settimeout(5000)
		local line,err, partial = sock:receive()
		if not line then
			lwarn("failed to read a line: ", err)
		else
			local pos = string.len("HTTP/1.1 ")+1
			local status = string.sub(line,pos,pos+2)
			if status == "200" then
				local inflate = false
				while line ~= nil do
					line, err, partial = sock:receive()
					if string.len(line) == 0 then
						break
					end
					if string.find(line,'deflate') then
						inflate = true
					end
				end
				local body
				body, err, partial = sock:receive("*a")
				if inflate then
					local inflate_stream = zlib.inflate()
					local inflated,eop,bytes_in,bytes_out = inflate_stream(body)
					body = inflated
				end
--				linfo("resp body:",body)
				resp = cjson.decode(body)
			else
				lwarn("error status:",status)
			end
		end
	end
	sock:close()
	return resp
end


function ctx_get_addr(ctx, stream_id, group_name)
	local group = ctx.groups[group_name]
	if nil == group then
		linfo("can't find group for name ", group_name)
		return nil,nil
	end

	local tip = group.tip
	local tport = group.tport
	local group_id = group.group_id
	local path = "/query?streamid="..stream_id.."&groupid="..group_id
	local rsp = http_query(tip, tport, path)

	if rsp == nil then
		return nil, nil
	end

	return rsp.ip, rsp.port
--[[
	local stream = group.streams[stream_id]
	local stream_min_load_server = nil
	if nil == stream then
		stream = {}
		--stream.online_num = 0
		stream.server_num = 0
		stream.servers = {}
		group.streams[stream_id] = stream
		group.stream_num = group.stream_num + 1
	else
		stream_min_load_server = get_min_load_server(stream.servers)
	end

	local min_load_server = get_min_load_server(group.servers)
	if stream_min_load_server ~= nil and stream_min_load_server.online_num + 1 <= MAX_LOAD then
		min_load_server = stream_min_load_server
	elseif min_load_server ~= nil then
		stream.server_num = stream.server_num + 1
	end

	if min_load_server == nil then
		return nil, nil
	end

	min_load_server.online_num = min_load_server.online_num + 1
	stream.servers[min_load_server.ip .. min_load_server.port] = min_load_server
	return min_load_server.ip, min_load_server.port
--]]
end

function ctx_get_sdp_addr(ctx, stream_id, group_name)
	local group = ctx.groups[group_name]
	if nil == group then
		linfo("can't find group for name ", group_name)
		return nil,nil
	end

	local stream = group.streams[stream_id]
	local stream_min_load_server = nil
	if nil == stream then
		stream = {}
		--stream.online_num = 0
		stream.server_num = 0
		stream.servers = {}
		group.streams[stream_id] = stream
		group.stream_num = group.stream_num + 1
	else
		stream_min_load_server = get_min_load_server(stream.servers)
	end

	local min_load_server = get_min_load_server(group.servers)
	if stream_min_load_server ~= nil and stream_min_load_server.online_num + 1 <= MAX_LOAD then
		min_load_server = stream_min_load_server
	elseif min_load_server ~= nil then
		stream.server_num = stream.server_num + 1
	end

	if min_load_server == nil then
		return nil, nil
	end

	min_load_server.online_num = min_load_server.online_num + 1
	stream.servers[min_load_server.ip .. min_load_server.port] = min_load_server
	return min_load_server.ip, min_load_server.port
end

-- function ctx_get_one_addr(ctx, stream_id, ip_list, ip_list_num)
-- 	math.randomseed(stream_id * 88661)
-- 	while ip_list_num > 0 do
-- 		local idx, ip, server
-- 		idx = math.random(ip_list_num)
-- 		ip = ip_list[idx]
-- 		server = ctx.server[ip]
-- 		if nil ~= server then
-- 			if server.enabled and server.online_num < server.max_online then
-- 				server.online_num = server.online_num + 1
-- 				return ip, server.port
-- 			end
-- 		end

-- 		ip_list_num = ip_list_num - 1
-- 		table.remove(ip_list, idx)
-- 	end

-- 	return nil, nil
-- end

function is_localhost_ip(client_ip)
	if 0x7F000000 <= client_ip and client_ip <= 0x7FFFFFFF then
		return true
	end

	return false
end

function is_LAN_ip(client_ip)
	if 0x0A000000 <= client_ip and client_ip <= 0x0AFFFFFF then
		return true
	elseif 0xAC100000 <= client_ip and client_ip <= 0xAC10FFFF then
		return true
	elseif 0xC0A80000 <= client_ip and client_ip <= 0xC0A8FFFF then
		return true
	else
		return false
	end
end

function ctx_get_region_group_impl(ctx, isp, region)
	local isp_t = ctx.region_group[isp]
	if nil == isp_t then
		--lwarn("can't find region group for isp ", isp)
		return nil
	end

	local region_g = isp_t[region]
	if nil ~= region_g then
		return region_g
	end

	local city_id = math.floor(region / 100)
	region_g = isp_t[city_id]
	if nil ~= region_g then
		return region_g
	end

	local province_id = math.floor(region / (100 * 100))
	region_g = isp_t[province_id]
	if nil ~= region_g then
		return region_g
	end

	region_g = isp_t[DEFAULT_REGION]
	if nil ~= region_g then
		return region_g
	end

	return nil
end

function ctx_get_region_group(ctx, isp, region)
	local ret
	
	ret = ctx_get_region_group_impl(ctx, isp, region)
	if nil ~= ret then
		return ret
	end

	ret = ctx_get_region_group_impl(ctx, DEFAULT_ISP, region)
	if nil ~= ret then
		return ret
	end

	linfo("can't get region group for ", isp, ", ", region)
	return nil
end

function ctx_get_region_server(ctx, isp, region)
	local server_addr = ngx.var.server_addr
	local server_port = ngx.var.server_port
	local client_ip = ngx.var.remote_addr
	local rpt;
	local group = ctx_get_region_group(ctx, isp, region)
	if nil == group then
		rpt = {server_addr..":"..server_port,"\tRPT\tlpl_sche\t",ngx.localtime(),"\tstat\tfail\tclient_ip:",client_ip,"\tisp:",isp,"\tregion:",region,"\t not find group\n"}
		lreporter.report(rpt)
		linfo("can't find group for isp ", isp, ", region ", region, " use default ", DEFAULT_GROUP)
		group = DEFAULT_GROUP
	end

	return group
end

--randomly select one CDN based on the weight
function ctx_get_one_cdn_random(item)
	math.randomseed(tostring(os.time()):reverse():sub(1, 6))
	local weight_sum = 0
	local begin_pos = 0
	local end_pos = nil

	local k, v
	weight_sum = item.w1 + item.w2 + item.w3 + item.w4

	local rand_pos = math.random(weight_sum)
	end_pos = begin_pos + item.w1 
	if begin_pos < rand_pos and rand_pos <= end_pos then
		return 1
	end
	begin_pos = end_pos
	end_pos = begin_pos + item.w2
	if begin_pos < rand_pos and rand_pos <= end_pos then
		return 2
	end
	begin_pos = end_pos
	end_pos = begin_pos + item.w3
	if begin_pos < rand_pos and rand_pos <= end_pos then
		return 3
	end
	begin_pos = end_pos
	end_pos = begin_pos + item.w4
	if begin_pos < rand_pos and rand_pos <= end_pos then
		return 4
	end
	begin_pos = end_pos
	return nil
end

function get_commerical_cdn(cdn_info, app_id)
	local commerical_cdn = cdn_info["101"]
	if app_id ~= "" then
		commerical_cdn = cdn_info[app_id]
	end
	return commerical_cdn
end

----------------------------------------------------
-- 1. check region rule
-- 2. check asn rule
-- 3. check operator rule
-- 4. check stream rule
-- 5. check is_use_p2p
-- 6. return by default
----------------------------------------------------
function ctx_select_cdn(sche_ctx, selected_rule, app_id, alias, stream_id, stream_id_orig, client_ip, is_use_p2p)
	local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
	local item = nil

	if nil == selected_rule then
		return CDN_OUR, asn, region, NOT_USE_P2P
	end
	
	if "" ~= alias and "" ~= app_id then
		item = selected_rule.alias_rules[app_id .. ":" .. alias]
		if nil ~= item then
			if USE_P2P == item.p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			elseif NOT_USE_P2P == item.p2p then
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			elseif is_use_p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			else
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			end
		end
	end

	item = selected_rule.stream_rules[stream_id_orig]
	if nil ~= item then
		if USE_P2P == item.p2p then
			local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
			return CDN_OUR, asn, region, USE_P2P
		elseif NOT_USE_P2P == item.p2p then
			local cdn = ctx_get_one_cdn_random(item)
			return cdn, asn, region, NOT_USE_P2P
		elseif is_use_p2p then
			local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
			return CDN_OUR, asn, region, USE_P2P
		else
			local cdn = ctx_get_one_cdn_random(item)
			return cdn, asn, region, NOT_USE_P2P
		end
	end

	if nil ~= region then
		local province_id = math.floor(region / (100 * 100))
		item = selected_rule.region_rules[tostring(region)]
		if nil ~= item then
			if USE_P2P == item.p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			elseif NOT_USE_P2P == item.p2p then
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			elseif is_use_p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			else
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			end
		end
	end

	if nil ~= asn then
		item = selected_rule.asn_rules[tostring(asn)]
		if nil ~= item then
			if USE_P2P == item.p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			elseif NOT_USE_P2P == item.p2p then
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			elseif is_use_p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			else
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			end
		end
	end

	if nil ~= operator then
		item = selected_rule.operator_rules[tostring(operator)]
		if nil ~= item then
			if USE_P2P == item.p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			elseif NOT_USE_P2P == item.p2p then
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			elseif is_use_p2p then
				local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
				return CDN_OUR, asn, region, USE_P2P
			else
				local cdn = ctx_get_one_cdn_random(item)
				return cdn, asn, region, NOT_USE_P2P
			end
		end
	end

	if is_use_p2p then
		local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
		return CDN_OUR, asn, region, USE_P2P
	end

	local cdn = ctx_get_one_cdn_random(selected_rule.default_rule)
	return cdn, asn, region, NOT_USE_P2P
end

----------------------------------------------------
-- for rtp schedule system
----------------------------------------------------
function ctx_do_rtp_sche(sche_ctx, app_id, alias, stream_id, stream_id_orig, client_ip_str)
	local commerical_cdn = get_commerical_cdn(CDN_INFO, app_id)
	local client_ip = nil
	client_ip = to_ip_number(client_ip_str)
	if nil == client_ip then
		lwarn("can't parse client ip", client_ip_str)
		return nil
	end

	local asn, region, operator = ctx_find_region(sche_ctx, client_ip)
	local info = {}
	info.public_ip = client_ip_str
	info.isp = asn or DEFAULT_ISP
	info.region = region or DEFAULT_REGION
	info.group = DEFAULT_GROUP

	local group = ctx_get_region_server(sche_ctx, info.isp, info.region)
	local ip, port
	ip, port = ctx_get_sdp_addr(sche_ctx, stream_id_orig, group)
	if nil == ip or DEFAULT_GROUP == group then
		-- return 4134 for asn, 110000 for region, if default asn or region
		if DEFAULT_ISP == info.isp or DEFAULT_REGION == info.region then
			info.isp = 4134
			info.region = 110000
		end
	end
	return ip, port, info, commerical_cdn[CDN_OUR].flag, NOT_USE_P2P
end

----------------------------------------------------
-- except rtp, will call this method for schedule
-- 1. select protocol
-- 2. select p2p or not
-- 3. return cdn ip, port
----------------------------------------------------
function ctx_do_sche(sche_ctx, rules_ctx, app_id, alias, stream_id, stream_id_orig, client_ip_str, req_type, is_use_p2p)
	local commerical_cdn = get_commerical_cdn(CDN_INFO, app_id)
	local client_ip = nil
	client_ip = to_ip_number(client_ip_str)
	if nil == client_ip then
		lwarn("can't parse client ip", client_ip_str)
		return nil
	end

	local info = {}
	info.public_ip = client_ip_str

	-- select rule first based on protocol
	local selected_rule = nil
	if lpl_co.REQ_FLV_V1 == req_type or lpl_co.REQ_FLV_V2 == req_type or lpl_co.REQ_FLV_V3 == req_type then
		selected_rule = rules_ctx["flv"]
	elseif lpl_co.REQ_FLV_AUDIO_V1 == req_type then
		selected_rule = rules_ctx["flv_audio_only"]
	elseif lpl_co.REQ_M3U8 == req_type or lpl_co.REQ_M3U8_HTML5 == req_type then
		selected_rule = rules_ctx["hls"]
	elseif lpl_co.REQ_P2P == req_type then
		selected_rule = rules_ctx["p2p"]
	end
	if nil == selected_rule then
		lwarn("can't find rules")
		return nil
	end

	-- decide p2p and select cdn
	local cdn, asn, region, use_p2p
	cdn, asn, region, use_p2p = ctx_select_cdn(sche_ctx, selected_rule, app_id, alias, stream_id, stream_id_orig, client_ip, is_use_p2p)
	
	info.isp = asn or DEFAULT_ISP
	info.region = region or DEFAULT_REGION
	info.group = DEFAULT_GROUP

	-- return cdn ip, port
	if lpl_co.REQ_M3U8 == req_type or lpl_co.REQ_M3U8_HTML5 == req_type then
		if CDN_LX == cdn or CDN_WS == cdn or CDN_WS_DAILY == cdn then
			return commerical_cdn[cdn].m3u8_cdn, commerical_cdn[cdn].m3u8_cdn_port, info, commerical_cdn[cdn].flag, NOT_USE_P2P
		else
			local group = ctx_get_region_server(sche_ctx, info.isp, info.region)
			local ip, port
			ip, port = ctx_get_addr(sche_ctx, stream_id_orig, group)
			if nil == ip or DEFAULT_GROUP == group then
				lerror("canot get addr, ip is null, redirect to default ip")
				return DEFAULT_FORWARD_IP, DEFAULT_FORWARD_PORT, info, commerical_cdn[CDN_OUR].flag, use_p2p
			end
			info.group = group
			return ip, port, info, commerical_cdn[CDN_OUR].flag, use_p2p
		end
	end

	if CDN_LX == cdn or CDN_WS == cdn or CDN_WS_DAILY == cdn then
		return commerical_cdn[cdn].flv_cdn, commerical_cdn[cdn].flv_cdn_port, info, commerical_cdn[cdn].flag, use_p2p
	else
		local group = ctx_get_region_server(sche_ctx, info.isp, info.region)
		local ip, port
		ip, port = ctx_get_addr(sche_ctx, stream_id_orig, group)
		if nil == ip or DEFAULT_GROUP == group then
			if USE_P2P == use_p2p then
				-- return 4134 for asn, 110000 for region, if default asn or region
				if DEFAULT_ISP == info.isp or DEFAULT_REGION == info.region then
					info.isp = 4134
					info.region = 110000
				end
				return ip, port, info, commerical_cdn[CDN_OUR].flag, use_p2p
			else
				lerror("canot get addr, ip is null, redirect to default ip")
				return DEFAULT_FORWARD_IP, DEFAULT_FORWARD_PORT, info, commerical_cdn[CDN_OUR].flag, use_p2p
			end
		end
		if USE_P2P == use_p2p then
			-- return 4134 for asn, 110000 for region, if default asn or region
			if DEFAULT_ISP == info.isp or DEFAULT_REGION == info.region then
				info.isp = 4134
				info.region = 110000
			end
		end
		info.group = group
		return ip, port, info, commerical_cdn[CDN_OUR].flag, use_p2p
	end
end

function load_all(base_path)
	math.randomseed(os.time())
	if not ctx_load_all(lpl_sche_ctx, lpl_rules_ctx, base_path) then
		return false
	end
	if not ctx_rtp_load_all(lpl_rtp_sche_ctx, base_path) then
		return false
	end
	return true
end

function init_streams(stream_tab)
	ctx_init_streams(lpl_sche_ctx, stream_tab)
	lpl_sche_ctx.streams_inited = true
end

function do_sche(app_id, alias, stream_id, stream_id_orig, client_ip_str, req_type, is_use_p2p)
	if lpl_co.REQ_SDP == req_type then
		return ctx_do_rtp_sche(lpl_rtp_sche_ctx, app_id, alias, stream_id, stream_id_orig, client_ip_str)
	else
		return ctx_do_sche(lpl_sche_ctx, lpl_rules_ctx, app_id, alias, stream_id, stream_id_orig, client_ip_str, req_type, is_use_p2p)
	end
end

function get_region_server(client_ip_str)
	local client_ip = nil
	client_ip = to_ip_number(client_ip_str)
	if nil == client_ip then
		lwarn("can't parse client ip", client_ip_str)
		return nil
	end
	local isp, region = ctx_find_region(ctx, client_ip)
	return ctx_get_region_server(lpl_sche_ctx, isp, region)
end


local ctx_update_stream_stat
ctx_update_stream_stat = function(ctx, stream_tab)
	local current_streams = {}
	for stream_id,info in pairs(stream_tab) do
		current_streams[stream_id] = true
		local current_servers = {}
		local server_stream_dict = info.server_stream_dict
		if nil ~= server_stream_dict then
			for addr,_ in pairs(server_stream_dict) do
				local ret, _, ip, port = string.find(addr, '(.+):(.+)')
				if nil ~=ip then
					current_servers[ip] = true
					local group,server = ctx_find_server(ctx,ip,port)
					if server ~= nil then
						local stream = group.streams[stream_id]
						if nil == stream then
							stream = {}
							stream.server_num = 0
							stream.servers = {}
							group.streams[stream_id] = stream
							group.stream_num = group.stream_num + 1
						end
						if stream.servers[ip]==nil then
							stream.servers[ip] = server
							stream.server_num = stream.server_num + 1
						end
					end
				end
			end
			for group_name,group in pairs(ctx.groups) do
				--				linfo("group_name:",group_name)
				local stream = group.streams[stream_id]
				if stream ~= nil then
					for ip,_ in pairs(stream.servers) do
						if current_servers[ip] == nil then
							stream.servers[ip] = nil
							stream.server_num = stream.server_num - 1
						end
					end
				end
			end
		end
	end
	for _,group in pairs(ctx.groups) do
		for stream_id,stream in pairs(group.streams) do
			if current_streams[stream_id] == nil then
				group.streams[stream_id] = nil
				group.stream_num = group.stream_num - 1
			end
		end
	end
end

local update_info
update_info = function(premature)
	linfo('update_stream')
	linfo('get_forward_stat')
	--local server_table = http_query(PORTAL_SERVER,PORTAL_PORT,"/get_forward_stat")
	if server_table ~= nil then
		ctx_update_server(lpl_sche_ctx, server_table)
	end
	linfo('get_stream_stat')
	--local stream_table = http_query(PORTAL_SERVER,PORTAL_PORT,"/get_stream_stat")
	if stream_table ~= nil then
		ctx_update_stream_stat(lpl_sche_ctx, stream_table)
	end

	ngx.timer.at(UPDATE_INTERVAL,update_info)
end

function init_update()
	lpl_sche_ctx.update_inited = true
	if nil ~= ngx then
		update_info(false)
	end
end

function update_stream(stream_tab)
	ctx_update_stream_stat(lpl_sche_ctx, stream_tab)
end

function check_server_update(curr_tick)
	if curr_tick - lpl_sche_ctx.server_update_tick >= 3.0 then
		lpl_sche_ctx.server_update_tick = curr_tick
		return true
	else
		return false
	end
end

function check_stream_update(curr_tick)
	if curr_tick - lpl_sche_ctx.stream_update_tick >= 3.0 then
		lpl_sche_ctx.stream_update_tick = curr_tick
		return true
	else
		return false
	end
end

function gen_token(ts, stream_id, para1_str, para2_str, secret_key, hash_func)
	local str
	local ts_str = string.format('%08x', math.floor(ts % 0x100000000))
	local stream_id_str = string.format('%u', stream_id)
	if string.len(para1_str) > 128 then
		para1_str = string.sub(para1_str, 1, 128)
	end

	if string.len(para2_str) > 128 then
		para2_str = string.sub(para2_str, 1, 128)
	end

	if string.len(secret_key) > 32 then
		secret_key = string.sub(secret_key, 1, 32)
	end

	str = ts_str .. stream_id_str .. secret_key .. para1_str .. para2_str
	--ldebug(str)

	return ts_str .. string.sub(hash_func(str), 9)
end

function gen_ws_token(ts, path, hash_func)
	local str
	local ts_str = string.format('%08x', math.floor(ts % 0x100000000))

	str = "zzzzzzz" .. path .. ts_str
	--ldebug(str)

	return ts_str, hash_func(str)
end

function check_token(curr_ts, stream_id, para1_str, para2_str, secret_key, hash_func, max_timeout, token)
	if string.len(token) ~= 32 then
		return false
	end
	local token_ts = tonumber(string.sub(token, 1, 8), 16)
	if nil == token_ts then
		return false
	end

	if math.abs(curr_ts - token_ts) > max_timeout then
		return false
	end

	local real_token = gen_token(token_ts, stream_id, para1_str, para2_str,
								 secret_key, hash_func)

	return real_token == token
end

