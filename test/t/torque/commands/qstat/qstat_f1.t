#!/usr//bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


# Test Modules
use CRI::Test;


use Torque::Job::Ctrl          qw(
                                   submitSleepJob
                                   runJobs
                                   delJobs 
                                 );
use Torque::Util        qw(
                                   run_and_check_cmd
                                   job_info
                                 );
use Torque::Util::Qstat qw(
                                   parse_qstat_f1
                                 );


# Test Description
plan('no_plan');
setDesc("qstat -f -1");

# Variables
my $cmd;
my %qstat;
my @job_ids;
my %job_info;
my $job_params;

my $user       = $props->get_property( 'torque.user.one' );
my $torque_bin = $props->get_property( 'Torque.Home.Dir' ) . '/bin/';

my @attributes = qw(
                     Job_Name
                     Job_Owner
                     job_state
                     queue
                     server
                     Checkpoint
                     ctime
                     Error_Path
                     Hold_Types
                     Join_Path
                     Keep_Files
                     Mail_Points
                     mtime
                     Output_Path
                     Priority
                     qtime
                     Rerunable
                     Resource_List.neednodes
                     Resource_List.nodect
                     Resource_List.nodes
                     Resource_List.walltime
                     substate
                     Variable_List
                     euser
                     egroup
                     queue_rank
                     queue_type
                     etime
                   );

# Submit a job
$job_params = {
                'user'       => $user,
                'torque_bin' => $torque_bin
              };

# submit a couple of jobs
push(@job_ids, submitSleepJob($job_params));
push(@job_ids, submitSleepJob($job_params));

# Test qstat
$cmd   = "qstat -f -1";
%qstat = run_and_check_cmd($cmd);

%job_info = parse_qstat_f1( $qstat{ 'STDOUT' } );

foreach my $job_id (@job_ids)
  {

  my $msg = "Checking job '$job_id'";
  diag($msg);
  logMsg($msg);

  foreach my $attribute (@attributes)
    {

    ok(defined $job_info{ $job_id }{ $attribute }, "Checking for '$job_id' $attribute attribute"); 

    } # END foreach my $attribute (@attributes)

  } # END foreach my $job_id (@job_ids)

# Delete the job
delJobs(@job_ids);
