#!/usr/bin/perl

use CRI::Test;

plan('no_plan');
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


my $testbase = $FindBin::Bin;

setDesc("RELEASE qmgr Compatibility Tests");

execute_tests("$testbase/setup.t") 
  or die("Couldn't setup for qmgr tests!");

execute_tests(
              "$testbase/qmgr_c_scheduling.t",
              "$testbase/qmgr_c_queue.t",
);

execute_tests("$testbase/cleanup.t"); 
