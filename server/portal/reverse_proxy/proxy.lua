--[[
--  @description: handle the request and redirect to
--  coresponding stream source
--  @author: zhangcan
]]

local this_uri = ngx.var.uri
local default_passwd = "98765"
local token = nil
local stream_id = nil
local args = ngx.req.get_uri_args()
for key, val in pairs(args) do
	if key == "a" and type(val) ~= "table" then
		token = val
	elseif key == "b" and type(val) ~= "table" then
		stream_id = val
	end
end

if nil ~= string.find(this_uri, "/live/hls/v1/(.*).m3u8") then
	stream_id = ngx.var[1]
elseif nil ~= string.find(this_uri, "/live/hls/v1/(.*)/(.*).ts") then
	stream_id = ngx.var[1]
elseif nil ~= string.find(this_uri, "/live/flv/v1/(.*).audio.flv") then
	stream_id = string.gsub(ngx.var[1], ".audio", "")
elseif nil ~= string.find(this_uri, "/live/flv/v1/(.*).flv") then
	stream_id = ngx.var[1]
elseif nil ~= string.find(this_uri, "/live/f/v1/(.*)/audio") then
	stream_id = string.gsub(ngx.var[1], "/audio", "")
elseif nil ~= string.find(this_uri, "/live/f/v1/(.*)") then
	stream_id = ngx.var[1]
end

if nil ~= stream_id then
	local url = ngx.var.uri
	local virtual_node = proxy_lib.binary_search(stream_id, proxy_lib.g_virtual_nodes)
	local ip = virtual_node.server.ip
	local port = virtual_node.server.port
	if nil ~= ip then
		url = string.format('http://%s:%s%s?%s', ip, port, url, ngx.var.args)
	end
	return ngx.redirect(url, ngx.HTTP_MOVED_TEMPORARILY)
else
	ngx.log(ngx.DEBUG, "invalid stream_id ", stream_id)
	ngx.status = ngx.HTTP_BAD_REQUEST
	return
end
