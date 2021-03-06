#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";


use CRI::Test;

plan('no_plan');
setDesc('Install (make install) BLCR');

# Variables
my $blcr_extracted_dir = $props->get_property( 'blcr.build.dir' );
my $makefile           = "$blcr_extracted_dir/Makefile";

# Make sure the extracted directory exists
ok(-e $makefile, "Checking for the BLCR Makefile at '$makefile'")
  or die "BLCR Makefile not found";

# cd to the blcr directory
ok(chdir $blcr_extracted_dir, "Changing directory to '$blcr_extracted_dir'")
  or die "Unable to switch to directory '$blcr_extracted_dir'";

# Extract the file
logMsg("Running the make install command ...\n");
my $make_cmd = "make install";
my %make     = runCommand($make_cmd);

# Check the make install results
ok($make{ 'EXIT_CODE' } == 0, "Checking the exit code for '$make_cmd'"  );
