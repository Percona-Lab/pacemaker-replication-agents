============================================= 
Percona replication manager (PRM) setup guide
=============================================

Author: Yves Trudeau, Percona

June 2012

.. contents::

--------
Overview
--------

The Percona replication manager (PRM) is a framwork using the Linux HA resource agent Pacemaker that manages replication and provides automatic failover. This covers the installation of the framework on a set of servers.  The PRM framework is made of 4 components: Corosync, Pacemaker, the mysql resource agent and MySQL itself.  Let's review each of these in more details.

Corosync
========

Corosync handles the communication between the nodes.  It implements a cluster protocol called Totem and communicates over UDP (default port 5405).  By default it uses multicast but version 1.4.2 also supports unicast (udpu).  Pacemaker uses Corosync as a messaging service.  Corosync is not the only communication layer that can be used with Pacemaker heartbeat is another one although its usage is going down.

Pacemaker can also use the heartbeat communication stack.  The setup using heartbeat is covered in the advanced topics.


Pacemaker
=========

Pacemaker is the heart of the solution, it the part managing where the logic is.  Pacemaker maintains a *cluster information base* **cib** that is a share xml databases between all the actives nodes.  The updates to the cib are send synchronously too all the nodes through Corosync.  Pacemaker has an amazingly rich set of configuration settings and features that allows very complex designs.  Without going into too much details, here are some of the features offered:

   - location rules: locating resources on nodes based on some criteria
   - colocating rules: colocating resources based on some criteria
   - clone set: a bunch of similar resource
   - master-slave clone set: a clone set with different level of members
   - a resource group: a group of resources forced to be together
   - ordering rules: in which order should some operation be performed
   - Attributes: kind of cluster wide variables, can be permanent or transient
   - Monitoring: resource can be monitored
   - Notification: resource can be notified of a cluster wide change

and many more.  The Pacemaker logic works with scores, the highest score wins.  

Note on pcs for EL6
-------------------

On EL6, the crm utility has been replaced by the package maintener by the pcs utility.  While both utilities work, this document is all based on crm so I suggest you go to the Suse repo, https://build.opensuse.org/package/show/network:ha-clustering:Stable/crmsh  and install crmsh from there.

mysql resource agent
====================

In order to manage mysql and mysql replication, Pacemaker uses a resource agent which is a bash script.  The mysql resource agent bash script supports a set of calls like start, stop, monitor, promote, etc.  That allows Pacemaker to perform the required actions.

MySQL
=====
 
The final service, the database.


-----------------------
Installing the packages
-----------------------

Redhat/Centos 6
===============

::

   [root@host-01 ~]# yum install pacemaker corosync


On Centos 6.2, this will install Pacemaker 1.1.6 and corosync 1.4.1.

Debian/Ubuntu
=============

::

   [root@host-01 ~]# apt-get install pacemaker corosync

On Debian Wheezy, this will install Pacemaker 1.1.6 and corosync 1.4.2

Redhat/Centos 5
===============

On older releases of RHEL/Centos, you have to install some external repos first:

::

   [root@host-01 ~]# wget http://download.fedoraproject.org/pub/epel/5/x86_64/epel-release-5-4.noarch.rpm
   [root@host-01 ~]# rpm -Uvh epel-release-5-4.noarch.rpm
   [root@host-01 ~]# wget -O /etc/yum.repos.d/pacemaker.repo http://clusterlabs.org/rpm/epel-5/clusterlabs.repo
   [root@host-01 ~]# yum install pacemaker corosync


On RHEL 5.8, this will install Pacemaker 1.0.12 and corosync 1.2.7.

--------------------
Configuring corosync
--------------------

Creating the cluster Authkey
============================

On **one** of the host, run the following command::

   [root@host-01 ~]# cd /etc/corosync
   [root@host-01 corosync]# corosync-keygen 


The key generator needs entropy, to speed up the key generation, I suggest you run commands in another session like ``tar cvj / | md5sum > /dev/null`` and similar.  The resulting file is ``/etc/corosync/authkey`` and its access bytes are 0400 and owner root, group root.  Copy the authkey file to the other hosts of the cluster, same location, owner and rights.

Creating the corosync.conf file
===============================

The next step is to configure the communiction layer, corosync by creating the corosync configuration file ``/etc/corosync/corosync.conf``.  Let's consider the hosts in question have eth1 on the 172.30.222.x network.  A basic corosync configuration will look like::

   compatibility: whitetank
   
   totem {
         version: 2
         secauth: on
         threads: 0
         interface {
                  ringnumber: 0
                  bindnetaddr: 172.30.222.0
                  mcastaddr: 226.94.1.1
                  mcastport: 5405
                  ttl: 1
         }
   }

   logging {
         fileline: off
         to_stderr: no
         to_logfile: yes
         to_syslog: yes
         logfile: /var/log/cluster/corosync.log
         debug: off
         timestamp: on
         logger_subsys {
                  subsys: AMF
                  debug: off
         }
   }

   amf {
         mode: disabled
   }


copy the file to both servers.


Starting Corosync
==================

Start corosync with ``service corosync start``.  In order to verify corosync is working correctly, run the following command::

   [root@host-01 corosync]# corosync-cmapctl | grep members | grep ip
   runtime.totem.pg.mrp.srp.members.-723640660.ip=r(0) ip(172.30.222.212) 
   runtime.totem.pg.mrp.srp.members.-1042407764.ip=r(0) ip(172.30.222.193)

This shows the 2 nodes that are member of the cluster.  If you have more than 2 nodes, you should have more similar entries.  Wit older version of corosync, the tool name is ``corosync-objctl``. If you don't have an output similar to the above, make sure iptables is not blocking udp port 5405 and inspect the content of ``/var/log/cluster/corosync.log`` for more information.

The above corosync configuration file is minimalist, it can be expanded in many ways.  For more information, ``man corosync.conf`` is your friend.

**NOTE:**  Older versions of corosync (RHEL/Centos 5) may not the members when running the *corosync-objctl* command.  You can see communication taking place with the following command (change the eth if not eth1)::

   tcpdump -i eth1 -n port 5405

And you should see output similar to the following::

   09:57:46.969162 IP 172.30.222.212.hpoms-dps-lstn > 172.30.222.193.netsupport: UDP, length 107
   09:57:46.989108 IP 172.30.222.193.hpoms-dps-lstn > 226.94.1.1.netsupport: UDP, length 119
   09:57:47.159079 IP 172.30.222.193.hpoms-dps-lstn > 172.30.222.212.netsupport: UDP, length 107

---------------------
Configuring Pacemaker
---------------------

The OS level configuration for Pacemaker is very simple, create the file ``/etc/corosync/service.d/pacemaker`` with the following content::

   service {
         name: pacemaker
         ver: 1
   }

Starting Pacemaker
==================

