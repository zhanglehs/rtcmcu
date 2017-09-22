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
local stream_id = string.gsub(ngx.var[1], ".audio", "")
stream_id = string.gsub(stream_id, "/audio", "")

local app_id = ""
local alias = ""
local session_id = ""
local token_origin = ""
local use_p2p = false
local key, val
local req_type = lpl_co.REQ_UNKNOWN

if nil ~= string.find(this_uri, "/live/flv/v1/(.*).audio.flv") then
	req_type = lpl_co.REQ_FLV_AUDIO_V1
elseif nil ~= string.find(this_uri, "/live/flv/v1/(.*).flv") then
	req_type = lpl_co.REQ_FLV_V1
elseif nil ~= string.find(this_uri, "/live/hls/v1/(.*).m3u8") then
	req_type = lpl_co.REQ_M3U8
elseif nil ~= string.find(this_uri, "/live/flvfrag/serverinfo") then
	req_type = lpl_co.REQ_P2P
elseif nil ~= string.find(this_uri, "/live/f/v1/(.*)/audio") then
	req_type = lpl_co.REQ_FLV_AUDIO_V1
elseif nil ~= string.find(this_uri, "/live/f/v1/(.*)") then
	req_type = lpl_co.REQ_FLV_V1
elseif nil ~= string.find(this_uri, "/live/h/v1/(.*)") then
	req_type = lpl_co.REQ_M3U8
elseif nil ~= string.find(this_uri, "/download/sdp/(.*)") then
	req_type = lpl_co.REQ_SDP
else
	ngx.log(ngx.DEBUG, "unknown uri ", this_uri)
	ngx.status = ngx.HTTP_NOT_FOUND
	return
end

local args = ngx.req.get_uri_args()
for key, val in pairs(args) do
	if key == "token" and type(val) ~= "table" then
		token_origin = val
	elseif key == "cip" and type(val) == "string" then
		client_ip = val
	elseif key == "sid" and type(val) == "string" then
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
if string.len(stream_id) >= 8 then
	stream_id = string.sub(stream_id, -8, -1)
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

	if lpl_co.REQ_FLV_V1 == req_type or lpl_co.REQ_FLV_AUDIO_V1 == req_type then
		local base_url = "/live/f/v1/"
		if lpl_co.REQ_FLV_V1 == req_type then
			url = url .. string.format("%s%s?token=%s&ts=%s", base_url, stream_id_orig, token, ts)
		else
			url = url .. string.format("%s%s/audio?token=%s&ts=%s", base_url, stream_id_orig, token, ts)
		end

		local content = {}
		content["v"] = "4.0"
		content["u"] = url
		content["stream_id"] = stream_id_orig
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
		content["token"] = token
		content["ac"] = info.region
		content["dma"] = info.isp
		content["dispatcher_url"] = "http://lps.xiu.youku.com/live/flvfrag/serverinfo?a=" .. token_origin .. "&b=" .. stream_id_orig
		local json_content = cjson.encode(content)
		ngx.header["Content-Type"] = "text/plain"
		ngx.header["Expires"] = "-1"
		ngx.header["Pragma"] = "no-cache"
		ngx.say(json_content)
		return
	elseif lpl_co.REQ_M3U8 == req_type then
		url = url .. string.format('/live/hls/v1/%s.m3u8?token=%s&ts=%s', stream_id_orig, token, ts)
		local content = "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=400000\n" .. url .. "\n"
		ngx.header.content_type = 'application/vnd.apple.mpegurl'
		ngx.header["Content-Length"] = tostring(string.len(content))
		ngx.header["Expires"] = "-1"
		ngx.header["Cache-Control"] = "private, max-age=0"
		ngx.header["Pragma"] = "no-cache"
		ngx.say(content)
		return
	elseif lpl_co.REQ_SDP == req_type then
		url = url .. string.format('/download/sdp/%s?token=%s', stream_id_orig, token)
		local content = {}
		content["ip"] = ip
		content["udp_port"] = 443
		content["tcp_port"] = 80
		content["play_token"] = token
		content["sdp_url"] = url
		local json_content = cjson.encode(content)
		ngx.header.content_type = 'text/plain'
		ngx.header["Content-Length"] = tostring(string.len(json_content))
		ngx.header["Expires"] = "-1"
		ngx.header["Cache-Control"] = "private, max-age=0"
		ngx.header["Pragma"] = "no-cache"
		ngx.say(json_content)
		return
	else
		ngx.log(ngx.WARN, "unknown req type ", req_type)
		ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
		return
	end
end
