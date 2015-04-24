. config
for ip in $IP1 $IP2 $IP3; do ssh ubuntu@$ip "sudo mkdir -p /tmp/mysql.ocf.ra.debug; sudo truncate --size 0 /tmp/mysql.ocf.ra.debug/log"; done
