#!/usr/bin/perl

# perl script to plot statistics produced by samplers

# usage

# to view the plots without saving them to files
# perl plot.pl view <title> <INFILE1> <LEGEND1> [<INFILE2>] [<LEGEND2]...
# example: perl plot.pl view fire-alarm fire-alarm1.csv single-off fire-alarm2.csv single-on

# to save the plots to files
# perl plot.pl save <title> <INFILE1> <LENGEND1> [<INFILE2>] [<LEGEND2>]...

# by default the eps files will be named "computedanswer.eps", "rejections.eps"
# "time.eps" and "variance.eps"

if (@ARGV < 4 or ($ARGV[0] ne "view" and $ARGV[0] ne "save")) {
    print "check usage\n";
    exit -1;
}

my $sv = shift(@ARGV);
my $title = shift(@ARGV);

open(my $gnuplot, "|-", "gnuplot") or die("cannot run gnuplot\n");
print $gnuplot "set datafile separator ','\n";
print $gnuplot "set autoscale\n";
print $gnuplot "set xtic auto\n";
print $gnuplot "set ytic auto\n";
print $gnuplot "set title '".$title."'\n";
print $gnuplot "set xlabel 'samples'\n";

#plot computed answer
print $gnuplot "set ylabel 'computed answer'\n";
if ($sv eq "view") {
    print $gnuplot "set terminal x11 1 persist\n";
}else{
    print $gnuplot "set terminal postscript eps color\n";
    print $gnuplot "set output 'computedanswer.eps'\n";
}
my $str = gencommand(@ARGV, 2);
chop($str);
print $gnuplot $str."\n";

#plot time
print $gnuplot "set ylabel 'time'\n";
if ($sv eq "view") {
    print $gnuplot "set terminal x11 2 persist\n";
}else{
    print $gnuplot "set terminal postscript eps color\n";
    print $gnuplot "set output 'time.eps'\n";
}
my $str = gencommand(@ARGV, 4);
chop($str);
print $gnuplot $str."\n";

#plot rejections
print $gnuplot "set ylabel 'rejections'\n";
if ($sv eq "view") {
    print $gnuplot "set terminal x11 3 persist\n";
}else{
    print $gnuplot "set terminal postscript eps color\n";
    print $gnuplot "set output 'rejections.eps'\n";
}
my $str = gencommand(@ARGV, 5);
chop($str);
print $gnuplot $str."\n";

close($gnuplot);

sub gencommand {
    my $column = pop(@_);
#    my @array = @_;
    my $command = "plot ";
    while (scalar(@_) > 0) {
	$command .= "'".shift(@_);
	$command .= "' using 1:".$column." title '".shift(@_).
	    "' with linespoints,";
    }
    return $command;
}
