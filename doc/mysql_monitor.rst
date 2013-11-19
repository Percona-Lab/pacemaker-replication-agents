============================= 
Using the mysql_monitor agent
=============================

Author: Yves Trudeau, Percona

November 2013

.. contents::

========
Overview
========

The mysql_monitor agent is a Pacemaker resource agent that just monitor MySQL
in order to set attributes allowing to control virtual IPs.  The agent can operate
in 3 modes: replication (default), pxc and read-only.  

The simplest mode is ``read-only``, only the state on the read_only variable is 
looked at.  If the node has *read_only* variable set to OFF, then the writer is 
set to 1 and reader attributes is set to 1 while if the node has *read_only* set 
to ON, the writer attributes will be set to 0 and the reader attribute set to 1. 

In ``replication`` mode, the writer and reader attributes are set to 1 on the nodes 
where the *read_only* variable is set to OFF and on the nodes where the *read_only* 
variable is set to ON, the reader attribute is set according to the replication 
state.

Finally, in the ``PXC`` mode, both attributes are set according to the return code 
of the clustercheck script. 

In all cases, if MySQL is not running, the reader and writer attributes are set to
0.

Installation
============

This document assumes you have a working Pacemaker cluster, if you don't, refer
to the PRM-setup-guide.  To install the agent::

    root@pacemaker-1:~# mkdir -p /usr/lib/ocf/resource.d/percona
    root@pacemaker-1:~# cd /usr/lib/ocf/resource.d/percona
    root@pacemaker-1:~# wget -O mysql_monitor https://github.com/percona/percona-pacemaker-agents/blob/master/agents/mysql_monitor
    root@pacemaker-1:~# chmod u+x mysql_monitor
    
Parameters
==========

The agent has the following parameters.

=======================  ========================================================================================================
Parameter                Description
=======================  ========================================================================================================
client_binary            Location of the MySQL client binary. *default on Linux: /usr/bin/mysql*

state                    State file, this shouldn't be changed

user                     MySQL user to connect and check MySQL, needs "Replication client" privilege

password                 Password of the user

pid                      Path of the MySQL pid file, used to detect if MySQL is running *default: /var/run/mysql/mysqld.pid*

socket                   MySQL socket file *default: /var/lib/mysql/mysql.sock*

reader_attribute         The reader attribute *default: readable*

reader_failcount         The number of times a monitor operation can find the slave to be unsuitable for reader VIP 
                         before failing.  Useful if there are short intermittent issues like clock adjustments in VMs 
                         causing abnormally high slave lag. Be aware that the reader attribute value will be set to this
                         value when the node is healthy and then decremented by one for each failure. Normally only useful
                         for replication, but can be used with other cluster types. *default: 1*

writer_attribute         The writer attribute *default: writable*

max_slave_lag            The maximum number of seconds a replication slave is allowed to lag behind its master. 
                         Do not set this to zero. When seconds behind master is greater than max_slave_lag and cluster_type 
                         is replication, the reader attribute is set to 0. Only relevant to cluster type replication. *default: 3600*

cluster_type             Three possible cluster type values: replication, read-only and pxc. *default: replication* 
    
    

Sample Pacemaker configuration
==============================

---------
Primitive
---------

Here's a sample primitive declaration for Pacemaker::

    primitive p_mysql_monit ocf:percona:mysql_monitor \
        params user="repl_user" password="WhatAPassword" pid="/var/lib/mysql/mysqld.pid" \
          socket="/var/run/mysqld/mysqld.sock" cluster_type="pxc" \
        op monitor interval="1s" timeout="30s" OCF_CHECK_LEVEL="1"
        
The important point here is that the monitor operation is mandatory to get the correct behavior.

---------
Clone set
---------

Once we have the primitive defined, we need to allow it to run on all the cluster nodes, this is accomplished
with a clone set::

    clone cl_mysql_monitor p_mysql_monit \
        meta clone-max="3" clone-node-max="1"
        
In this cluster, we have 3 nodes.  The command ``crm_mon -A1`` can be used to check if the agent is 
working correctly, if the above case, the output is::

    root@pacemaker-1:~# crm_mon -A1
    ============
    Last updated: Tue Nov 19 14:31:08 2013
    Last change: Tue Nov 12 11:50:27 2013 via crmd on pacemaker-2
    Stack: openais
    Current DC: pacemaker-3 - partition with quorum
    Version: 1.1.7-ee0730e13d124c3d58f00016c3376a1de5323cff
    3 Nodes configured, 3 expected votes
    3 Resources configured.
    ============

    Online: [ pacemaker-1 pacemaker-2 pacemaker-3 ]

     Clone Set: cl_mysql_monitor [p_mysql_monit]
         Started: [ pacemaker-1 pacemaker-2 pacemaker-3 ]

    Node Attributes:
    * Node pacemaker-1:
        + readable                          : 1         
        + writable                          : 1         
    * Node pacemaker-2:
        + readable                          : 1         
        + writable                          : 1         
    * Node pacemaker-3:
        + readable                          : 1         
        + writable                          : 1 
        
-------------
Handling VIPs
-------------

Like with PRM, VIPs can be defined and managed by the attributes.  Here's an example using::

    primitive writer_vip ocf:heartbeat:IPaddr2 \
        params ip="172.30.212.100" nic="eth1" \
        op monitor interval="10s" 
    primitive reader_vip_1 ocf:heartbeat:IPaddr2 \
        params ip="172.30.212.101" nic="eth1" \
        op monitor interval="10s" 
    primitive reader_vip_2 ocf:heartbeat:IPaddr2 \
        params ip="172.30.212.102" nic="eth1" \
        op monitor interval="10s"
    location No-reader-vip-1-loc reader_vip_1 \
        rule $id="No-reader-vip-1-rule" -inf: readable eq 0
    location No-reader-vip-2-loc reader_vip_2 \
        rule $id="No-reader-vip-2-rule" -inf: readable eq 0
    location No-writer-vip-loc writer_vip \
        rule $id="No-writer-vip-rule" -inf: writable eq 0
    colocation col_vip_dislike_each_other -200: reader_vip_1 reader_vip_2 writer_vip
    