You can then start pacemaker with ``service pacemaker start``.  Once started, you should be able to verify the cluster status with the crm command::

   [root@host-02 corosync]# crm status
   ============
   Last updated: Thu May 24 17:06:57 2012
   Last change: Thu May 24 17:05:32 2012 via crmd on host-01
   Stack: openais
   Current DC: host-01 - partition with quorum
   Version: 1.1.6-3.el6-a02c0f19a00c1eb2527ad38f146ebc0834814558
   2 Nodes configured, 2 expected votes
   0 Resources configured.
   ============

   Online: [ host-01 host-02 ]

Here, ``host-01`` and ``host-02`` correspond to the ``uname -n`` values.

-----------------
Configuring MySQL
-----------------

Installation of MySQL
=====================

Install packages like you would normally do depending on the distribution you are using.  The minimal requirements for my.cnf are a unique ``server_id`` for replication, ``log-bin`` to activate the binary log. If you plan to use the slave resync feature after a master crash, you'll need to enable the ``log-slave-updates`` variables. Also, make sure pid-file and socket correspond to what will be defined below for the configuration of the mysql primitive in Pacemaker.  In our example, on Centos 6 servers::

   [root@host-01 ~]# cat /etc/my.cnf 
   [client]
   socket=/var/run/mysqld/mysqld.sock
   [mysqld]
   datadir=/var/lib/mysql
   socket=/var/run/mysqld/mysqld.sock
   user=mysql
   # Disabling symbolic-links is recommended to prevent assorted security risks
   symbolic-links=0
   log-bin
   server-id=1
   pid-file=/var/lib/mysql/mysqld.pid
   log-slave-updates

Start Mysql manually with ``service mysql start`` or the equivalent.

Required Grants
===============

The following grants are needed::

   grant replication client, replication slave on *.* to repl_user@'172.30.222.%' identified by 'ola5P1ZMU';
   grant replication client, replication slave, SUPER, PROCESS, RELOAD on *.* to repl_user@'localhost' identified by 'ola5P1ZMU';
   grant select ON mysql.user to test_user@'localhost' identified by '2JcXCxKF';

Setup replication
=================

You setup the replication like you normally do, make sure replication works fine between all hosts.  With 2 hosts, a good way of checking is to setup master-master replication.  Keep in mind though that PRM will only use master-slave.  Once done, stop MySQL and make sure it doesn't start automatically after boot.  In the future, Pacemaker will be managing MySQL

-----------------------
Pacemaker configuration
-----------------------

Downloading the latest MySQL RA
===============================

The PRM solution requires a specific Pacemaker MySQL resource agent.  The new resource agent is available in version 3.9.3 of the resource-agents package.  In the Centos version used for this documentation, the version of this package is::

   [root@host-01 corosync]# rpm -qa | grep resour
   resource-agents-3.9.2-7.el6.i686

which will not do.  Since it is very recent, we can just download the latest agent from github like here::

   [root@host-01 corosync]# cd /usr/lib/ocf/resource.d/
   [root@host-01 resource.d]# mkdir percona
   [root@host-01 resource.d]# cd percona/
   [root@host-01 percona]# wget -O mysql -q https://github.com/percona/percona-pacemaker-agents/raw/1.0.0-stable/agents/mysql_prm
   [root@host-01 percona]# chmod u+x mysql_prm

The procedure must be repeated on all hosts.  We have created a "percona" directory to make sure there would be no conflict with the default MySQL resource agent if the resource-agents package is updated. You can also find packages for Debian/Ubuntu and RedHat/Centos in the github repo::

    https://github.com/percona/percona-pacemaker-agents/tree/master/packages/build


Configuring Pacemaker
=====================

Cluster attributes
------------------

For the sake of simplicity we start by a 2 nodes cluster.  The problem with a 2 nodes cluster is the loss of quorum as soon as one of the hosts is down.  In order to have a functional 2 nodes we must set the *no-quorum-policy* to ignore like this::

   crm_attribute --attr-name no-quorum-policy --attr-value ignore

This can be revisited for larger clusters.  Also, since for this example we are not configuring any stonith devices, we have to disable stonith with::

   crm_attribute --attr-name stonith-enabled --attr-value false
   
One has to realize that without stonith device may be stuck if a node is unable to complete an operation.  A simple case is a node having a VIP which has a severe storage problem and is no longer able to access the ``ip`` binary required to remove the VIP.  The kernel is still responding to the VIP but the node is effectively unable to do anything useful.  For such a case stonith devices are needed.  Stonith devices are highly recommended in production.

IP configuration for replication
--------------------------------

The PRM solution needs to know which IP it should use to connect to a master when configuring replication, basically, for the *master_host* parameter of the ``change master to`` command.  There's 2 ways of configuring the IPs.  

The default way is to make sure the host names resolves correctly on all the members of the cluster.  Collect the hostnames with ``uname -n`` and verify those names resolve to the IPs you want to from all hosts using replication.  If possible, avoid DNS and use /etc/hosts since DNS adds a big single point of failure.

The other way uses a node attribute.  For example, if the MySQL resource primitive name (next section) is ``p_mysql`` then you can add ``p_mysql_mysql_master_IP`` (``_mysql_master_IP`` concatenated to the resource name) to each node with the IP you want to use. Here's an example::

   node host-01 \
         attributes p_mysql_mysql_master_IP="172.30.222.193"
   node host-02 \
         attributes p_mysql_mysql_master_IP="172.30.222.212"
   
Which means the IP 172.30.222.193 will be use for the ``change master to`` command when host-01 is the master and same for 172.30.222.212, which will be used when host-02 is the master.  These IPs correspond to the private network (eth1) of those hosts.  The best way to modify the Pacemaker configuration is with the command ``crm configure edit`` which loads the configuration in vi.  Once done editing, save the file ":wq" and the new configuration will be loaded by Pacemaker.

**NOTE:** Older versions of corosync (RHEL/Centos 5) may trigger an error like the following::

   /var/run/crm/cib-invalid.vlD2Dq:14: element instance_attributes: Relax-NG validity error : Type ID doesn't allow value 'host-01-instance_attributes'
   /var/run/crm/cib-invalid.vlD2Dq:14: element instance_attributes: Relax-NG validity error : Element instance_attributes failed to validate content
   ...

In this case, ``vi`` many not work for attribute editing so you can use a command like the following to set the IP (or other attributes)::

   crm_attribute -l forever -G --node host-01 --name p_mysql_mysql_master_IP -v "172.30.222.193"

The MySQL resource primitive
----------------------------

We are now ready to start giving work to Pacemaker the first thing we will do is configure the mysql primitive which defines how Pacemaker will call the mysql resource agent.  The resource has many parameter, let's first review them, the defaults presented are the ones for Linux.

