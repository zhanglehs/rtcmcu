--@version $Id$
--reload
--cjson = require "cjson"

module("lreporter", package.seeall)


local sock = nil

function report(data)
	if sock == nil then
		sock = ngx.socket.udp()
	end
	local ok, err = sock:setpeername(REPORT_SERVER_IP, REPORT_SERVER_PORT)
	if not ok then
		ngx.log(ngx.WARN,"failed to connect report server ", err)
	else
		ok, err = sock:send(data)
		if not ok then
			ngx.log(ngx.WRAN, "udp send fail ",err)
		end
	end
	sock:close()
end
