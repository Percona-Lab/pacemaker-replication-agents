Percona resource agents
=======================

fake_mysql_novip  
----------------

Agent useful in a VIPless environment to redirect application server to the database master with IPtables.  Basically, the application servers are part of the PRM master-slave clone set and forbidden to be master.  Instead of the mysql_prm script, they run the fake_mysql_novip script (rename mysql_prm) locally.  They'll get the notification and readapt an IPtables rule to direct traffic to the master.

IPaddr3  
------- 

A slightly modified version of the IPaddr2 script that better behaves with ClusterIP. See: http://www.mysqlperformanceblog.com/2014/01/10/using-clusterip-load-balancer-pxc-prm-mha/.


mysql_monitor  
-------------

A script to monitor mysql and set pacemaker attributes to control VIPs.  See: http://www.mysqlperformanceblog.com/2013/11/20/add-vips-percona-xtradb-cluster-mha-pacemaker/


mysql_prm  
---------

The PRM resource agent for regular async replication.


mysql_prm_gtid
-----------

The PRM resource agent for gtid based replication.