=======================  ========================================================================================================
Parameter                Description
=======================  ========================================================================================================
binary                   Location of the MySQL server binary. Typically, this will point to the mysqld or the mysqld_safe file.  
                         The recommended value is the the path of the the mysqld binary, be aware it may not be the defautl.
                         *default: /usr/bin/safe_mysqld*
                         
binary_prefix            A prefix to put on the mysqld command line.  A common use would be an "LD_PRELOAD" or a call to numactl.
                         *default: empty*

client_binary            Location of the MySQL client binary.  *default: mysql*

config                   Location of the mysql configuation file. *default: /etc/my.cnf*

datadir                  Directory containing the MySQL database *default: /var/lib/mysql*

user                     Unix user under which will run the MySQL daemon *default: mysql*

group                    Unix group under which will run the MySQL daemon *default: mysql*

log                      The logfile to be used for mysqld. *default: /var/log/mysqld.log*

pid                      The location of the pid file for mysqld process. *default: /var/run/mysql/mysqld.pid*

socket                   The MySQL Unix socket file. *default: /var/lib/mysql/mysql.sock*

test_table               The table used to test mysql with a ``select count(*)``. *default: mysql.user*

test_user                The MySQL user performing the test on the test table.  Must have ``grant select`` on the test table.
                         *default: root*

test_passwd              Password of the test user. *default: no set*

enable_creation          Runs ``mysql_install_db`` if the datadir is not configured. *default: 0 (boolean 0 or 1)*  

additional_parameters    Additional MySQL parameters passed (example ``--skip-grant-tables``). *default: no set*

replication_user         The MySQL user to use in the ``change master to master_user`` command.  The user must have 
                         REPLICATION SLAVE and REPLICATION CLIENT from the other hosts and SUPER, REPLICATION SLAVE,
                         REPLICATION CLIENT, and PROCESS from localhost.  *default: no set*

replication_passwd       The password of the replication_user. *default: no set*

replication_port         TCP Port to use for MySQL replication. *default: 3306*

max_slave_lag            The maximum number of seconds a replication slave is allowed to lag behind its master. 
                         Do not set this to zero. What the cluster manager does in case a slave exceeds this maximum lag 
                         is determined by the evict_outdated_slaves parameter.  If evict_outdated_slaves is true, slave is 
                         stopped and if false, only a transcient attribute (see reader_attribute) is set to 0.

evict_outdated_slaves    This parameter instructs the resource agent how to react if the slave is lagging behind by more
                         than max_slave_lag.  When set to true, outdated slaves are stopped.  *default: false*

reader_attribute         This parameter sets the name of the transient attribute that can be used to adjust the behavior
                         of the cluster given the state of the slave.  Each slaves updates this attributor at each
                         monitor call and sets it to 1 is sane and 0 if not sane.  Sane is defined as lagging by less than
                         max_slave_lag and slave threads are running.  *default: readable*

reader_failcount         The number of times a monitor operation can find the slave to be unsuitable for reader VIP 
                         before failing.  Useful if there are short intermittent issues like clock adjustments in VMs.
                         *default: 1*
                         
geo_remote_IP            Geo DR IP to access the remote cluster, see the PRM-Geographic-DR-guide for more information.

booth_master_ticket      Booth ticket name of the Geo DR master role, see the PRM-Geographic-DR-guide for more information

post_promote_script      A script that is called at the end of the promote operation.  It can be used for some special
                         use case like preventing failback.  See "Preventing failback" in the How to section.
                         
prm_binlog_parser_path   Path of the tool used by PRM to parse the binlog and relaylog. It is derived from ybinlog developed by
                         Yelp. It can be found at:
                         https://github.com/percona/percona-pacemaker-agents/tree/master/tools/ybinlogp
                         
async_stop               Causes the agent not to wait for MySQL to complete its shutdown procedure before failing over.  Useful
                         to speed up failover when there're a lot of Innodb dirty pages to be flushed to disk.  For now, the
                         is 0, disabled, but it may eventually default to 1, enabled.

=======================  ========================================================================================================                      

So here's a typical primitive declaration::

   primitive p_mysql ocf:percona:mysql_prm \
         params config="/etc/my.cnf" pid="/var/lib/mysql/mysqld.pid" socket="/var/run/mysqld/mysqld.sock" replication_user="repl_user" \
                replication_passwd="ola5P1ZMU" max_slave_lag="60" evict_outdated_slaves="false" binary="/usr/libexec/mysqld" \
                test_user="test_user" test_passwd="2JcXCxKF" \
         op monitor interval="5s" role="Master" OCF_CHECK_LEVEL="1" \
         op monitor interval="2s" role="Slave" OCF_CHECK_LEVEL="1" \
         op start interval="0" timeout="60s" \
         op stop interval="0" timeout="60s" 

An easy way to load the above fragment is to use the ``crm configure edit`` command.  You will notice that we also define two monitor operations, one for the role Master and one for role slave with different intervals.  It is important to have different intervals, for Pacemaker internal reasons. Also, I defined the timeout for start and stop to 60s, make sure you have configured innodb_log_file_size in a way that mysql can stop in less than 60s with the maximum allowed number of dirty pages and that it can start in less than 60s while having to perform Innodb recovery.

An alterate way to configure this using pcs is::

    pcs resource create p_mysql ocf:percona:mysql_prm \
                config="/etc/my.cnf" pid="/var/lib/mysql/mysqld.pid" socket="/var/run/mysqld/mysqld.sock" replication_user="repl_user" \
                replication_passwd="ola5P1ZMU" max_slave_lag="60" evict_outdated_slaves="false" binary="/usr/libexec/mysqld" \
                test_user="test_user" test_passwd="2JcXCxKF"
    pcs resource op add p_mysql start interval="0" timeout="60s"
    pcs resource op add p_mysql stop interval="0" timeout="60s" 

Since the snippet refers to role Master and Slave, you need to also include the master slave clone set (below).

The Master slave clone set
--------------------------

Next we need to tell Pacemaker to start a set of similar resource (the p_mysql type primitive) and consider the primitives in the set as having 2 states, Master and slave.  This type of declaration uses the ``ms`` type (for master-slave).  The configuration snippet for the ``ms`` is::

   ms ms_MySQL p_mysql \
        meta master-max="1" master-node-max="1" clone-max="2" clone-node-max="1" notify="true" globally-unique="false" target-role="Master" is-managed="true"

Here, the importants elements are clone-max and notify.  ``clone-max`` is the number of databases node involded in the ``ms`` set.  Since we are consider a two nodes cluster, it is set to 2.  If we ever add a node, we will need to increase ``clone-max`` to 3.  The solution works with notification, so it is mandatory to enable notifications with ``notify`` set to true.

