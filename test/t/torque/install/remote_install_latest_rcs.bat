#!/usr/bin/perl 

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";

use CRI::Test;
plan('no_plan');
setDesc('Install Latest Torque from subversion to use BLCR checkpointing on a remote node');

my $testbase = $FindBin::Bin;

execute_tests(
"$testbase/configure.t",
"$testbase/make_clean.t",
"$testbase/make.t",
"$testbase/make_install.t",
"$testbase/setup.t",
"$testbase/config_mom.t",
"$testbase/cp_chkpt_scripts.t",
"$testbase/create_torque_conf.t",
"$testbase/check_blcr.t",
);
