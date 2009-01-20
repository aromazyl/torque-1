#!/usr/bin/perl

use CRI::Test;

plan('no_plan');
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../../lib/";


my $testbase=$props->get_property('test.base') . "torque/commands/qhold/checkpoint";
setDesc("RELEASE qhold checkpoint tests");

execute_tests(
              "$testbase/qhold.t",
              "$testbase/qhold_h_u.t",
              "$testbase/qhold_h_o.t",
              "$testbase/qhold_h_s.t",
              "$testbase/qhold_h_sou.t",
);
