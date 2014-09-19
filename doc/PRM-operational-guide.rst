=================================================== 
Percona replication manager (PRM) operational guide
===================================================

Author: Yves Trudeau, Percona

State: first draft completed

May 2013

.. contents::

--------
Overview
--------

Once you have a running Percona replication manager (PRM) cluster, you'll need to operate it correctly.  The goal of this guide is to provide the sysadmin or dba the information they need to perform the daily maintenance tasks with the PRM cluster.  If you are more interested by the install of a PRM cluster, have you a look the accompanying ``PRM-setup-guide`` which this guide assumes some notions and repeats some other.

----------
Monitoring
----------

One of the most common thing an operator will do is to look at the cluster status.  The preferred tool to look at the cluster status is ``crm_mon``.  With no arguments, the tool display the status and refreshes only when there's a cluster change, to stop it you'll need to type Ctrl-c.  Adding "-1" cause the tool to run only once and exit.  Other useful arguments are:

- "-A" to see the attributes which are sometime useful to understand decision taken by Pacemaker
- "-o" to get the lastest operations
- "-t" to get the operations execution times

The tool can also be used as a daemon to write the cluster status to an html file or to send an email at every cluster change.

Nagios
------

Monitoring replication with Nagios is a bit more tricky with PRM in place since the master role may move.  The replication test script must first determine if it is running on the master of on a slave.  Here's a simple test using the Percona Monitoring Plugins for Nagios::

    #!/bin/bash

    hostname=`uname -n`

    if [ `crm_mon -1 | grep $hostname | grep -c Master` -gt 0 ]; then
            echo "OK running as master"
            exit 0
    else   
            /usr/lib64/nagios/plugins/pmp-check-mysql-replication-running
            rc=$?
            exit $rc
    fi

----------------
Maintenance mode
----------------

There's one feature that is very useful when you are doing non regular operations on the cluster, the ``maintenance mode``. Basically, in maintance mode, Pacemaker take a break, it stop the monitoring and leave everything as it is.  Of course, automatic failover is gone but you won't get any surprize from Pacemaker.  To turn on maintenance mode you do::

    crm configure property maintenance-mode=true
    
once in maintenance mode, if you check the cluster status with ``crm_mon -1``, you'll see that all the resources are unmanaged.  When you're done and want Pacemaker to resume its work, you'll need to do::

    crm configure property maintenance-mode=false

It is recommend to look at the trace file (see below) to check if the monitor operations have restarted.  Some versions of Pacemaker had issues with this.  If you don't see activity in the trace file, run ``crm_resource --reprobe``, that should fix the issue.

--------------
Failed actions
--------------

Failed actions occur when an agent action fails or returns an error code.  Failed actions requires a human to intervene, they shouldn't be managed automatically.  The main reasons for failure are: 

- Error returned by the agent
- Syntax error in the agent code
- Timeout

Error returned by the agent are normal and require to be addressed, depending on the operational status of the cluster.  The example below is a failed monitor action returning an error (rc=1) because the mysqld process have been killed manually::

    root@pacemaker-1-1:~# crm_mon -1
    ============
    Last updated: Fri May 17 11:28:02 2013
    Last change: Fri May 17 11:01:06 2013 via crmd on pacemaker-1-2
    Stack: openais
    Current DC: pacemaker-1-2 - partition with quorum
    Version: 1.1.7-ee0730e13d124c3d58f00016c3376a1de5323cff
    2 Nodes configured, 2 expected votes
    5 Resources configured.
    ============

    Online: [ pacemaker-1-2 pacemaker-1-1 ]

     reader_vip1    (ocf::heartbeat:IPaddr2):       Started pacemaker-1-1
     reader_vip2    (ocf::heartbeat:IPaddr2):       Started pacemaker-1-2
     writer_vip     (ocf::heartbeat:IPaddr2):       Started pacemaker-1-2
     Master/Slave Set: ms_MySQL [p_mysql]
         Masters: [ pacemaker-1-2 ]
         Slaves: [ pacemaker-1-1 ]

    Failed actions:
        p_mysql:0_monitor_10000 (node=pacemaker-1-1, call=20, rc=7, status=complete): not running
        
Here, even though pacemaker restarted mysql on pacemaker-1-1, the failed action won't go away.  Here, no need to investigate since the cause is well  In order to clear the failed action run::

    crm resource cleanup p_mysql:0