An alterate way to configure this using pcs is::

   pcs resource update p_mysql   --master master-max="1" master-node-max="1" clone-max="2" clone-node-max="1" notify="true" globally-unique="false" target-role="Master" is-managed="true"
   pcs resource master ms_MySQL p_mysql
   pcs constraint colocation add  master  ms_MySQL with writer_vip
   pcs resource op add  p_mysql monitor interval="5s" role="Master" OCF_CHECK_LEVEL="1"
   pcs resource op add  p_mysql monitor interval="2s" role="Slave" OCF_CHECK_LEVEL="1"



The VIP primitives
------------------

Let's assume we want to have a writer virtual IP (VIP), 172.30.222.100 and two reader virtual IPs, 172.30.222.101 and 172.30.222.102.  The first thing we need to do is to add the primitives to the cluster configuration.  Those primitives will look like::

   primitive reader_vip_1 ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.101" nic="eth1" \
         op monitor interval="10s"
   primitive reader_vip_2 ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.102" nic="eth1" \
         op monitor interval="10s"
   primitive writer_vip ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.100" nic="eth1" \
         op monitor interval="10s"

After adding these primitives to the cluster configuration with ``crm configure edit``, the VIPs will be distributed in a round-robin fashion, not exactly ideal.  This is why we need to add rules to control on which hosts they'll be on.


An alterate way to configure this using pcs is::

   pcs resource create reader_vip_1 ocf:heartbeat:IPaddr2 \
         ip="172.30.222.101" nic="eth1" \
         op monitor interval="10s"
   pcs resource create reader_vip_2 ocf:heartbeat:IPaddr2 \
         ip="172.30.222.102" nic="eth1" \
         op monitor interval="10s"
   pcs resource create writer_vip ocf:heartbeat:IPaddr2 \
         ip="172.30.222.100" nic="eth1" \
         op monitor interval="10s"

Reader VIP location rules
-------------------------

One of the new element introduced with this solution is the addition of a transient attribute to control if a host is suitable to host a reader VIP.  The replication master are always suitable but the slave suitability is determine by the monitor operation which set the transient attribute to 1 is ok and to 0 is not.  In the MySQL primitive above, we have not set the *reader_attribute* parameter so we are using the default value "readable" for the transient attribute.  The use of the transient attribute is through a location rule which will but a score on -infinity for the VIPs to be located on unsuitable hosts.  The location rules for the reader VIPs are the following::

   location loc-no-reader-vip-1 reader_vip_1 \
         rule $id="rule-no-reader-vip-1" -inf: readable lt 1
   location loc-No-reader-vip-2 reader_vip_2 \
         rule $id="rule-no-reader-vip-2" -inf: readable lt 1

Again, use ``crm configure edit`` to add the these rules.

An alterate way to configure this using pcs is::

   pcs constraint location  reader_vip_1  avoids  readable eq 0
   pcs constraint location  reader_vip_2  avoids  readable eq 0

Writer VIP rules
----------------

The writer VIP is simpler, it is bound to the master.  This is achieved with a colocation rule and an order like below::  

   colocation writer_vip_on_master inf: writer_vip ms_MySQL:Master 
   order ms_MySQL_promote_before_vip inf: ms_MySQL:promote writer_vip:start

An alterate way to configure this using pcs is::

   pcs constraint colocation add  writer_vip p_mysql role=Master
   pcs constraint order promote p_mysql then start writer_vip

All together
------------

Here's all the snippets grouped together::

   [root@host-01 ~]# crm configure show
   node host-01 \
         attributes p_mysql_mysql_master_IP="172.30.222.193"
   node host-02 \
         attributes p_mysql_mysql_master_IP="172.30.222.212"
   primitive p_mysql ocf:percona:mysql_prm \
         params config="/etc/my.cnf" pid="/var/lib/mysql/mysqld.pid" socket="/var/run/mysqld/mysqld.sock" replication_user="repl_user" replication_passwd="ola5P1ZMU" max_slave_lag="60" evict_outdated_slaves="false" binary="/usr/libexec/mysqld" test_user="test_user" test_passwd="2JcXCxKF" \                                                                                           
         op monitor interval="5s" role="Master" OCF_CHECK_LEVEL="1" \
         op monitor interval="2s" role="Slave" OCF_CHECK_LEVEL="1" \
         op start interval="0" timeout="60s" \
         op stop interval="0" timeout="60s"
   primitive reader_vip_1 ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.101" nic="eth1" \
         op monitor interval="10s"
   primitive reader_vip_2 ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.102" nic="eth1" \
         op monitor interval="10s"
   primitive writer_vip ocf:heartbeat:IPaddr2 \
         params ip="172.30.222.100" nic="eth1" \
         op monitor interval="10s"
   ms ms_MySQL p_mysql \
         meta master-max="1" master-node-max="1" clone-max="2" clone-node-max="1" notify="true" globally-unique="false" target-role="Master" is-managed="true"
   location loc-No-reader-vip-2 reader_vip_2 \
         rule $id="rule-no-reader-vip-2" -inf: readable lt 1
   location loc-no-reader-vip-1 reader_vip_1 \
         rule $id="rule-no-reader-vip-1" -inf: readable lt 1
   colocation writer_vip_on_master inf: writer_vip ms_MySQL:Master
   order ms_MySQL_promote_before_vip inf: ms_MySQL:promote writer_vip:start
   property $id="cib-bootstrap-options" \
         dc-version="1.1.6-3.el6-a02c0f19a00c1eb2527ad38f146ebc0834814558" \
         cluster-infrastructure="openais" \
         expected-quorum-votes="2" \
         no-quorum-policy="ignore" \
         stonith-enabled="false" \
         last-lrm-refresh="1338928815"
   property $id="mysql_replication" \
         p_mysql_REPL_INFO="172.30.222.193|mysqld-bin.000002|106"


You'll notice toward the end, the ``p_mysql_REPL_INFO`` attribute (the value may differ) that correspond to the master status when it has been promoted to master.
If using pcs ``pcs config`` will show a similar output.
 
--------
Behavior
--------

Crash of master
===============

If the node where the master was running is still up, PRM will try to restart it once per hour, this is the best way of insuring no data is lost.  

If it keeps crashing or if the whole master server crashed, a failover will occur.  In such case, PRM will detect the master crashed and will initiate the following procedure:

   # The best candidate for the master role is found, based on the amount of binlog downloaded from the crashed master
   # The newly elected master publishes to the cib the binlog positions and md5 hashes of the payload for last 3000 transactions in its binlog up to 1 minute back in time.
   # The other slaves, will calculate the md5 of their last trx in their respective relay log and will find the corresponding binlog file and position for the transactions published in the cib by the new master.
   
Note: This behavior also requires binlog XID events that are only generated with Innodb so it doesn't work with MyISAM.


Determining the best master candidate
=====================================

During normal operation, when there's a failover, all slaves are at the same point so that's not critical. It is different when the master crashed, slaves may not all be at the same point and it is very important to pick the most up to date one.  In order to achieve this, the master publishes its current master status at every monitor operation and when PRM needs to determine who's the best candidate, the score will be calculated like this::
   
   master_score=100000000 + ((current_slave_master_log_file_number - last_reported_master_log_file_number) * master_max_binlog_size +
               current_slave_master_log_pos - last_reported_master_log_pos)/100

