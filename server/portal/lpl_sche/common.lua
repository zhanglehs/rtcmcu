----------------------------------------------------
-- Common Globals and Definitions.
-- Author: zhangcan
-- Email: zhangcan@youku.com
----------------------------------------------------
----------------------------------------------------

module("common", package.seeall)
----------------------------------------------------
-- Request Type Definitions.
----------------------------------------------------
REQ_UNKNOWN = 0
REQ_FLV_V1 = 1
REQ_FLV_AUDIO_V1 = 2
REQ_FLV_V2 = 3
REQ_M3U8 = 4
REQ_M3U8_V2 = 5
REQ_RTMP_V1 = 6
REQ_FLV_V3 = 7
REQ_M3U8_HTML5 = 8
REQ_P2P = 9
REQ_SDP = 10

----------------------------------------------------
-- Global config and rules structure.
----------------------------------------------------
function ctx_create()
	local ctx = {}
	ctx.ip_regions = {}
	-- ctx.ip_region[idx].ip_s = 0
	-- ctx.ip_region[idx].ip_e = 0
	-- ctx.ip_region[idx].isp = 0
	-- ctx.ip_region[idx].region = 0
	-- ctx.ip_region[idx].operator = 0
	ctx.ip_region_num = 0

	ctx.groups = {}
	-- ctx.group[group_name] = group
	-- group.servers[ip] = server
	-- group.streams[stream_id] = stream
	-- stream.servers[ip] = server

	ctx.group_num = 0
	ctx.region_group = {}
	-- ctx.region_group[isp][region] = group_name
	ctx.region_group_num = 0

	-- ctx.server = {}
	-- ctx.server[ip_str].port = 0
	-- ctx.server[ip_str].name = ''
	-- ctx.server[ip_str].weight = 0
	-- ctx.server[ip_str].enabled = 1
	-- ctx.server[ip_str].online_num = 0
	-- ctx.server[ip_str].max_online = 0
	-- ctx.server[ip_str].curr_outbound = 0
	-- ctx.server[ip_str].max_outbound = 0
	ctx.server_num = 0
	ctx.server_update_tick = 0
	ctx.stream_update_tick = 0
	ctx.streams_inited = false
	ctx.update_inited = false

	-- rules to filter
	local rules_ctx = {}
	-- local rule_ctx = {}
	-- rule_ctx.stream_rules = {}
	-- rule_ctx.ip_rules = {}
	-- rule_ctx.asn_rules = {}
	-- rule_ctx.region_rules = {}
	-- rule_ctx.operator_rules = {}
	-- rule_ctx.default_rule = {}
	-- rules_ctx.flv = {}
	-- rules_ctx.hls = {}
	return ctx, rules_ctx
end

function ctx_rtp_create()
	local ctx = {}
	ctx.ip_regions = {}
	-- ctx.ip_region[idx].ip_s = 0
	-- ctx.ip_region[idx].ip_e = 0
	-- ctx.ip_region[idx].isp = 0
	-- ctx.ip_region[idx].region = 0
	-- ctx.ip_region[idx].operator = 0
	ctx.ip_region_num = 0

	ctx.groups = {}
	-- ctx.group[group_name] = group
	-- group.servers[ip] = server
	-- group.streams[stream_id] = stream
	-- stream.servers[ip] = server

	ctx.group_num = 0
	ctx.region_group = {}
	-- ctx.region_group[isp][region] = group_name
	ctx.region_group_num = 0

	-- ctx.server = {}
	-- ctx.server[ip_str].port = 0
	-- ctx.server[ip_str].name = ''
	-- ctx.server[ip_str].weight = 0
	-- ctx.server[ip_str].enabled = 1
	-- ctx.server[ip_str].online_num = 0
	-- ctx.server[ip_str].max_online = 0
	-- ctx.server[ip_str].curr_outbound = 0
	-- ctx.server[ip_str].max_outbound = 0
	ctx.server_num = 0
	ctx.server_update_tick = 0
	ctx.stream_update_tick = 0
	ctx.streams_inited = false
	ctx.update_inited = false

	return ctx
end

lpl_sche_ctx, lpl_rules_ctx = ctx_create()
lpl_rtp_sche_ctx = ctx_rtp_create()

----------------------------------------------------
-- Globals and Definitions.
----------------------------------------------------

CDN_OUR = 1
CDN_LX = 2
CDN_WS = 3
CDN_WS_DAILY = 4

COMMERICAL_CDNS_101 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.xiu.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "dfwss.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "dmwss.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "drwss.xiu.youku.com", rtmp_cdn_port = 80 }
}

COMMERICAL_CDNS_201 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.201.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.201.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.201.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "fwss.201.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.201.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.201.youku.com", rtmp_cdn_port = 80 }
}

COMMERICAL_CDNS_301 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.301.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.301.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.301.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "fwss.301.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.301.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.301.youku.com", rtmp_cdn_port = 80 }
}

COMMERICAL_CDNS_401 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.401.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.401.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.401.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "fwss.401.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.401.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.401.youku.com", rtmp_cdn_port = 80 }
}

COMMERICAL_CDNS_501 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.501.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.501.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.501.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "fwss.501.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.501.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.501.youku.com", rtmp_cdn_port = 80 }
}

COMMERICAL_CDNS_601 = {
	[CDN_OUR] = { name = "Our", flag = 0 },
	[CDN_LX] = { name = "LanXun", flag = 1,
	  flv_cdn = "fccsource.xiu.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mccsource.xiu.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rcc.xiu.youku.com", rtmp_cdn_port = 1935 },
	[CDN_WS] = { name = "WangSu", flag = 2,
	  flv_cdn = "fwss.601.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.601.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.601.youku.com", rtmp_cdn_port = 80 },
	[CDN_WS_DAILY] = { name = "WangSuDaily", flag = 2,
	  flv_cdn = "fwss.601.youku.com", flv_cdn_port = 80, 
	  m3u8_cdn = "mwss.601.youku.com", m3u8_cdn_port = 80, 
	  rtmp_cdn = "rwss.601.youku.com", rtmp_cdn_port = 80 }
}

CDNS_INFO = {
	["101"] = COMMERICAL_CDNS_101,
	["201"] = COMMERICAL_CDNS_201,
	["301"] = COMMERICAL_CDNS_301,
	["401"] = COMMERICAL_CDNS_401,
	["501"] = COMMERICAL_CDNS_501,
	["601"] = COMMERICAL_CDNS_601
}
