#!/usr/bin/perl
# auto increment a quad-decimal version number at build time

use strict;
use POSIX qw(strftime);

my $current_version;
my $previous_version;
my $build;

my $cur_major;
my $cur_minor;
my $cur_revis;

my $prev_major;
my $prev_minor;
my $prev_revis;

open (my $vfile, "<.version");
my @version_data = <$vfile>;
close $vfile;

foreach my $version (@version_data)
{
	if ($version =~ m/^current_ver=(.*)$/)
	{
		$current_version = $1;
	}
	if ($version =~ m/^previous_ver=(.*)$/)
	{
		$previous_version = $1;
	}
	if ($version =~ m/^build=(.*)$/)
	{
		$build = $1;
	}
}

if ($current_version =~ m/^(\d+)\.(\d+)\.(\d+)$/)
{
	($cur_major, $cur_minor, $cur_revis) = ($1, $2, $3);
}

if ($previous_version =~ m/^(\d+)\.(\d+)\.(\d+)$/)
{
	($prev_major, $prev_minor, $prev_revis) = ($1, $2, $3);
}

if ($cur_major > $prev_major || $cur_minor > $prev_minor || $cur_revis > $prev_revis)
{
	print("same");
	$build=1;
} else
{
	print("increment");
	$build++;
}

my $date = strftime("%Y-%m-%d", localtime);
my $time = strftime("%H:%M:%S", localtime);

open ($vfile, ">version.h");
print $vfile <<EOT;
#ifndef VERSION_H
#define VERSION_H

#define __PLANETARY_VERSION__ "$cur_major.$cur_minor.$cur_revis.$build"
#define __BUILD_DATE__ "$date"
#define __BUILD_TIME__ "$time"

#endif // VERSION_H
EOT
close $vfile;

open ($vfile, ">.version");
print $vfile "current_ver=$cur_major.$cur_minor.$cur_revis\n";
print $vfile "previous_ver=$prev_major.$prev_minor.$prev_revis\n";
print $vfile "build=$build\n\n";
close $vfile;

