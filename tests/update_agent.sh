. config
for ip in $IP1 $IP2 $IP3; do scp ../agents/$1 ubuntu@$ip:$1; ssh ubuntu@$ip "sudo chown root.root $1; sudo chmod a+x $1; sudo mv $1 /usr/lib/ocf/resource.d/percona/$1"; done