-------------------------
Useful Pacemaker commands
-------------------------

To check the cluster status
===========================

Two tools can be used to query the cluster status, ``crm_mon`` and ``crm status`` (equivalent ``pcs status``).  They produce the same output but ``crm_mon`` is more like top, it stays on screen and refreshes at every changes.  ``crm status`` is a one time status dump.  The output is the following::

   [root@host-01 ~]# crm status
   ============
   Last updated: Tue Jun  5 17:09:01 2012
   Last change: Tue Jun  5 16:43:08 2012 via cibadmin on host-01
   Stack: openais
   Current DC: host-01 - partition with quorum
   Version: 1.1.6-3.el6-a02c0f19a00c1eb2527ad38f146ebc0834814558
   2 Nodes configured, 2 expected votes
   5 Resources configured.
   ============

   Online: [ host-01 host-02 ]

   Master/Slave Set: ms_MySQL [p_mysql]
      Masters: [ host-01 ]
      Slaves: [ host-02 ]
   reader_vip_1   (ocf::heartbeat:IPaddr2):       Started host-01
   reader_vip_2   (ocf::heartbeat:IPaddr2):       Started host-02
   writer_vip     (ocf::heartbeat:IPaddr2):       Started host-01

To view and/or edit the configuration
=====================================

To view the current configuration use ``crm configure show`` (equilvalent ``pcs config``) and to edit, use ``crm configure edit``.  The later command starts the vi editor on the current configuration.  If you want to use another editor, set the EDITOR session variable.  Editing resources with pcs can be done with ``pcs resource modify`` however for constraits look at the ``pcs constraint help`` for options.

To change a node status
=======================

It is often required to put a node in standby mode in order to perform maintenance operations on it.  The best way is to use the ``standby`` node status.  Let's consider this initial state::

   root@host-02:~# crm status
   ============
   Last updated: Fri Nov 23 09:17:31 2012
   Last change: Fri Nov 23 09:16:40 2012 via crm_attribute on host-01
   Stack: openais
   Current DC: host-01 - partition with quorum
   Version: 1.1.7-ee0730e13d124c3d58f00016c3376a1de5323cff
   2 Nodes configured, 2 expected votes
   5 Resources configured.
   ============

   Online: [ host-01 host-02 ]

   Master/Slave Set: ms_MySQL [p_mysql]
      Masters: [ host-01 ]
      Slaves: [ host-02 ]
   reader_vip_1   (ocf::heartbeat:IPaddr2):       Started host-02
   reader_vip_2   (ocf::heartbeat:IPaddr2):       Started host-01
   writer_vip     (ocf::heartbeat:IPaddr2):       Started host-01

Now, if we want to put host-02 in standby we do ``crm node standby host-02``, which, after a few seconds will produce the status::

   root@host-02:~# crm status
   ============
   Last updated: Fri Nov 23 09:25:21 2012
   Last change: Fri Nov 23 09:25:11 2012 via crm_attribute on host-02
   Stack: openais
   Current DC: host-01 - partition with quorum
   Version: 1.1.7-ee0730e13d124c3d58f00016c3376a1de5323cff
   2 Nodes configured, 2 expected votes
   5 Resources configured.
   ============

   Node host-02: standby
   Online: [ host-01 ]

   Master/Slave Set: ms_MySQL [p_mysql]
      Masters: [ host-01 ]
      Stopped: [ p_mysql:1 ]
   reader_vip_1   (ocf::heartbeat:IPaddr2):       Started host-01
   reader_vip_2   (ocf::heartbeat:IPaddr2):       Started host-01
   writer_vip     (ocf::heartbeat:IPaddr2):       Started host-01

The node host-02 can be put back online with ``crm node online host-02``.  If above we would have chose to put host-01 in standby, the master role would have been switch to host-02 and the result would have been pretty similar, inverting host-01 and host-02 and the above status. 


----------------
Testing failover
----------------

An HA setup is only HA in theory until tested so that's why the testing part is so important.

Basic tests
===========

The basic tests don't require the presence of a stonith device and the minimalistic set of tests that should be performed.  All these tests should be run while sending writes to the master.  As a bare minimum, use simple bash script like::

   #!/bin/bash
   # 
   MYSQLCRED='-u writeuser -pwrites -h 172.30.212.100'

   mysql $MYSQLCRED -e "create database if not exists test;"
   mysql $MYSQLCRED -e "create table if not exists writeload (id int not null auto_increment,data char(10), primary key (id)) engine = innodb;" test
   
   while [ 1 ]
   do
      mysql $MYSQLCRED -e "insert into writeload values (data) values ('test');" test
      sleep 1
   done

Adjust the credentials so that the writes can follow the writer VIP as it moves between servers.  Make sure you don't grant ``SUPER`` since it breaks the read-only barrier.

Manual failover
---------------

If the master is host-01, but it in standby with ``crm node standby host-01`` and check that the inserts resume on the host-02.  The script may have thrown a few errors but that's normal.  Then, put host-01 back online with ``crm node online host-01``, it should be back as a slave and should pickup the missing from replication.  Verify that replication is ok and there are no holes in the ids.

Slave lagging
-------------

The following test is design to verify the behavior of the reader_vips when replication is lagging.  With the above write script still running, run the following query on the master::

   insert into test.writeload select sleep(2*max_slave_lag);

For that to run, max_slave_lag must be larger than the monitor operation interval times the failcount for the slave in the ``p_mysql`` primitive definition.  After you started the query on the master, start the shell tool ``crm_mon``.  After about 3 times the max_slave_lag, the reader_vip should move away from the slave and then after about 4 times max_slave_lag, go back.

Replication broken
------------------

If you break replication by inserting a row on the save in the writeload table, the reader_vip should move away from the affected slave in around the monitor operation interval times the failcount.  Once corrected, the reader_vip should come back.


Crash of master
---------------

A crash of the ``mysqld`` process, on either the master or the slave should cause Pacemaker to restart it .  If the restart are normal, there's no need for the master role to switch over.  If there are more than one crash in a one hour span, a failover will occur.  


Kill of MySQL no restart
------------------------

As we are progressing in our tests, let's be a bit rougher with MySQL, we'll kill the master mysqld process but we will start nc to bind the 3306 port, preventing it to restart.  It is advisable to reduce the ``op start`` and ``op stop`` values for that test, 900s is a long while to wait.  I personally ran the test with both at 20s.  So, on the master, run::

   kill `pidof mysqld`; nc -l -p 3306 > /dev/null &

