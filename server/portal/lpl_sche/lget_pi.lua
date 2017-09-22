--@version $Id$

local pl_sche_secret_key = 'HowCanIFindTheBestKey.JustTry.'
local pl_sche_token_timeout = 60 * 60 * 24 * 7
local secret_key = '01234567890123456789012345678901'
local wangsu_secret_key = 'fjasoiWEAfsfwo$#fsaowqofqo@@7AEES.'
local default_passwd = '98765'

local OUR_SOURCE = 0
local CMM_CDN_LX = 1
local CMM_CDN_WS = 2

local USE_P2P = 1
local NOT_USE_P2P = 0

-- TODO move follow macro to a global place
local RTMP_APP = 'trm'

local client_ip = ngx.var.remote_addr
local this_uri = ngx.var.uri
local server_addr = ngx.var.server_addr
local server_port = ngx.var.server_port
local stream_id = ''
local app_id = ""
local alias = ""
local session_id = ''
local token_origin = ''
local use_p2p = false
local key, val
local req_type = lpl_co.REQ_UNKNOWN

if this_uri == '/v1/ss' then
	req_type = lpl_co.REQ_FLV_V1
elseif this_uri == '/v1/ms' then
	req_type = lpl_co.REQ_M3U8
elseif this_uri == '/v1/ms.m3u8' then
	req_type = lpl_co.REQ_M3U8_HTML5
elseif this_uri == '/v2/ss' then
	req_type = lpl_co.REQ_FLV_V2
elseif this_uri == '/v3/ss' then
	req_type = lpl_co.REQ_FLV_V3
elseif this_uri == '/v1/rs' then
	req_type = lpl_co.REQ_RTMP_V1
elseif this_uri == '/live/flvfrag/serverinfo' then
	req_type = lpl_co.REQ_P2P
else
	ngx.log(ngx.DEBUG, "unknown uri ", this_uri)
	ngx.status = ngx.HTTP_NOT_FOUND
	return
end

local args = ngx.req.get_uri_args()
for key, val in pairs(args) do
	if key == "a" and type(val) ~= "table" then
		token_origin = val
	elseif key == "b" and type(val) ~= "table" then
		stream_id = val
	elseif key == "cip" and type(val) == 'string' then
		client_ip = val
	elseif key == "sid" and type(val) == 'string' then
		session_id = val
	elseif key == "p2p" and type(val) == "string" then
		use_p2p = tonumber(val)
		if 0 == use_p2p or nil == use_p2p then
			use_p2p = false
		else
			use_p2p = true
		end
	elseif key == "alias" and type(val) == "string" then
		alias = val
	elseif key == "app_id" and type(val) == "string" then
		app_id = val
	end
end

stream_id_orig = stream_id
if req_type == lpl_co.REQ_FLV_V3 or req_type == lpl_co.REQ_P2P then
	if string.len(stream_id) >= 8 then
		stream_id = string.sub(stream_id, -8, -1)
	end
end

stream_id = tonumber(stream_id, 16)
if nil == stream_id then
	ngx.log(ngx.DEBUG, "invalid stream_id ", stream_id)
	ngx.status = ngx.HTTP_BAD_REQUEST
	return
end

local ts = ngx.time()
local ret = lpl_sche.check_token(ts, stream_id, '0', '', pl_sche_secret_key, ngx.md5, pl_sche_token_timeout, token_origin)
if not ret and token_origin ~= default_passwd then
	ngx.log(ngx.DEBUG, "fails to check pl_sche_token.", token_origin)
	ngx.status = ngx.HTTP_FORBIDDEN
	return
end

if nil == lpl_sche.to_ip_number(client_ip) then
	ngx.log(ngx.WARN, "invalid client ip ", client_ip)
	ngx.status = ngx.HTTP_BAD_REQUEST
	return
end

local user_agent = ngx.req.get_headers()['user-agent']
if type(user_agent) ~= 'string' then
	if nil == user_agent then
		user_agent = ''
	elseif type(user_agent) == 'table' then
		user_agent = val[1]
	else
		ngx.log(ngx.WARN, "find invalid user_agent ", user_agent)
		ngx.status = ngx.HTTP_BAD_REQUEST
		return
	end
end

if not lpl_co.lpl_sche_ctx.update_inited then
	ngx.log(ngx.INFO, "init_update")
	lpl_sche.init_update()
end

local ip, port, info, use_cdn, p2p_flag = lpl_sche.do_sche(app_id, alias, stream_id, stream_id_orig, client_ip, req_type, use_p2p)

if 1 == p2p_flag then
	use_p2p = true
else
	use_p2p = false
end

if nil == ip then
	ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
	if info ~= nil then
		local rpt = {server_addr..":"..server_port,"\tRPT\tlpl_sche\t",ngx.localtime(),"\tsche\tfail\t",session_id,"\t",stream_id,"\t",client_ip,"\t",info.isp,"\t",info.region,"\t",info.group,"\n"}
		lreporter.report(rpt)
	end

	return
