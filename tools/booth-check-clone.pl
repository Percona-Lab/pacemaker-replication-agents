#!/usr/bin/perl -w
#
# This script can be used as 'before-acquire-handler' in booth
# This will prevent a ticket to be renewed or acquired when
# the specified ms clone set does not have active nodes.
#

use XML::XPath;
use XML::XPath::XMLParser;
use strict;

my $clonename=$ARGV[0] or die("Usage: $0 clonename [debug]\nEXIT: 255\n");
my $debug=(($ARGV[1] || "") eq "debug" ? 1 : 0);

print "Clonename: $clonename\n" if $debug;
print "Debug: Enabled\n" if $debug;

my $xml = qx(crm_mon -Ar -1 -Xr);
print "`crm_mon -Ar -1 -Xr`:\n" . $xml . "\n" if $debug;

my $xp = XML::XPath->new(xml => $xml);
my $nodeset = $xp->find('/crm_mon/resources/clone[@id="' . $clonename . '"][@failed="false"]/resource[@active="true"][@failed="false"]');
my $matches=0;

foreach my $node ($nodeset->get_nodelist) {
	$matches++;	
	print "Found Match: ". XML::XPath::XMLParser::as_string($node) . "\n" if $debug;
}

if ( $matches > 0 ) {
	print "Found $matches matches\n" if $debug;
	print "EXIT: 0\n" if $debug;
	exit 0;
} else {
	print "ERROR: Wasn't able to find any active resource for clone $clonename\n";
	print "EXIT: 1\n";
	exit 1;
}