In my case, the master was host-02.  After a short while the status should be like::

   root@host-02:~# crm status
   ============
   Last updated: Fri Nov 23 13:55:55 2012
   Last change: Fri Nov 23 13:53:06 2012 via crm_attribute on host-01
   Stack: openais
   Current DC: host-01 - partition with quorum
   Version: 1.1.7-ee0730e13d124c3d58f00016c3376a1de5323cff
   2 Nodes configured, 2 expected votes
   5 Resources configured.
   ============

   Online: [ host-01 host-02 ]

   Master/Slave Set: ms_MySQL [p_mysql]
      Masters: [ host-01 ]
      Stopped: [ p_mysql:1 ]
   reader_vip_1   (ocf::heartbeat:IPaddr2):       Started host-01
   reader_vip_2   (ocf::heartbeat:IPaddr2):       Started host-01
   writer_vip     (ocf::heartbeat:IPaddr2):       Started host-01

   Failed actions:
      p_mysql:1_start_0 (node=host-02, call=87, rc=-2, status=Timed Out): unknown exec error

If another node is promoted master than test is successful.  To put thing back in place do the following step on the failed node::

   root@host-02:~# kill `pidof nc`; crm resource cleanup p_mysql:1

   Cleaning up p_mysql:1 on host-01
   Cleaning up p_mysql:1 on host-02
   Waiting for 3 replies from the CRMd... OK
   [1]+  Exit 1                  nc -l -p 3306 > /dev/null
   root@host-02:~#

and host-02 should become a slave of host-01.

Reboot 1 node
-------------

Rebooting any of the nodes should always leave the database system with a master.  Be careful if you reboot nodes in sequences while writing to them, give at least a few seconds for the slave process to catch up.

Reboot all node
---------------

After the reboot, a master should be promoted and the other nodes should be slaves of the master.  


------
How to
------

How to add a new node
=====================

Adding a new node to the corosync and pacemaker cluster will follow the steps listed above that describe installing the packages and configuring corosync.  Then, only start corosync.  If you are on the latest corosync/pacemaker version, you have two disctinct startup script it is easy to start only corosync.  If you are on an older version where only corosync is started, temporarily move the file ``/etc/corosync/service.d/pacemaker`` to a safe place, like /root, and then start corosync.  That will cause the node to appear in the cluster when running ``crm status`` on the old nodes.  Put the new node in standby with ``crm node standby host-09`` assuming the new node hostname is ``host-09``.  Once in standby start pacemaker or for older installs, put the file ``/etc/corosync/service.d/pacemaker`` back in place and restart corosync. 



Once the new node has joined the cluster, you need to let the ``ms`` resource know that it can have another clone (slave).  You can achieve this by increasing the ``clone-max`` attribute by one.

::

   ms ms_MySQL p_mysql \
        meta master-max="1" master-node-max="1" clone-max="3" clone-node-max="1" notify="true" globally-unique="false" target-role="Master" is-managed="true"

Note that the easiest way to make this configuration change is with ``crm configure edit``, which allows you to edit the existing configuration in the EDITOR of your choice.  You may also want to put the pacemaker cluster into maintenance-mode first::

	crm(live)configure# property maintenance-mode=on
	crm(live)configure# commit

If the new node is added successfully to the existing corosync ring and pacemaker cluster, then it should appear in the ``crm status`` and be in the ``standby`` status.  Taking the cluster out of ``maintenance-mode`` should be safe at this point, but be sure to leave your new node in ``standby``.

Once the cluster is out of maintenance and the new node shows up in the configuration, you need to manually clone the new slave and set it up to replicate from whichever node is the active master.  This document will not cover the basics of cloning a slave.  Note that you will have to manually start mysql on your new node (be careful to do this exactly as pacemaker does it on the other nodes) once you have a full copy of the mysql data and before you execute your ``CHANGE MASTER ...; SLAVE START;``

Verify that the new node is working, replication is consistent, and allow it to catch up using standard methods.  Once it is caught up:

#. Shutdown the manually started mysql instance.  ``mysqladmin shutdown`` may be helpful here.
#. Bring the node 'online' in pacemaker.  ``crm node online new_node_name``

The trick here is that PRM will not re-issue a CHANGE MASTER if it detects that the given mysql instance was already replicating from the current master node.  Once this node is online, then it should behave as other slave nodes and failover (and possibly be promoted to the master) accordingly.


How to repair replication
=========================

Repairing replication is an advanced mysql replication topic, which won't be covered in detail here.  However, it should be noted that there are two basic methods to repairing replication:

#. Inline repair (i.e., tools like `pt-table-sync`)
#. Repair by slave reclone (i.e., throw the slave's data away and re-clone it from the master or another slave )


Inline repairs should not require any PRM intervention.  As far as PRM is concerned, it is all normal replication traffic.

Reclone repairs will end up following similar steps to the ``How to add a new node`` steps above.  See above for details, but the basic steps are:

#. Put the offending slave into standby
#. Effect whatever repairs/data copying necessary
#. Bring the slave up manually, configure replication, and wait for it to catch up
#. Shutdown mysql on the slave
#. Bring the slave online in Pacemaker


How to exclude a node from the master role (or less likely to be)
=================================================================

Pacemaker offers a very powerful configuration language to do exactly this, and many variations are possible.   The simplest way is to simply assign a negative priority to the ms Master role and the node you want to exclude::

	location avoid_being_the_master ms_MySQL \
 		rule $role="Master" -1000: #uname eq my_node

This should downgrade the possiblity of ``my_node`` being the master unless there simply are no other candidates.  To prevent ``my_node`` from becoming the master ever, simply take it further::

	location never_be_the_master ms_MySQL \
		rule $role="Master" -inf: #uname eq my_node

How to verify why a reader VIP is not on a slave
================================================

If there's enough reader VIPs for all slaves, the most likely cause is that the slave in question is not suitable for reads.  The best and quickest way to see if a slave is suitable to have a reader VIP is query the CIB like this::

   root@host-02:~# cibadmin -Q | grep readable | grep nvpair
          <nvpair id="status-host-02-readable" name="readable" value="1"/>
          <nvpair id="status-host-01-readable" name="readable" value="1"/>

This is the ``readable`` attribute used in the location rules of the reader VIPs.  If the value is 0, there is something wrong with replication, either it is broken or lagging behind.

How to clean up error in pacemaker
==================================

Pacemaker is rather verbose regarding errors (failed actions) it encounters and it the responsability of a human to acknowledge the errors but once acknowledge, how do you get rid of the error.  Here's an example error output from ``crm status``::

   Online: [ pacemaker-1 pacemaker-2 ]

   Master/Slave Set: ms_MySQL [p_mysql]
      Masters: [ pacemaker-2 ]
      Slaves: [ pacemaker-1 ]
   reader_vip_1   (ocf::heartbeat:IPaddr2):       Started pacemaker-1
   reader_vip_2   (ocf::heartbeat:IPaddr2):       Started pacemaker-2
   writer_vip     (ocf::heartbeat:IPaddr2):       Started pacemaker-2

   Failed actions:
      p_mysql:0_monitor_2000 (node=pacemaker-1, call=10, rc=1, status=complete): unknown error

