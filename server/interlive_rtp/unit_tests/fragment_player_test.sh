mkdir download_data
STREAM_ID='0000000a'
SID='a'
BASE_PATH=/opt/interlive3.0/receiver/download_data
HD='00'
CLEAR='VO'
TARGET_PATH=/opt/interlive3.0/receiver/dump_stream/${STREAM_ID}/${HD}-flv
wrongres=0
for i in {1..100}
do
    echo $i
    wget -U Mozilla -c "http://10.10.69.121:8080/v2/s?a=98765&b=${SID}&c=${CLEAR}&fn=$i" -O $BASE_PATH/fragment_$i.flv
    A=`md5sum $BASE_PATH/fragment_$i.flv`
    B=`echo $A |awk -F ' ' '{print $1}'`
    fn=`printf %08x $i`
    target=$TARGET_PATH/$fn.flv
    echo target is $target
    C=`md5sum $target`
    D=`echo $C |awk -F ' ' '{print $1}'`
    if [ $B = $D ];then
        echo $target check right >> rightresult.txt
    else 
        echo $target check wrong >>wrongresult.txt
        ((wrongres=$wrongres+1))
    fi
done

echo there are $wrongres different files!
