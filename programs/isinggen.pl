#!/usr/bin/perl

# perl script to generate prism programs for ising models
# usage
# perl isinggen.pl <nodes> <coupling>

my $n = $ARGV[0];
my $coupling = $ARGV[1];

# first we compute the factors
my $downdown = exp($coupling);
my $downup = exp(-1 * $coupling);
my $updown = exp(-1 * $coupling);
my $upup = exp($coupling);

my $norm = $downdown + $downup + $updown + $upup;

$downdown = sprintf("%.4f", $downdown / $norm);
$downup = sprintf("%.4f", $downup / $norm);
$updown = sprintf("%.4f", $updown / $norm);
$upup = sprintf("%.4f", $upup / $norm);

open ($file, ">", "ising".$n."J".$coupling."\.P");
print $file "values(_, [[d,d],[d,u],[u,d],[u,u]])."."\n";

my $dist = "[".$downdown.",".$downup.",".$updown.",".$upup."]";
#write the set_sw facts
print $file "set_sw(_, ".$dist.")."."\n";
#now print the prism program.
print $file "\n";

print $file "world(";
for ($count = 1; $count <= $n; $count++) {
    for ($count1 = 1; $count1 <= $n; $count1++) {
	print $file "X".$count.$count1;
	if (not ($count == $n && $count1 == $n)) {
	    print $file ",";
	}
    }
}
print $file "):-\n";
for ($i = 1; $i < $n; $i++) {
    #here i represents row
    for ($j = 1; $j < $n; $j++) {
	#write the factors corresponding to the horizontal edges
	print $file "\tmsw(row".$i.$j.", [X".$i.$j.",X".$i.($j+1)."]),\n";
    }
    for ($j = 1; $j <= $n; $j++) {
	#write the factors corresponding to the vertical edges
	print $file "\tmsw(col".$i.$j.", [X".$i.$j.",X".($i+1).$j."]),\n";
    }
}
#print the last row
for ($i = 1; $i < $n-1; $i++) {
    print $file "\tmsw(row".$n.$i.", [X".$n.$i.",X".$n.($i+1)."]),\n";
}
print $file "\tmsw(row".$n.($n-1).", [X".$n.($n-1).",X".$n.$n."]).\n";
close($file);