Such failed actions are remove by this command::

   crm resource cleanup p_mysql:0

where ``p_mysql`` is the primitive name and ``:0`` the clone set instance that has the error.



Configuring a report slave with a dedicated VIP
===============================================

Sometimes, people needs to configure a special slave that is used for report.  This slave needs to be less likely be be the master, more likely to have the report VIP and less likely to have the normal reader VIPs.  Assuming ``pacemaker-3`` is a report slave, here's how this can be implemented.  First, we need a rule to lower the score to become a master, the rule will look like::

   location pacemaker-3_lesslikely_master ms_MySQL \
        rule $id="pacemaker-3_lesslikely_master-rule" $role="master" -50: #uname eq pacemaker-3.dc1.beachbody.com

Next, we need to favor the ``report-vip`` to be on ``pacemaker-3``.  The rule for this is::

   location report-vip_prefers_ptbb-mys5 report-vip \
        rule $id="rule-report-vip_prefers_pacemaker-3" 150: #uname eq pacemaker-3
        
Then of course, we need the existing regular reader VIPs to be less likely on ``pacemaker-3``::

   location reader_vip_1_lesslikely_ reader_vip_1 \
        rule $id="rule-reader_vip_1_lesslikely_pacemaker-3" -50: #uname eq pacemaker-3
   location reader_vip_2_lesslikely_ reader_vip_2 \
        rule $id="rule-reader_vip_2_lesslikely_pacemaker-3" -50: #uname eq pacemaker-3
        
        
Enabling trace in the resource agent
====================================

The golden way of debugging a PRM setup is with the agent trace file which is the output of "bash -x".  To enable the trace file simply do::

   mkdir -p /tmp/mysql.ocf.ra.debug
   touch /tmp/mysql.ocf.ra.debug/log

Be aware, this is a very chatty file, about 20MB/h.  If left unattented, it can fill a disk.  When you are done, simply remove the log file.  
If you plan to keep it there, add a logrotate config file like:: 

   [root@host-01 mysql.ocf.ra.debug]# more /etc/logrotate.d/mysql-ra-trace
   /tmp/mysql.ocf.ra.debug/log {
         # create 600 mysql mysql
         notifempty
         daily
         rotate 4
         missingok
         compress
      postrotate
         touch /tmp/mysql.ocf.ra.debug/log
      endscript
   }


Preventing failback
===================

In some cases, for operational and backup concerns, it may be required to have a preferred master and allow failover to a slave but not a failback to the preferred master after a failure.  This can be achieve fairly easily with a post-promote script.  Assuming pacemaker-1 is the preferred master, pacemaker-2 the failover slave, create the following script only on pacemaker-2::

    root@pacemaker-2 # chmod u+x /usr/local/bin/post-promote-pacemaker-2
    root@pacemaker-2 # cat /usr/local/bin/post-promote-pacemaker-2
    /usr/sbin/crm_attribute -N pacemaker-2 -n prm-no-failback -l forever -v 1

then, create the prm-no-failback attribute in the cib with value 0::
    
    root@pacemaker-2 # /usr/sbin/crm_attribute -N pacemaker-2 -n prm-no-failback -l forever -v 0
    root@pacemaker-2 # /usr/sbin/crm_attribute -N pacemaker-1 -n prm-no-failback -l forever -v 0 
    
and add the following location rule::

    location loc-no-failback ms_MySQL \
        rule $id="rule-no-failback" $role="master" -inf: prm-no-failback eq 1

Every time the node pacemaker-2 is promote to master, it will set the attribute to 1, preventing pacemaker-1 to return to the master role.  To monitor the attribute value, use ``crm_mon -A1``.  To re-enable pacemaker-1 to the master role, you'll need to run::

    /usr/sbin/crm_attribute -N pacemaker-1 -n prm-no-failback -l forever -v 0


Manual failover for replication
===============================    

In order to manually control replication failover, one can extend the "Preventing failback" recipe by adding the following rules to the cib::
   
   location loc-allowed-master ms_MySQL \
      rule $id="rule-allowed-master" $role="master" -inf: p_mysql_master_allowed eq 1
      
and then, the node or nodes, allowed to be master will have::

   crm_attribute -N pacemaker-1 -n p_mysql_master_allowed -l forever -v 1
   
and the other one::

   crm_attribute -N pacemaker-2 -n p_mysql_master_allowed -l forever -v 0
   crm_attribute -N pacemaker-3 -n p_mysql_master_allowed -l forever -v 0

If at some point, the master role need to be moved to the pacemaker-2 node, then, simply run::

      crm_attribute -N pacemaker-2 -n p_mysql_master_allowed -l forever -v 1
      crm_attribute -N pacemaker-1 -n p_mysql_master_allowed -l forever -v 0
      
---------------
Advanced topics
---------------

VIPless cluster (cloud)
=======================

With many cloud provider, it is not possible to have virtual IPs so in that case, how can we reach the MySQL server.  For simplicity we'll consider only the master access, accessing the slaves for reads in such environment is possible but more challenging.  The principle of operation here will be to also run pacemaker on the application servers but instead of running MySQL, they'll be running a fake MySQL resource agent that will reconfigure access to the master based on the post-promote notification it will receive from the pacemaker cluster.  Configure the application with pacemaker like described above for a MySQL server but keep the node in standby for now.  Then, replace the mysql agent using the following procedure::

   [root@app-01 corosync]# cd /usr/lib/ocf/resource.d/
   [root@app-01 resource.d]# mkdir percona
   [root@app-01 resource.d]# cd percona/
   [root@app-01 percona]# wget -q -O mysql https://github.com/percona/percona-pacemaker-agents/raw/master/agents/fake_mysql_novip
   [root@app-01 percona]# chmod u+x mysql

By default the IP and port used are::

   Fake_Master_IP=74.125.141.105  #a google IP
   Fake_Master_port=3306

You must make sure your application use these values to connect to the master even though it is likely not the actual IP of the master server.  Next, we must change the configuration of Pacemaker in order to grow the master-slave clone set and prevent the master role from running on the application server node.  If initially we had 3 database nodes we would be replacing::

   ms ms_MySQL p_mysql \
        meta master-max="1" master-node-max="1" clone-max="3" \
        clone-node-max="1" notify="true" globally-unique="false" \
        target-role="Master" is-managed="true"

with::

   ms ms_MySQL p_mysql \
        meta master-max="1" master-node-max="1" clone-max="4" \
        clone-node-max="1" notify="true" globally-unique="false" \
        target-role="Master" is-managed="true"
   location app_01_not_master ms_MySQL \
        rule $id="app_01_not_maste-rule" $role="master" -inf: #uname eq app-01

