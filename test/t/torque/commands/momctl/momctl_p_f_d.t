#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


# Test Module
use CRI::Test;
use Torque::Util::Momctl qw(test_mult_diag);

# Test Case Description
plan('no_plan');
setDesc('Momctl -p <PORT> -f <HOSTLISTFILE> -d <LOGLEVEL>');

# Other Variables
my %momctl;
my $mom_port           = $props->get_property('mom.host.port');
my $mom_host_list_file = $props->get_property('mom.host.list.file');
my @mom_hosts          = split(/,|\s/, `cat $mom_host_list_file`);

# Test momctl -p $mom_port -f $mom_host_list_file -d 3
%momctl = runCommand("momctl -p $mom_port -f $mom_host_list_file -d 3");
ok($momctl{ 'EXIT_CODE' } == 0,
   "Checking that 'momctl -p $mom_port -f $mom_host_list_file -d 3' ran")
  or die "Couldn't run momctl -d";

test_mult_diag(3, $momctl{ 'STDOUT' }, \@mom_hosts);

# Test momctl -p $mom_port -f $mom_host_list_file -d 2
%momctl = runCommand("momctl -p $mom_port -f $mom_host_list_file -d 2");
ok($momctl{ 'EXIT_CODE' } == 0,
   "Checking that 'momctl -p $mom_port -f $mom_host_list_file -d 2' ran")
  or die "Couldn't run momctl -d";

test_mult_diag(2, $momctl{ 'STDOUT' }, \@mom_hosts);

# Test momctl -p $mom_port -f $mom_host_list_file -d 1
%momctl = runCommand("momctl -p $mom_port -f $mom_host_list_file -d 1");
ok($momctl{ 'EXIT_CODE' } == 0,
   "Checking that 'momctl -p $mom_port -f $mom_host_list_file -d 1' ran")
  or die "Couldn't run momctl -d";

test_mult_diag(1, $momctl{ 'STDOUT' }, \@mom_hosts);

# Test momctl -p $mom_port -f $mom_host_list_file -d 0
%momctl = runCommand("momctl -p $mom_port -f $mom_host_list_file -d 0");
ok($momctl{ 'EXIT_CODE' } == 0,
   "Checking that 'momctl -p $mom_port -f $mom_host_list_file -d 0' ran")
  or die "Couldn't run momctl -d";

test_mult_diag(0, $momctl{ 'STDOUT' }, \@mom_hosts);
