#coding:utf-8 
def gen_download_item(type, url, format, res, rt, av, 
            token, stream_id, p2p_state, out_lst, out_lst_std, 
            is_standard = True, format_ver = 2):
        if out_lst == None:
           out_lst = []
        out_lst.append({"type": str(type),
                       "url": str(url),      
                       "format": str(format), 
                       "res": str(res),    
                       "rt": str(rt), 
                       "av": str(av), 
                       "token": str(token), 
                       "stream_id": str(stream_id),
                       "p2p": str(p2p_state)
                       })

        if is_standard == True:
           is_origin = '0'
           if out_lst_std == None:
               out_lst_std = []

           type_std = str(type)
           format_std = str(format)
           if type == 'flv_v2' or type == 'rtp':
              is_origin = '1'
              format_std = 'flv'
           else:
              is_origin = '0'

           if type == 'rtp':
              format_std = 'sdp'

           elif type == 'ts':
              format_std = 'm3u8'

           else:
               pass
              
           new_url = url
           #new_url = lutil.url_values_del(url, ['token'])
           logging.debug('url: %s, new_url:%s', url, new_url)


           out_lst_std.append({
                       "url": new_url,      
                       "format": format_std, 
                       "res": str(res),    
                       "rt": str(rt), 
                       "av": str(av), 
                       "stream_id": str(stream_id),
                       "p2p": str(p2p_state),
                       "format_ver": str(format_ver),
                       "origin": is_origin
                       })

        return (out_lst, out_lst_std)