If you have many application servers, you can add them in a similar way.


Non-multicast cluster (cloud)
=============================

Cloud environment are also well known for their lack of support for Ethernet multicast (and broadcast).  There are 2 solutions to this problem, one using Heartbeat unicast and the other using Corosync udpu.  For Heartbeat, the ha.cf file will look like::

   autojoin any
   ucast eth0 10.1.1.1
   ucast eth0 10.1.1.2
   ucast eth0 10.1.1.3
   warntime 5
   deadtime 15
   initdead 60
   keepalive 2
   crm respawn

and for corosync, the corosync.conf file with the udpu configuration looks like::

   compatibility: whitetank

   totem {
         version: 2
         secauth: on
         threads: 0
         interface {
                  member {
                           memberaddr: 10.1.1.1
                  }
                  member {
                           memberaddr: 10.1.1.2
                  }
                  member {
                           memberaddr: 10.1.1.3
                  }
                  ringnumber: 0
                  bindnetaddr: 10.1.1.0
                  netmask: 255.255.255.0
                  mcastport: 5405
                  ttl: 1
         }
            transport: udpu
   }

   logging {
         fileline: off
         to_stderr: no
         to_logfile: yes
         to_syslog: yes
         logfile: /var/log/cluster/corosync.log
         debug: off
         timestamp: on
         logger_subsys {
                  subsys: AMF
                  debug: off
         }
   }

   amf {
         mode: disabled
   }

Be aware that in order to use ``udpu`` with corosync, you need version 1.3+.

Stonith devices
===============

An HA setup without stonith devices is relying on the willingness of the nodes to perform the required tasks.  When everything is running fine, there's no problem to make such an assumption but if you are considering HA, it is because you want to cover cases where things are going wrong.  For example, take one of the simplest HA resource, a VIP.  In order to create and remove the VIP, Pacemaker needs to access the ``/sbin/ip`` binary.  What happends if the filesystem is not available?  The kernel has the VIP defined but Pacemaker is unable to remove it.  Another node in the cluster will start the VIP and boom... you have twice the same IP on your network.  So, you need a way to resolve cases when a node cannot perform a required task like releasing a resource.  Fencing is answer and stonith (Shoot The Other Node In The Head) devices are the implementation.  There are many stonith devices available but the most commons are IPMI and ILO.  To get access to the most recent stonith devices, install the package ``fence-agents`` from RedHat cluster, these are usable with Pacemaker.  In pacemaker, stonith devices are defined a bit like normal primitives.  Here's an example using ILO::

   primitive stonith-host-01 stonith:fence_ilo \
         params pcmk_host_list="host-01" pcmk_host_check="static-list" \
         ipaddr="10.1.2.1" login="iloadmin" passwd="ilopass" verbose="true" \
         op monitor interval="60s"
   primitive stonith-host-02 stonith:fence_ilo \
         params pcmk_host_list="host-02" pcmk_host_check="static-list" \
         ipaddr="10.1.2.2" login="iloadmin" passwd="ilopass" verbose="true" \
         op monitor interval="60s"
   location stonith-host-01_loc stonith-host-01 \
         rule $id="stonith-host-01_loc-rule" -inf: #uname eq host-01
   location stonith-host-02_loc stonith-host-02 \
         rule $id="stonith-host-02_loc-rule" -inf: #uname eq host-02

In the above example, IPs in the 10.1.2.x are the IPs of the ILO devices.  For each ILO device, you specify in the pcmk_host_list which host it fences. We also need location rules to prevent a stonith device to run on the node it is supposed to kill.


Using heartbeat
===============

Although Corosync is now the default communication stack with Pacemaker, Pacemaker works also well with Hearbeat. Here are the steps you need to configure Heartbeat instead of Corosync.  The first thing, you need a cluster key which can be created as simply as::

   echo 'auth 1' > /etc/ha.d/authkeys
   echo -n '1 sha1 ' >> /etc/ha.d/authkeys
   date | md5sum >> /etc/ha.d/authkeys
   chown root.root /etc/ha.d/authkeys
   chmod 600 /etc/ha.d/authkeys

Copy this file to all the nodes and preserve the ownership and rights.  Then, we must configure heartbeat to use pacemaker.  Here's a very simple Heartbeat configuration file (/etc/ha.d/ha.cf)::

   autojoin any
   bcast eth0
   warntime 5
   deadtime 15
   initdead 60
   keepalive 2
   crm respawn

Any node with the right authkeys file will be able to join (autojoin any).  Communication will be using ethernet broadcast (bcast) but multicast or even unicast could also be used.  Finally, Pacemaker is started with the "crm respawn" line.  Compared to the corosync setup described above, in order to start Pacemaker with Heartbeat, you just need to start Heartbeat.


Performing rolling restarts for config changes
==============================================

Because failover is automated on the PRM cluster, performing rolling configuration changes that require mysql restart (i.e., not dynamic variables) is fairly straightforward:

#. Set the node to standby
#. Make configuration changes
#. Set the node to online
#. Go to the next node

Backups with PRM
================

There are a few basic ways to take a mysql backup, so depending on your method it will affect what steps you need to take in pacemaker (if any).

If MySQL can continue running and the load of the backup is not a problem for continuing service on the slave, then you don't need to do anything.  Simply take your backup and allow normal service to continue.

If you need to shift production traffic away from the node (i.e., a reader vip), then simply move the resource to some other node::

	crm move slave_vip_running_on_backup_node not_the_backup_node

Perform your backup here (note replication will remain running, but tools like mysqldump should not have a problem with this because it either locks the tables or wraps its backup in a transaction).  Then, to allow pacemaker to resume management of that vip::

	crm unmove the_slave_vip_you_moved


If you need to fully shutdown mysql to take your backup, it's best to simply standby the node::

	crm node standby backup_node


Telling the Reader VIPs to avoid the Writer VIP
==================================================

If we want the master to take the reader vips if no other slaves are available then it should move the reader vip away from the master.  We can do this with this rule (need one for each reader vip)::

	colocation col_vip_dislike_each_other -200: reader_vip_1 writer_vip


---------------
Troubleshooting
---------------


Diagnosing Resource Placement Issues
====================================

Sometimes if a resource isn't going where you expected, you need to try to track down why.  Ultimately pacemaker configuration only does what you tell it (except when it doesn't), so it's important to try to look at the inputs it uses in the scoring system.

If you run ``crm_simulate -s -L`` you can see a list of scores for each resource on each node.  The highest score should get the resource.


*further topics*:

+ Determining good backup candidate (i.e., not the master)
+ Prohibiting the selected backup node from being eligible for the master during the backup.
+ Using Xtrabackup's --safe-slave-backup with a PRM slave (see `Issue Here <https://github.com/jayjanssen/Percona-Pacemaker-Resource-Agents/issues/3>`_)

Special slave
  less likely master
  sticky vip
