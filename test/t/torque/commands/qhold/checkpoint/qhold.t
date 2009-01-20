#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../../lib/";


# Test Modules
use CRI::Test;
use Torque::Job::Ctrl          qw( 
                                    submitCheckpointJob
                                    runJobs
                                    delJobs
                                 );
use Torque::Test::Utils        qw( 
                                    run_and_check_cmd
                                 );
use Torque::Test::Qhold::Utils qw(
                                    verify_qhold_chkpt
                                 );


# Test Description
plan('no_plan');
setDesc("qhold (checkpointable)");

# Variables
my $params;
my $job_id;
my $cmd;
my %qhold;

###############################################################################
# Test qhold when the job is checkpointable
###############################################################################

# Submit a job
$params = {
            'user'       => $props->get_property('torque.user.one'),
            'torque_bin' => $props->get_property('torque.home.dir') . 'bin/',
            'app'        => $props->get_property('test.base') . 'torque/test_programs/test.pl'
          };

$job_id = submitCheckpointJob($params);

# Run the job
runJobs($job_id);

# Test qhold
$cmd     = "qhold $job_id";
%qhold   = run_and_check_cmd($cmd);

# Verify that the job was held correctly
verify_qhold_chkpt($job_id);

# Delete the job
delJobs($job_id);
