==============================================
Preparing for Passwordless Non-root SSH Access
==============================================

The Percona Replication Manager agent uses SSH to query remove CIB to determine replication and cluster resources information between DCs. Some company security policy might not allow ``root`` key based SSH access between the cluster nodes, we will use a separate less privileged SSH account with a list of specific sudo commands to execute.

For our procedure below, assume we have 3 sites with 4 nodes outlined below:

+------------+----------------+-----------------+-------------+
| Node       | Service IP     | Booth VIP       | Site        |
+============+================+=================+=============+
| node01     | 192.168.8.5    | 192.168.8.10    | Primary     |
+------------+----------------+                 |             |
| node02     | 192.168.8.6    |                 |             |
+------------+----------------+-----------------+-------------+
| node03     | 192.168.8.7    | 192.168.8.11    | Secondary   |
+------------+----------------+-----------------+-------------+
| node04     | 192.168.8.8    | 192.168.8.12    | Arbitrator  |
+------------+----------------+-----------------+-------------+

#. Create an account on ALL MYSQL nodes first, for example, we will create ``prmagent`` SSH account on ``node01``. ::

    useradd prmagent
    passwd prmagent
    usermod -aG haclient prmagent

#. To be able to execute the required cluster commands, we need to add those commands to ``/etc/sudoers.d/prm`` file. We also need to do this on all MySQL nodes: ::

    prmagent        ALL=(root)      NOPASSWD: /usr/sbin/crm_attribute, /usr/sbin/crm_resource

#. Next we create our key-pair for ``prmagent`` account: ::

    su prmagent
    ssh-keygen -t rsa
    cat ~/.ssh/id_rsa.pub > ~/.ssh/authorized_keys
    chmod 0600 .ssh/authorized_keys
    exit

#. Now we need to populate the root account's ``~/.ssh/known_hosts`` file so future logins will not prompt to accept host key verification prompt. We need to do this only from the first host just so we get a full list of the host keys::

    su - root
    for h in node01 node02 node03: do \
        ssh $h hostname; \
    done

#. After the previous step, you should have an ``known_hosts`` file like below. If you see duplicates, i.e. a host is also defined with the same key you can merge them: ::

    [root@node01 ~]$ cat .ssh/known_hosts
    node01 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAn7l1r[...]
    node02 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEA9CbwM[...]
    node03 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAtbul[...]

#. We also need to add the ``booth`` VIPs and service IPs to the ``.ssh/known_hosts`` file. Since VIPs can be assigned to any node on the sites, we will add these VIPs as alias of actual node IPs. For example, the ``booth`` VIP for our primary site is ``192.168.8.5``, and this can be assigned to any nodes between ``node01`` and ``node02``. At the end of this step, you should have a file that looks something like this - all 3 lines represent the 3 MySQL database nodes on our cluster: ::

    [root@node01 ~]$ cat .ssh/known_hosts
    node01,192.168.8.5,192.168.8.10 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAn7l1r[...]
    node02,192.168.8.6,192.168.8.10 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEA9CbwM[...]
    node03,192.168.8.7,192.168.8.11 ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAtbul[...]


#. Now that our skeleton ``.ssh`` directory is ready, we need to copy it to the other nodes: ::

    sudo su - prmagent
    for h in node02 node03: do \
        scp -r ~/.ssh $h:~/; \
    done

#. Similarly, make sure root's known_hosts file above is also copied to the 2 other nodes!
#. Now you can test with the following: ::

    # First with the service IPs, there should be no prompts 
    # for password or host key verification
    for h in 5 6 7: do \
        ssh -i /home/prmagent/.ssh/id_rsa prmagent@192.168.8.$h hostname; \
    done

    # Then for the VIPs, for example we will test with the primary
    # booth IP. First assign the VIP to the first node, node01
    ip ad add 192.168.8.5/32 dev eth1
    /sbin/arping -c 5 -U -A -I eth1 192.168.8.5
    ssh -i /home/prmagent/.ssh/id_rsa prmagent@192.168.8.5 hostname
    # Again, there should be no prompts for password or host key
    # Then remove the IP again
    ip ad delete 192.168.8.5/32 dev bond1
    # Do the same tests on the second node on the primary site
    # For the secondary, test with the other VIP 192.168.8.6