else
	local url = ""
	if lpl_co.REQ_RTMP_V1 == req_type then
		url = url .. "rtmp://" .. ip
		if 1935 ~= port then
			url = url .. ":" .. port
		end
		url = url .. "/" .. RTMP_APP
	else
		url = url .. "http://" .. ip
		if 80 ~= port then
			url = url .. ":" .. port
		end
	end
	local token = default_passwd
	local ts = ngx.time()
	if use_cdn == OUR_SOURCE then
		token = lpl_sche.gen_token(ts, stream_id, user_agent, ip, secret_key,
								   ngx.md5)
	elseif use_cdn == CMM_CDN_WS then
		ts, token = lpl_sche.gen_ws_token(ts, this_uri, ngx.md5)
	end

	ngx.log(ngx.DEBUG, " gen_token ", token, " to ", ip)
	if info ~= nil then
		local rpt = {server_addr..":"..server_port,"\tRPT\tlpl_sche\t",ngx.localtime(),"\tsche\tsuccess\t",session_id,"\t",stream_id,"\t",client_ip,"\t",info.isp,"\t",info.region,"\t",info.group,"\t",ip,"\n"}
		lreporter.report(rpt)
	end

	if lpl_co.REQ_FLV_V1 == req_type or lpl_co.REQ_FLV_V2 == req_type then
		local base_url = "/live/f/v1/"
		stream_id_orig = "000000000000000000000000" .. stream_id_orig
		url = url .. string.format("%s%s?token=%s&ts=%s", base_url, stream_id_orig, token, ts)
		if lpl_co.REQ_FLV_V1 == req_type then
			return ngx.redirect(url, ngx.HTTP_MOVED_TEMPORARILY)
		else
			local content = {}
			content["v"] = "1.0"
			content["u"] = url
			if use_cdn == OUR_SOURCE then
				content["s"] = OUR_SOURCE
			elseif use_cdn == CMM_CDN_LX then
				content["s"] = CMM_CDN_LX
			else
				content["s"] = CMM_CDN_WS
			end
			if use_p2p then
				content["p2p"] = 1
			else
				content["p2p"] = 0
			end
			content["stream_id"] = stream_id_orig
			content["token"] = token
			content["ac"] = info.region
			content["dma"] = info.isp
			content["dispatcher_url"] = "http://lps.xiu.youku.com/live/flvfrag/serverinfo?a=" .. token_origin .. "&b=" .. stream_id_orig
			local json_content = cjson.encode(content)
			ngx.say(json_content)
			return
		end
	elseif lpl_co.REQ_FLV_V3 == req_type then
		local base_url = "/v2/s"
		if OUR_SOURCE ~= use_cdn then
			base_url = "/v2/o"
		end
		url = url .. string.format("%s?a=%s&b=%s", base_url, token, stream_id_orig)
		local content = {}
		content["v"] = "3.0"
		content["u"] = url
		if OUR_SOURCE == use_cdn then
			content["s"] = OUR_SOURCE
		elseif CMM_CDN_LX == use_cdn then
			content["s"] = CMM_CDN_LX
		else
			content["s"] = CMM_CDN_WS
		end
		if use_p2p then
			content["p2p"] = 1
		else
			content["p2p"] = 0
		end
		content["stream_id"] = stream_id_orig
		content["token"] = token
		content["ac"] = info.region
		content["dma"] = info.isp
		content["dispatcher_url"] = "http://lps.xiu.youku.com/live/flvfrag/serverinfo?a=" .. token_origin .. "&b=" .. stream_id_orig
		local json_content = cjson.encode(content)
		ngx.say(json_content)
		return
	elseif lpl_co.REQ_RTMP_V1 == req_type then
		url = url .. string.format("/%x", stream_id)
		local content = {}
		content["v"] = "1.0"
		content["u"] = url
		if OUR_SOURCE == use_cdn then
			content["s"] = OUR_SOURCE
		elseif CMM_CDN_LX == use_cdn then
			content["s"] = CMM_CDN_LX
		else
			content["s"] = CMM_CDN_WS
		end
		local json_content = cjson.encode(content)
		ngx.say(json_content)
		return
		--return ngx.redirect(url, ngx.HTTP_MOVED_TEMPORARILY)
	elseif lpl_co.REQ_M3U8 == req_type then
		url = url .. string.format('/v1/m?a=%s&b=%08x&sid=%s', token, stream_id,session_id)
		local content = "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=400000\n" .. url .. "\n"
		ngx.header.content_type = 'application/vnd.apple.mpegurl'
		ngx.header["Content-Length"] = tostring(string.len(content))
		ngx.header["Expires"] = "-1"
		ngx.header["Cache-Control"] = "private, max-age=0"
		ngx.header["Pragma"] = "no-cache"
		ngx.say(content)
		return
	elseif lpl_co.REQ_M3U8_HTML5 == req_type then
		url = url .. string.format('/v1/m.m3u8?a=%s&b=%08x&sid=%s', token, stream_id,session_id)
		local content = "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=400000\n" .. url .. "\n"
		ngx.header.content_type = 'application/vnd.apple.mpegurl'
		ngx.header["Content-Length"] = tostring(string.len(content))
		ngx.header["Expires"] = "-1"
		ngx.header["Cache-Control"] = "private, max-age=0"
		ngx.header["Pragma"] = "no-cache"
		ngx.say(content)
		return
	elseif lpl_co.REQ_P2P == req_type then
		local content = {}
		content["server"] = ip .. ":" .. port
		local json_content = cjson.encode(content)
		ngx.say(json_content)
		return
	else
		ngx.log(ngx.WARN, "unknown req type ", req_type)
		ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
		return
	end
end