Here, "p_mysql:0" refers to the instance "0" of the ms_MySQL clone set. The most frequent source of timeouts (status=timeout) are the start and stop operations because often timeouts are set too low in the configuration for these operations.  Remember that upon start, MySQL may have to perform an InnoDB recovery and when stopping, it has to flush the dirty pages.  Both of these operations may take quite some time, ajust your configuration accordingly.  If you get timeouts for the monitor, promote, demote operations, that's more concerning, look at swapping.  In more complex cases, you'll need to investigate why the action failed.  First to place to look is the trace file if you activated it (see below) and syslog for pacemaker logs.  


--------------------
The agent trace file
--------------------

The agent trace file is the best tool to understand why something went wrong.  Although, it is very verbose, enabling it and setting up log rotation on it allows an easy access to the trace and prevent the disk from filling up.  To activate the trace file, simple do::

    root@pacemaker-1-1:~# mkdir /tmp/mysql.ocf.ra.debug
    root@pacemaker-1-1:~# touch /tmp/mysql.ocf.ra.debug/log
    
the agent will detect the presence of the file and will start logging to it.  To stop the trace remove or rename the file "log".  Here's a typical header of an event trace::

    Fri May 17 14:09:38 EDT 2013
    monitor
    OCF_RA_VERSION_MAJOR=1
    OCF_RA_VERSION_MINOR=0
    OCF_RESKEY_CRM_meta_OCF_CHECK_LEVEL=1
    OCF_RESKEY_CRM_meta_clone=0
    OCF_RESKEY_CRM_meta_clone_max=2
    OCF_RESKEY_CRM_meta_clone_node_max=1
    ...
    
you have the date, the operation, monitor in the example and then all the variables passed to the script by Pacemaker.  After that, you'll have the bash trace of the agent script and, in this case it ends with::

    ...
    + ocf_log debug 'MySQL monitor succeeded'
    + '[' 2 -lt 2 ']'
    + __OCF_PRIO=debug
    + shift
    + __OCF_MSG='MySQL monitor succeeded'
    + case "${__OCF_PRIO}" in
    + __OCF_PRIO=DEBUG
    + '[' DEBUG = DEBUG ']'
    + ha_debug 'DEBUG: MySQL monitor succeeded'
    + '[' x0 = x0 ']'
    + return 0
    + return 0

after a few hundred lines.  Depending on the monitor interval, the trace file may grow by more than 300MB per day.  To keep this manageable it is recommended to activate logrotate on the trace like::

    root@pacemaker-1-1:~# cat /etc/logrotate.d/pacemaker-ra 
    /tmp/mysql.ocf.ra.debug/log {
            notifempty
            daily
            rotate 7
            missingok
            compress
        postrotate
            touch /tmp/mysql.ocf.ra.debug/log
        endscript
    }

--------
Swapping
--------

It is for a good reason that some high-availability solutions, like MySQL NDB cluster, prohibit to call malloc after the startup phase.  Swapping for clusters is bad.  This is also very true for a PRM cluster.  Under high swap, operations will timeout leaving the cluster in a totally unpredictable state.  If you ended with a messed up setup where the actual replication master is different from the one defined in the cib,  look at swapping.  Here're some recommendations to avoid swapping

swappiness
----------

Set swappiness to 0, the file cache isn't more precious than MySQL data in memory. Do::

    echo "vm.swappiness = 0" >> /etc/sysctl.conf
    sysctl -p
    
OS write cache
--------------

Less trivial, but quite dangerous is the OS write cache.  If you happened to write a lot of data disk without using O_DIRECT, for example while running xtrabackup, you'll endup with a lot of dirty pages in memory.  The default value allows up to 20% of the physical RAM to contains dirty pages, not yet written to disk.  That can hurts.  To avoid this::

    echo "vm.dirty_bytes = 536870912" >> /etc/sysctl.conf
    echo "vm.dirty_expire_centisecs = 500" >> /etc/sysctl.conf
    echo "vm.dirty_writeback_centisecs = 100" >> /etc/sysctl.conf
    sysctl -p
    
NUMA
----

Numa can be another source of swapping is there's an imbalance between the physical CPU. I recommend you follow the guidelines set by Jeremy Cole from http://blog.jcole.us/2010/09/28/mysql-swap-insanity-and-the-numa-architecture/ and http://blog.jcole.us/2012/04/16/a-brief-update-on-numa-and-mysql/

MySQL configuration
-------------------

Since you are looking at HA, don't be over zealous configuring MySQL, be on the conservative side when you allocate memory.  Of course you can set the innodb_buffer_pool_size so that you free memory ends up being very small and that's likely a very optimal setup but it is running for trouble.  Leave a bit more free than you would normally on a standalone server. 

----------------------
Recovering replication
----------------------

In some cases, you may have to correct replication issues.  Most of these case should never happened but that's in theory, in practice they may happened and nearly also because of swapping or network outage.  

