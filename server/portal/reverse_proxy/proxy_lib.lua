
local io = require("io")

-- for xml parser
package.path = package.path .. ';/usr/local/share/lua/5.1/?.lua'
package.cpath = package.cpath .. ';/usr/local/lib/lua/5.1/?.so'
require("lxp.lom")

module("proxy_lib", package.seeall)

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

local NODE_NUM = 100
g_virtual_nodes = {}

local function load_xml_from_file(filepath)
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

local function find_xml_node(xml_obj, node_name)
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


local function load_server_internal(host_nodes)
	local hosts = {}
	local hosts_num = 0
	local k, v
	for k, v in pairs(host_nodes) do
		if 'host' == v['tag'] then
			local host = {}
			host.name = v.attr.name
			host.ip = v.attr.ip
			host.port = v.attr.port
			hosts_num = hosts_num + 1
			hosts[hosts_num] = host
		end
	end

	return hosts_num, hosts
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

-- TODO: use murmurhash instead
local function hash_fn(key) 
	local md5 = ngx.md5_bin(key)  
	return ngx.crc32_long(md5) 
end

local function load_server(base_path, file_name)
	local file_name = path_join(base_path, file_name)
	local xo = load_xml_from_file(file_name)
	local net_node = find_xml_node(xo, 'net')
	local servers_num, servers = load_server_internal(net_node)

	local virtual_nodes = {}
	local virtual_node_num = 0
	local i = 1
	local j = 1
	for i = 1, servers_num do
		for j = 1, NODE_NUM do
			local virtual_node = {}
			virtual_node.name = 'server-' .. i .. 'node-' .. j
			virtual_node.server = servers[i]
			virtual_node_num = virtual_node_num + 1
			virtual_nodes[virtual_node_num] = virtual_node
		end
	end

	table.sort(virtual_nodes, function (a,b)
		return (hash_fn(a.name) < hash_fn(b.name))
	end)

	return virtual_node_num, virtual_nodes
end

local compare
compare = function(a, b)
	if hash_fn(a) == hash_fn(b) then
		return 0
	elseif hash_fn(a) < hash_fn(b) then
		return -1
	else
		return 1
	end
end

-- initialize shards and virtual nodes
function init(base_path)
	local virtual_node_num, virtual_nodes = load_server(base_path, 'servers.xml')
	g_virtual_nodes.num = virtual_node_num
	g_virtual_nodes.nodes = virtual_nodes
end

-- return the integer part of a floor number
local function get_int_part(x)
	local rt = math.ceil(x)
	if rt>0 and rt~=x then
		rt = rt - 1
	end
	return rt
end

-- use binary search to find the most closed virtual node
-- TODO: optimize using red black tree
function binary_search(key, buckets)
	local virtual_nodes = buckets.nodes
	local virtual_nodes_num = buckets.num
	local i = 1
	local j = virtual_nodes_num
	local mid = -1
	while true do 
		mid = get_int_part((i + j) / 2)
		if compare(key, virtual_nodes[mid].name) == 0 then
			return virtual_nodes[mid]
		elseif compare(key, virtual_nodes[mid].name) < 0 then
			if mid -1 < 1 then
				return virtual_nodes[1]
			else
				j = mid - 1
			end
		elseif mid + 1 <= virtual_nodes_num and compare(key, virtual_nodes[mid + 1].name) <= 0 then
			return virtual_nodes[mid + 1]
		elseif mid + 1 <= virtual_nodes_num and compare(key, virtual_nodes[mid + 1].name) > 0 then
			i = mid + 1
		else
			return virtual_nodes[1]
		end
	end
end


http_query = function(host, port, path)
	local resp = nil
	local sock = ngx.socket.tcp()
	local ok, err = sock:connect(host, port)
	if not ok then
		lwarn("failed to connect to ",host,":",port,",error:",err)
	else
		sock:send({"GET ", path," HTTP/1.1\r\n","Host: ",host .. ":" .. port,"\r\n","Accept: */*\r\n","Accept-Encoding:deflate\r\n","User-Agent: lpl\r\n","Connection: Close \r\n\r\n"})
		sock:settimeout(5000)
		local line, err, partial = sock:receive()
		if not line then
			lwarn("failed to read a line: ", err)
		else
			local pos = string.len("HTTP/1.1 ")+1
			local status = string.sub(line, pos, pos+2)
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
				body, err, partial = sock:receive()
				if inflate then
					local inflate_stream = zlib.inflate()
					local inflated,eop,bytes_in,bytes_out = inflate_stream(body)
					body = inflated
				end
				linfo("resp body:",body)
				resp = body
			else
				lwarn("error status:",status)
			end
		end
	end
	sock:close()
	return resp
end
