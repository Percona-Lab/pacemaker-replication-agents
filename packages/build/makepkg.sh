
version='1.0.0'
iteration='1'

mkdir -p percona-agents/usr/lib/ocf/resource.d/percona/
cp ../../agents/* percona-agents/usr/lib/ocf/resource.d/percona/
sudo chown root.root -R percona-agents/usr/lib/ocf/resource.d/percona/
sudo chmod 755 -R percona-agents/usr/lib/ocf/resource.d/percona/
cd percona-agents
tar cvzf ../percona-agents.tgz usr
cd ..
fpm -s tar -t rpm -v $version --iteration $iteration --description 'The Pacemaker resource agents from Percona' percona-agents.tgz 
fpm -s tar -t deb -v $version --iteration $iteration --description 'The Pacemaker resource agents from Percona' percona-agents.tgz
sudo rm -rf percona-agents
rm -f percona-agents.tgz