Failed actions on the slave, MySQL down
---------------------------------------

Since this is a slave, it is safe to simply clear the failed action.  MySQL should start normally and resume replication with the master.


Failed actions on the slave, MySQL up
-------------------------------------

In such a case most likely on of the failed action relates to the "stop" operation.  Put the node in standby, stop MySQL, cleanup the failed actions and put the node back online.  MySQL should start normally and resume replication with the master.


Failed actions on the master, MySQL up
-------------------------------------

That's more problematic since cleaning up the failed action will likely cause a failover so first, demote the master-slave clone set before cleaning up the failed actions.  If the MySQL master-slave clone set is name ms_MySQL, you'll end do::

    crm resource demote ms_MySQL
    crm resource cleanup ....
    crm resource promote ms_MySQL
    
The master role will change but since you demoted first, everything will be fine. 

Wrong master
------------

That a very silly case that requires heavy swapping.  A node is the effective master and has the writer_vip but in the cib replication attribute, another host is defined as master.  Further more, you may have slaves pointing to either ones.  The explanation for this is that when promotion occurred, the promote operation timed out before the cib was updated, again, because of swapping.  So, the correct master is the one with the writer vip.  The first this to do is to make sure the master writes its coordinates in the cib replication attribute otherwise all the updates since when the problem occurred will be in limbo.  In order to do this, put all nodes in standby and then put back online _only_ the node that was master until it is promoted.  Then of course, all the slaves are screwed and you'll need to reprovision them with a backup from the master.  Once done, investigate swapping. 

Reloading the data
------------------

The best way to reload a backup on a slave is to first put it in standby mode and then, start MySQL manually.  Restore the backup as usual, configure replication, being careful to use the same definition for the master like in the cib replication attribute, hostname or IP.  Once the slave is in sync, stop MySQL and put the host online.  It should happily join the master as slave. 

-----------
Adding node
-----------

Adding a new node to the corosync and pacemaker cluster will follow the steps listed in the setup guide that describe installing the packages and configuring corosync.  Then, only start corosync.  If you are on the latest corosync/pacemaker version, you have two disctinct startup script it is easy to start only corosync.  If you are on an older version where only corosync is started, temporarily move the file ``/etc/corosync/service.d/pacemaker`` to a safe place, like /root, and then start corosync.  That will cause the node to appear in the cluster when running ``crm status`` on the old nodes.  Put the new node in standby with ``crm node standby host-09`` assuming the new node hostname is ``host-09``.  Once in standby start pacemaker or for older installs, put the file ``/etc/corosync/service.d/pacemaker`` back in place and restart corosync. 

Once the new node has joined the cluster, you need to manually clone the new slave and set it up to replicate from whichever node is the active master.  This document will not cover the basics of cloning a slave.  Note that you will have to manually start mysql on your new node (be careful to do this exactly as pacemaker does it on the other nodes) once you have a full copy of the mysql data and before you execute your ``CHANGE MASTER ...; SLAVE START;``


to let the ``ms`` resource know that it can have another clone (slave).  You can achieve this by increasing the ``clone-max`` attribute by one.

::

   ms ms_MySQL p_mysql \
        meta master-max="1" master-node-max="1" clone-max="3" clone-node-max="1" notify="true" globally-unique="false" target-role="Master" is-managed="true"

you need to manually clone the new slave and set it up to replicate from whichever node is the active master.  This document will not cover the basics of cloning a slave.  Note that you will have to manually start mysql on your new node (be careful to do this exactly as pacemaker does it on the other nodes) once you have a full copy of the mysql data and before you execute your ``CHANGE MASTER ...; SLAVE START;``

Verify that the new node is working, replication is consistent, and allow it to catch up using standard methods.  Once it is caught up:

#. Shutdown the manually started mysql instance.  ``mysqladmin shutdown`` may be helpful here.
#. Bring the node 'online' in pacemaker.  ``crm node online new_node_name``

The trick here is that PRM will not re-issue a CHANGE MASTER if it detects that the given mysql instance was already replicating from the current master node.  Once this node is online, then it should behave as other slave nodes and failover (and possibly be promoted to the master) accordingly.

-------------
Removing node
-------------

Depending on your messaging layer, use one of the following:

Corosync:

Pacemaker post 1.1.7: http://clusterlabs.org/doc/en-US/Pacemaker/1.1/html/Pacemaker_Explained/_removing_a_corosync_node.html

Pacemaker pre 1.1.8: http://clusterlabs.org/doc/en-US/Pacemaker/1.0/html/Pacemaker_Explained/s-node-delete.html

Heartbeat: 

