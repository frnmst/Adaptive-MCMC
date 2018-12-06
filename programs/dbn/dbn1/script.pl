#!/usr/bin/perl

# perl script to test the particle filter implementation
my $noisy = 0;
my $datadir="";
if ($noisy) {
    $datadir = "/home/arun/svn/code/Matlab/particlefilter/dbn/noisy/";
} else {
    $datadir = "/home/arun/svn/code/Matlab/particlefilter/dbn/notnoisy/";
}
foreach my $experiment (1..100) {
    open ($obsfile, ">", "observations.P");
    open ($datafile1, "<", $datadir."ya".$experiment);
    open ($datafile2, "<", $datadir."yb".$experiment);
    open ($datafile3, "<", $datadir."yc".$experiment);
    my @lines = <$datafile1>;
    my $line = shift(@lines);
    chomp($line);
    my @ya = split(',', $line);
    close($datafile1);
    @lines = <$datafile2>;
    $line = shift(@lines);
    chomp($line);
    my @yb = split(',', $line);
    @lines = <$datafile3>;
    $line = shift(@lines);
    chomp($line);
    my @yc = split(',', $line);
    my $time = 0;
    foreach my $a (@ya) {
	my $b = shift(@yb);
	my $c = shift(@yc);
	print $obsfile "obs($time, [$a, $b, $c]).\n";
	$time = $time + 1;
    }
    close ($obsfile);
    close ($datafile1);
    close ($datafile2);
    close ($datafile3);
    open (my $distfile, ">", "exactdist.P");
    open ($datafile1, "<", $datadir."fdist".$experiment);
    @lines = <$datafile1>;
    $line = shift(@lines);
    chomp($line);
    my @d000 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d001 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d010 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d011 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d100 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d101 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d110 = split(',', $line);
    $line = shift(@lines);
    chomp($line);
    my @d111 = split(',', $line);
    close($datafile1);
    foreach $time (0..49) {
	print $distfile "exact_dist($time, [".shift(@d000).", ".
	                                      shift(@d001).", ".
					      shift(@d010).", ".
					      shift(@d011).", ".
					      shift(@d100).", ".
					      shift(@d101).", ".
					      shift(@d110).", ".
					      shift(@d111)."]).\n";
    }
    close ($distfile);

    open (my $xsbscript, ">", "runexpmts.P");
    print $xsbscript "go1 :- \n";
    print $xsbscript "\t consult('particlefilter'),\n";
    print $xsbscript "\t consult('dbn1test'),\n";
    if ($noisy) {
	print $xsbscript "\t load_dyn('../../trunk/programs/dbn/dbn1noisyparams'),\n";
    } else {
	print $xsbscript "\t load_dyn('../../trunk/programs/dbn/dbn1params'),\n";
    }
    print $xsbscript "\t load_dyn('observations'),\n";
    print $xsbscript "\t load_dyn('exactdist'),\n";
    print $xsbscript "\t numparticles(100),\n";
    print $xsbscript "\t resampling_threshold(1.0),\n";
    print $xsbscript "\t filterdist(dbn([0,0,0], 49), _).\n";
    close ($xsbscript);
    foreach (1..49) {
	print "obseq: ".$experiment." experiment: ".$_."\n";
	system("xsb startup\n");
	open (my $fh, ">>", "errors.csv");
	print $fh ",";
	close ($fh);
    }
    print "obseq: ".$experiment." experiment: 50\n";
    system("xsb startup\n");
    open (my $fh, ">>", "errors.csv");
    print $fh "\n";
    close ($fh);
}
