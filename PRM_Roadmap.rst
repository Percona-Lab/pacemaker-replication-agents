Prm roadmap

1. Start/stop unclean, master score issue (done May 2013)
   
   Basically, if master fails, give a chance to restart 
    
2. Change master score calculation for slaves

   Instead of looking for SBM, look at last published Xid of the master (attribute)
   and derive score from that. 
   
   On the master monitor runs:
   
   mysqlbinlog --start-position=676865964 db-bin.000050 | grep -B1 Xid | tail -n 2
   
   start position comes from a cache file (replication_pos) which contains
   "db-bin.000050|676865964".  If cache file is not present, --start-datetime 
   is used with the last 2 minutes, File come from show master status.
   
   The output is:
   
   # at 676992173
   #130325 18:41:39 server id 1  end_log_pos 676992200     Xid = 2057166312
   
   Master publish an attribute (reboot type), last_xid
   
   Slave performs the same thing with relaylog file.  
   
   Master_score = 100000 + (slave_xid-master_xid), the value is capped
   between 0 and 200000.  Since the master is not constantly updating the 
   cib (monitor interval),  slave can be ahead of the master value in cib.  
   The Master master_score is 300000. 
   
   Slave updates their master_score for the last time during the post-demote
   notification.  Normally, they should all be equal at that point.
   
   Things to check:
   ** mysqlbinlog must be accessible,  to be checked 
   ** Validate edge cases for new files after rotation.
   
   Question:
     1. Is there a post-demote event if the master crashes?
        - yes, if only mysqld crashed
        - no, if the host crashed.
   
   
3. MHA like behavior, requires point 2.

   case 1: master mysqld master crash and can't restart
   
        - restarted in place (see point 1.)
   
        - if restart fails  master will get a stop event.  Upon 
          unclean stop, tar last 2 files and open stream with nc (port?)
          
        - The newly promoted master during pre-promote or promote grabs 
          the last 2 master binlogs and process them, applying what's 
          missing based on it's max applied Xid.  The new master then looks 
          at its binlog/relaylog/downloaded files and create a map in a 
          table prm.binlog_recovery_index (xid,file,pos) for the last 
          transactions (1 min?)
          
        - During post-promote, slaves looks at their last xid from binlog 
          query the master for file and position based on their Xid.  With 
          that, they start replication.
          
   case 2:  Master server died 
   
        - slaves are doing monitor more frequently then token time * 3 so
          they should have at least one monitor ops after master server 
          failure.  master_score is accurate.
          
        - Slave with highest Xid (master_score) is chosen, same algo as
          above.
          
        - The newly promoted master during pre-promote or promote grabs 
          the last 2 master binlogs and process them, applying what's 
          missing based on it's max applied Xid.  The new master then looks 
          at its binlog/relaylog/downloaded files and create a map in a 
          table prm.binlog_recovery_index (xid,file,pos) for the last 
          transactions (1 min?)
          
        - During post-promote, slaves looks at their last xid from binlog 
          query the master for file and position based on their Xid.  With 
          that, they start replication.
          
          
4. Unit testing

    - likely a shell script with an option file running tests. 
    - each test as a separate script sourcing the option file
    - Called with common api (prepare, run, verify)
    
    
5. Documentation

    - Geo DR (doc + blog) (done)
    - Maintenance mode (done)
    - Schema change (done)

