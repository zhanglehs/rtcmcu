#!/bin/sh

base_url="http://10.10.72.254:9090"
stream_type_arr=('pc_plugin' 'fc' 'rtmp' 'rtp' 'ns')
res_arr=('1280x720' '960x540' '360x480' '328x174')
create_stream_ver_arr=('v2' 'v3' 'v4')
stream_format_arr=('flv' 'rtmp' 'mms' 'mmsh')
app_id_arr=(101 201 301 401 501 601)

stream_type_arr_len=${#stream_type_arr[@]}
res_arr_len=${#res_arr[@]}
create_stream_ver_arr_len=${#create_stream_ver_arr[@]}
stream_format_arr_len=${#stream_format_arr[@]}
app_id_arr_len=${#app_id_arr[@]}

#stream_list<app_id, alias>
declare -a appid_list
declare -a alias_list



function create_stream()
{
    #req_url = "http://10.10.69.195:9090/v3/create_stream?app_id=201&stream_type=rtmp&res=1280x720&stream_format=flv&nt=1&p2p=1&alias=rzl2223"
    app_id="$1"
    stream_type="${stream_type_arr[$(($RANDOM%${stream_type_arr_len}))]}"
    res="${res_arr[$(($RANDOM%${res_arr_len}))]}"
    nt="$(($RANDOM%2))"
    p2p="$(($RANDOM%2))"
    alias="$((`date +%s`%100000))"
    ver="${create_stream_ver_arr[$(($RANDOM%${create_stream_ver_arr_len}))]}"
    stream_format="${stream_format_arr[$(($RANDOM%${stream_format_arr_len}))]}"
    req_url="${base_url}/$ver/create_stream?app_id=$app_id&stream_type=$stream_type&res=$res&nt=$nt&p2p=$p2p&alias=$alias&stream_format=$stream_format"
    respose=`curl -s "$req_url"`
    key=$(($key+$alias))
    appid_list[$key]="$app_id"
    alias_list[$key]="$alias"
    echo "Request:$req_url respose:$respose"

}


function get_upload_url()
{
    app_id="$1"
    alias="$2"
    #req_url = "http://10.10.69.195:9090/v1/get_upload_url?alias=rzl"
    req_url="${base_url}/v1/get_upload_url?app_id=$app_id&alias=$alias"
    respose=`curl -s "$req_url"`
    echo "Request:$req_url respose:$respose"
}


function get_playlist()
{
     app_id="$1"
     alias="$2"
     #req_url = http://10.10.69.195:9090/v1/get_playlist?alias=rzl&app_id=201
     req_url="${base_url}/v1/get_playlist?app_id=$app_id&alias=$alias"
     respose=`curl -s "$req_url"`
     echo "Request:$req_url respose:$respose"
}

function get_stream_list()
{
    app_id="$1"
    #req_url = http://10.10.69.195:9090/v1/get_stream_list?app_id=101
    req_url="${base_url}/v1/get_stream_list?app_id=$app_id"
    respose=`curl -s "$req_url"`
    echo "Request:$req_url respose:$respose"
}

function get_all_stream()
{
    #http://10.10.69.195:9090/v1/get_all_stream_valid
    req_url="${base_url}/v1/get_all_stream_valid"
    respose=`curl -s "$req_url"`
    echo "Request:$req_url respose:$respose"
}


function main_test()
{
    for i in $(seq 1000);do
       app_id="${app_id_arr[$(($RANDOM%${app_id_arr_len}))]}"
       create_stream $app_id
       sleep 0.05s
    done

    for key in ${appid_list[@]};do
        app_id=${appid_list[$key]}
        alias=${alias_list[$key]}
        get_upload_url $app_id $alias
        get_playlist $app_id $alias
        get_stream_list $app_id
        get_all_stream
        sleep 1s
    done
}


main_test 1
