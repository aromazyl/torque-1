#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


use CRI::Test;
use Torque::Ctrl        qw( startTorque 
                            stopTorque 
                            stopPbssched
                          );
use Torque::Util qw( 
                            run_and_check_cmd
                            list2array             
                          );

# Describe Test
plan('no_plan');
setDesc('Qterm Setup');

# Torque params
my @remote_moms    = list2array($props->get_property('Torque.Remote.Nodes'));
my $torque_params  = {
                     'remote_moms' => \@remote_moms
                     };

###############################################################################
# Stop Torque
###############################################################################
stopTorque($torque_params) 
  or die 'Unable to stop Torque';

###############################################################################
# Stop pbs_sched
###############################################################################
stopPbssched() 
  or die 'Unable to stop pbs_sched';

###############################################################################
# Restart Torque
###############################################################################
startTorque($torque_params)
  or die 'Unable to start Torque';

###############################################################################
# Stop all jobs
###############################################################################
run_and_check_cmd('qdel -p all');