http://clusterlabs.org/doc/en-US/Pacemaker/1.1/html/Pacemaker_Explained/_removing_a_heartbeat_node.html

---------------
Switching roles
---------------

Switching roles in PRM cluster is a common task.  There're a simple and a complex way of achieving this.  If your goal is just to move the master role to another host, try::

    crm resource demote ms_MySQL; sleep 5;crm resource promote ms_MySQL

given the ms_MySQL is the master-slave clone set name.  That command should promote another host if, of course, another host is suitable to run as a master.  If your goal is to plan which host should be the next master, the only way is to add a location rule for the master role.  You must set the score to a high enough value to defeat the current score.  To see the current scores do::
    
    root@pacemaker-1-1:~# crm_simulate -L -s | grep promotion
    p_mysql:0 promotion score on pacemaker-1-1: 1015
    p_mysql:1 promotion score on pacemaker-1-2: 15

The meanining of "1015" is simply "1000 + max_slave_lag" (max_slave_lag = 15 in this config).  With the above values, assuming you know that pacemaker-1-2 is sane to become the master and you want to favor its promotion you have to add a score of at least 1001.  It is "at least" 1001 because you may have defined a resource stickiness settings that would also need to be added.  Just to avoid confusion, let's add a score of "1500" to locate the master on pacemker 1-2 with the following rule::

    location want_pacemaker_1_2_as_master ms_MySQL \
        rule $id="want_pacemaker_1_2_as_master-rule" $role="Master" 1500: #uname eq pacemaker-1-2
        
After the applying the rule, the master will be pacemaker-1-2 and the promotion scores will look like::

    root@pacemaker-1-1:~# crm_simulate -L -s | grep promotion
    p_mysql:1 promotion score on pacemaker-1-2: 2515
    p_mysql:0 promotion score on pacemaker-1-1: 15
    
Where this time "2515" means "1000 + 1500 + max_slave_lag".  The promotion process adds a score of 1000 for master stability reason. Once pacemaker-1-2 has been promoted, you can remove the role or simply set its score to 0 (instead of 1500).  Leaving such a rule permanently can be dangerous since it could lead to the promotion of a node that is not the best candidate for the master role.

--------------
Schema changes
--------------

Schema changes, provided that they don't break replication can easily be done with very minimal impacts on the cluster operation by following this procedure.

Slaves
------

For each of the slaves, do::

    crm node standby slave_node_name
    /etc/init.d/mysql start
    mysql -e 'start slave;alter table ....'
    
Once the alter table is complete, wait for the slave to catch up with the master then::

    /etc/init.d/mysql stop
    crm node online slave_node_name
    
The Percona server and MySQL 5.6.x dump/restore of the Innodb buffer pool feature are very useful for such setup.

Master
------

Once all the slaves are done and in sync with the master, you can repeat the same procedure with the master.  In the sequence of event that with put the master in standby, a clean demotion will occur and a new master will be promoted. 

-------
Backups
-------

Backups with a PRM cluster are very similar to backups on a regular replication cluster with some exceptions.

Cold backup
-----------

Put the node in standby first::

   crm node standby backup_node_name 

and once the backup is completed do::

    crm node online backup_node_name
    
mysqldump
---------

Nothing specific

Xtrabackup
----------

One the master, nothing specific but on the slave, if you want to include "--slave-info", you need to prevent Pacemaker to restart the slave thread when xtrabackup is running.  This is achieved using the ``backup_lockfile`` parameter which default to "/var/lock/innobackupex".  So, assuming the default value, you'll need to invoke the backup script like::

    flock -xn /var/lock/innobackupex innobackupex --safe-slave-backup /tmp/mysqlbackup
    

-----------------------------------------------
Correcting frequent reader VIP movements in VMs
-----------------------------------------------

In virtual environments, you will often find some instabilities with the reader VIPs causing rather frequent VIP movements, even on nearly idle clusters.  The source of thos VIP movements is the clock scew adjustements that causes MySQL to return for a short time an abnormally high second behind master value on a slave.  To correct this behavior, you can use the ``reader_failcount`` parameter.  This parameter forces Pacemaker to fail a number of times, depending on the setting value, because moving the VIP.  A small value, 2 or 3, is sufficient to suppress the behavior.  Make sure that the location rules you use for the reader VIPs are using "gt" and not "eq" like this::

   location loc-no-reader-vip-1 reader_vip_1 \
         rule $id="rule-no-reader-vip-1" -inf: readable lt 1
   location loc-No-reader-vip-2 reader_vip_2 \
         rule $id="rule-no-reader-vip-2" -inf: readable lt 1

Since the domain of values for readable will be in the [0, reader_failcount] interval. 
