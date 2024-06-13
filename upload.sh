file=.pio/build/wt32-eth01/firmware.bin
md5=($(md5 < $file))
mode=firmware
host=<your device ip here>
user=user
pass=pass

curl --compressed -L -X POST -F "MD5=$md5" -F "name=$mode" -F "data=@$file;filename=$mode" "http://$user:$pass@$host/update"
