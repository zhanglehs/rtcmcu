----------------------------------------------------
-- Interface for adjusting rules.
-- Author: zhangcan
-- Email: zhangcan@youku.com
----------------------------------------------------
----------------------------------------------------

-- for xml parser
package.path = package.path .. ';/usr/local/share/lua/5.1/?.lua'
package.cpath = package.cpath .. ';/usr/local/lib/lua/5.1/?.so'
require("LuaXml")
-- rules structure
local lpl_rules_ctx = lpl_co.lpl_rules_ctx

function update_stream_rule(protocol, p2p, stream_id, cdns, base_path)
	lpl_rules_ctx[protocol].stream_rules[stream_id] = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('stream_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == stream_id then
			table.remove(rule_node, idx)
			break
		end
	end

	local node = rule_node:append('stream')
	node.id = stream_id
	node.w1 = cdns.w1
	node.w2 = cdns.w2
	node.w3 = cdns.w3
	node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function del_stream_rule(protocol, stream_id, base_path)
	lpl_rules_ctx[protocol].stream_rules[stream_id] = nil
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('stream_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == stream_id then
			table.remove(rule_node, idx)
			break
		end
	end
	f:save(file_name)
end

function get_stream_rule(protocol, stream_id)
	return lpl_rules_ctx[protocol].stream_rules[stream_id]
end

function update_alias_rule(protocol, p2p, id, cdns, base_path)
	lpl_rules_ctx[protocol].alias_rules[id] = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('alias_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == id then
			table.remove(rule_node, idx)
			break
		end
	end

	local node = rule_node:append('alias')
	node.id = id
	node.w1 = cdns.w1
	node.w2 = cdns.w2
	node.w3 = cdns.w3
	node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function del_alias_rule(protocol, id, base_path)
	lpl_rules_ctx[protocol].alias_rules[id] = nil
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('alias_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == id then
			table.remove(rule_node, idx)
			break
		end
	end
	f:save(file_name)
end

function get_alias_rule(protocol, id)
	return lpl_rules_ctx[protocol].alias_rules[id]
end

function update_region_rule(protocol, p2p, region_id, cdns, base_path)
	lpl_rules_ctx[protocol].region_rules[region_id] = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('region_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == region_id then
			table.remove(rule_node, idx)
			break
		end
	end

	local node = rule_node:append('region')
	node.id = region_id
	node.w1 = cdns.w1
	node.w2 = cdns.w2
	node.w3 = cdns.w3
	node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function del_region_rule(protocol, region_id, base_path)
	lpl_rules_ctx[protocol].region_rules[region_id] = nil
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('region_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == region_id then
			table.remove(rule_node, idx)
			break
		end
	end
	f:save(file_name)
end

function get_region_rule(protocol, region_id)
	return lpl_rules_ctx[protocol].region_rules[region_id]
end

function update_asn_rule(protocol, p2p, asn_id, cdns, base_path)
	lpl_rules_ctx[protocol].asn_rules[asn_id] = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('asn_rule')
	
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == asn_id then
			table.remove(rule_node, idx)
			break
		end
	end

	local node = rule_node:append('asn')
	node.id = asn_id
	node.w1 = cdns.w1
	node.w2 = cdns.w2
	node.w3 = cdns.w3
	node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function del_asn_rule(protocol, asn_id, base_path)
	lpl_rules_ctx[protocol].asn_rules[asn_id] = nil
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('asn_rule')
	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == asn_id then
			table.remove(rule_node, idx)
			break
		end
	end
	f:save(file_name)
end

function get_asn_rule(protocol, asn_id)
	return lpl_rules_ctx[protocol].asn_rules[asn_id]
end

function update_operator_rule(protocol, p2p, operator_id, cdns, base_path)
	lpl_rules_ctx[protocol].operator_rules[operator_id] = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('operator_rule')

	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == operator_id then
			table.remove(rule_node, idx)
			break
		end
	end

	local node = rule_node:append('operator')
	node.id = operator_id
	node.w1 = cdns.w1
	node.w2 = cdns.w2
	node.w3 = cdns.w3
	node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function del_operator_rule(protocol, operator_id, base_path)
	lpl_rules_ctx[protocol].operator_rules[operator_id] = nil
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('operator_rule')

	local idx = 1
	for idx = 1, table.getn(rule_node) do
		if rule_node[idx].id == operator_id then
			table.remove(rule_node, idx)
			break
		end
	end
	f:save(file_name)
end

function get_operator_rule(protocol, operator_id)
	return lpl_rules_ctx[protocol].operator_rules[operator_id]
end

function update_default_rule(protocol, p2p, cdns, base_path)
	lpl_rules_ctx[protocol].default_rule = cdns
	local file_name = lpl_sche.path_join(base_path, 'self_rules.xml')
	local f = xml.load(file_name)
	if nil == f then
		lerror('cannot open the file.')
	end
	local protocol_node = f:find(protocol)
	local rule_node = protocol_node:find('default')
	rule_node.w1 = cdns.w1
	rule_node.w2 = cdns.w2
	rule_node.w3 = cdns.w3
	rule_node.w4 = cdns.w4
	if nil ~= cdns.p2p then
		rule_node.p2p = cdns.p2p
	end
	f:save(file_name)
end

function get_default_rule(protocol)
	return lpl_rules_ctx[protocol].default_cdn
end

local this_uri = ngx.var.uri
local conf_path = "/opt/interlive/nginx/conf"

local id = ngx.req.get_uri_args()["id"]
local w1 = ngx.req.get_uri_args()["w1"] or "0"
local w2 = ngx.req.get_uri_args()["w2"] or "0"
local w3 = ngx.req.get_uri_args()["w3"] or "0"
local w4 = ngx.req.get_uri_args()["w4"] or "0"
local p2p = tonumber(ngx.req.get_uri_args()["p2p"])

local cdns = {}
cdns.w1 = tonumber(w1)
cdns.w2 = tonumber(w2)
cdns.w3 = tonumber(w3)
cdns.w4 = tonumber(w4)
if nil ~= p2p then
	cdns.p2p = tonumber(p2p)
end

if this_uri == "/rule/flv/default/update" then
	update_default_rule('flv', use_p2p, cdns, conf_path)
elseif this_uri == "/rule/flv_audio_only/default/update" then
	update_default_rule('flv_audio_only', use_p2p, cdns, conf_path)
elseif this_uri == "/rule/hls/default/update" then
	update_default_rule('hls', use_p2p, cdns, conf_path)
elseif this_uri == '/rule/flv/alias/update' then
	update_alias_rule('flv', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv_audio_only/alias/update' then
	update_alias_rule('flv_audio_only', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/hls/alias/update' then
	update_alias_rule('hls', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv/stream/update' then
	update_stream_rule('flv', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv_audio_only/stream/update' then
	update_stream_rule('flv_audio_only', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/hls/stream/update' then
	update_stream_rule('hls', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv/region/update' then
	update_region_rule('flv', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv_audio_only/region/update' then
	update_region_rule('flv_audio_only', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/hls/region/update' then
	update_region_rule('hls', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv/asn/update' then
	update_asn_rule('flv', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv_audio_only/asn/update' then
	update_asn_rule('flv_audio_only', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/hls/asn/update' then
	update_asn_rule('hls', use_p2p, id, cdns, conf_path)
elseif this_uri == '/rule/flv/default/get' then
	cdns = get_default_rule('flv')
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv_audio_only/default/get' then
	cdns = get_default_rule('flv_audio_only')
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/hls/default/get' then
	cdns = get_default_rule('hls')
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv/alias/get' then
	cdns = get_stream_rule('flv', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv_audio_only/alias/get' then
	cdns = get_stream_rule('flv_audio_only', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/hls/alias/get' then
	cdns = get_stream_rule('hls', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv/stream/get' then
	cdns = get_stream_rule('flv', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv_audio_only/stream/get' then
	cdns = get_stream_rule('flv_audio_only', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/hls/stream/get' then
	cdns = get_stream_rule('hls', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv/region/get' then
	cdns = get_region_rule('flv', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv_audio_only/region/get' then
	cdns = get_region_rule('flv_audio_only', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/hls/region/get' then
	cdns = get_region_rule('hls', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv/asn/get' then
	cdns = get_asn_rule('flv', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv_audio_only/asn/get' then
	cdns = get_asn_rule('flv_audio_only', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/hls/asn/get' then
	cdns = get_asn_rule('hls', id)
	local ret = cdns and cdns.w1.. ":" .. cdns.w2 .. ":" .. cdns.w3 .. ":" .. cdns.w4 .. " "
	if nil ~= cdns.p2p then
		ret = ret .. ":" .. cdns.p2p
	else 
		ret = ret .. ":" .. "0"
	end
	ngx.say(ret)
elseif this_uri == '/rule/flv/alias/del' then
	del_alias_rule('flv', id, conf_path)
elseif this_uri == '/rule/flv_audio_only/alias/del' then
	del_alias_rule('flv_audio_only', id, conf_path)
elseif this_uri == '/rule/hls/alias/del' then
	del_alias_rule('hls', id, conf_path)
elseif this_uri == '/rule/flv/stream/del' then
	del_stream_rule('flv', id, conf_path)
elseif this_uri == '/rule/flv_audio_only/stream/del' then
	del_stream_rule('flv_audio_only', id, conf_path)
elseif this_uri == '/rule/hls/stream/del' then
	del_stream_rule('hls', id, conf_path)
elseif this_uri == '/rule/flv/region/del' then
	del_region_rule('flv', id, conf_path)
elseif this_uri == '/rule/flv_audio_only/region/del' then
	del_region_rule('flv_audio_only', id, conf_path)
elseif this_uri == '/rule/hls/region/del' then
	del_region_rule('hls', id, conf_path)
elseif this_uri == '/rule/flv/asn/del' then
	del_asn_rule('flv', id, conf_path)
elseif this_uri == '/rule/flv_audio_only/asn/del' then
	del_asn_rule('flv_audio_only', id, conf_path)
elseif this_uri == '/rule/hls/asn/del' then
	del_asn_rule('hls', id, conf_path)
else
	ngx.log(ngx.DEBUG, "unknown uri ", this_uri)
	ngx.status = ngx.HTTP_NOT_FOUND
end

return


