#!/usr/bin/perl

# perl script to automate experiments
# Usage:
# perl script.pl <INPUT_FILE>
# Assumes that xsb is in the $PATH environment variable
# Assumes that startup.xwam is available in $execdir

use Cwd;

if (@ARGV != 1) {
    print "check usage\n";
    exit -1;
}

my $inputfile = $ARGV[0];
#print "inputfile: $inputfile\n";
open(my $fh, "<", $inputfile) or die "cannot open input file\n";
my $execdir, $sourcedir, $datadir;
my @experiments=();
while (my $line = <$fh>) {
    if ($line =~ /#.*/) {
	next;
    } elsif ($line =~ /^execdir=(.+)/) {
	$execdir = $1;
    } elsif ($line =~ /^sourcedir=(.+)/) {
	$sourcedir = $1;
    } elsif ($line =~ /^datadir=(.+)/) {
	$datadir = $1;
    } else {
	push(@experiments, $line);
    }
}
close($fh);
chdir($execdir);
foreach my $experiment (@experiments) {
    open (my $xsbscript, ">", "runexpmts.P");
    print $xsbscript "go1 :- \n";
    print $xsbscript "\t consult('ami'),\n";
    chomp($experiment);
    my @fields = split(/ /,$experiment);
    print $xsbscript "\t adaptation(".$fields[2]."),\n";
    print $xsbscript "\t resampling_style(".$fields[3]."),\n";
    print $xsbscript "\t load_dyn('".$sourcedir."/".$fields[0]."'),\n";
	",".$fields[7].",".$fields[8].",".$fields[9].",'".$datadir."/".$fields[1]
	."').\n";
    print $xsbscript "\t write_stats(".$fields[4].",".$fields[5].",".$fields[6].
	",".$fields[7].",".$fields[8].",".$fields[9].",'".$datadir."/".$fields[1]
	."').\n";
    close($xsbscript);
    system("xsb startup\n");
}
