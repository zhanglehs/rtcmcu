--[[
--  @description: handle the request and redirect to
--  coresponding stream source
--  @author: zhangcan
]]

local backend_host = "10.10.69.200"
local backend_port = 8888
local asn = "1000" -- tel

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
end

if nil ~= stream_id then
	local path = "/query?" .. "streamid=" .. stream_id .. "&asn=" .. asn
	local ip = proxy_lib.http_query(backend_host, backend_port , path)
	local port = 443
	local url = ngx.var.uri
	if nil ~= ip then
		url = string.format('http://%s:%s%s?%s', ip, port, url, ngx.var.args)
		return ngx.redirect(url, ngx.HTTP_MOVED_TEMPORARILY)
	else
		ngx.status = ngx.HTTP_BAD_REQUEST
		return
	end
else
	ngx.status = ngx.HTTP_BAD_REQUEST
	return
end
