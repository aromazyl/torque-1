#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";


use CRI::Test;

plan('no_plan');
setDesc("FULL Torque HA Regression Tests");

my $testbase = $FindBin::Bin;


execute_tests(
#    "$testbase/fail_over.bat",
);
