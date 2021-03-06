#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


# Test Modules
use CRI::Test;
use Torque::Util           qw( 
                               list2array
                               run_and_check_cmd 
                             );
use Torque::Util::Pbsnodes qw( 
                               parse_list_output      
                             );

# Test Description
plan('no_plan');
setDesc('pbsnodes -s $server -s <SERVER> -l');

# Variables
my $cmd;
my $status_update_time = $props->get_property('mom.config.status_update_time');
my $nodes_str          = $props->get_property('Test.Host');
my $server             = $props->get_property('Test.Host');
my @nodes              = list2array($nodes_str);
my %pbsnodes;
my %node_states;

# Set the nodes to offline
$cmd      = "pbsnodes -s $server -o $nodes_str";
%pbsnodes = run_and_check_cmd($cmd);

# Test pbsnodes -s $server -l for the offline status
$cmd      = "pbsnodes -s $server -l";
%pbsnodes = run_and_check_cmd($cmd);
%node_states = parse_list_output($pbsnodes{ 'STDOUT' });

foreach my $node (@nodes)
  {

  ok($node_states{ $node } =~ /offline/i, "Checking for the offline state for '$node'");

  } # END foreach my $node (@nodes)

# Test pbsnodes -s $server -l offline for the offline status
$cmd      = "pbsnodes -s $server -l offline";
%pbsnodes = run_and_check_cmd($cmd);
%node_states = parse_list_output($pbsnodes{ 'STDOUT' });

foreach my $node (@nodes)
  {

  ok($node_states{ $node } =~ /offline/i, "Checking for the offline state for '$node'");

  } # END foreach my $node (@nodes)

# Force pbs_mom to cycle so we have time to do our tests
logMsg('Force pbs_mom to cycle so we have time to do our tests');
$cmd = "momctl -C";
%pbsnodes = run_and_check_cmd($cmd);

# Reset the nodes so they appear in the down state
$cmd = "pbsnodes -s $server -r $nodes_str";
%pbsnodes = run_and_check_cmd($cmd);

# Test pbsnodes -s $server -l for the down status
$cmd      = "pbsnodes -s $server -l";
%pbsnodes = run_and_check_cmd($cmd);
%node_states = parse_list_output($pbsnodes{ 'STDOUT' });

foreach my $node (@nodes)
  {

  ok($node_states{ $node } =~ /down/i, "Checking for the down state for '$node'");

  } # END foreach my $node (@nodes)

# Test pbsnodes -s $server -l down for the down status
$cmd      = "pbsnodes -s $server -l down";
%pbsnodes = run_and_check_cmd($cmd);
%node_states = parse_list_output($pbsnodes{ 'STDOUT' });

foreach my $node (@nodes)
  {

  ok($node_states{ $node } =~ /down/i, "Checking for the down state for '$node'");

  } # END foreach my $node (@nodes)

# Wait for pbsmom to update it's status
my $sleeptime = $status_update_time * 2;
diag("Sleeping for $sleeptime");
sleep($sleeptime);

# Test pbsnodes -s $server -l for no status
$cmd      = "pbsnodes -s $server -l";
%pbsnodes = run_and_check_cmd($cmd);
%node_states = parse_list_output($pbsnodes{ 'STDOUT' });

foreach my $node (@nodes)
  {

  ok(! defined $node_states{ $node }, "Checking for no status for '$node'");

  } # END foreach my $node (@nodes)
