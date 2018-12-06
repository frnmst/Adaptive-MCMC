## perl script to generate source files for graph reachability
# usage: $perl gensource.pl <NUM_NODES>
# Pita source code is written to a file named reach<n>dpita.cpl
# PRISM source code is written to a file named reach<n>dprism.P
my $n = $ARGV[0];
open $filepita, ">", "reach".$n."dpita.cpl";
open $fileprism, ">", "reach".$n."dprism.P";

print $filepita "reach(X, X).\n";
print $filepita "reach(X, Y) :- edge(X, Y).\n";
print $filepita "reach(X, Y) :- edge(X, Z), reach(Z, Y).\n";

print $fileprism "reach(X, X).\n";
print $fileprism "reach(X, Y) :- edge(X, Y).\n";
print $fileprism "reach(X, Y) :- edge(X, Z), reach(Z, Y).\n";
print $fileprism "edge(X, Y) :- edge1(X, Y), msw(coin(X, Y), heads).\n";
print $fileprism "values(coin(_, _), [heads, tails]).\n";
print $fileprism "set_sw(coin(_, _), [0.5, 0.5]).\n";

# probabilistic facts based on size of the graph

# construct a 'cycle' with last edge in reversed to preserve acyclicity
# nodes are numbered 1 through N
my $i = 1;
while ($i < $n) {
	$j = $i + 1;
	print $filepita "edge(".$i.", ".$j."): 0.5.\n";
        print $fileprism "edge1(".$i.", ".$j.").\n";
	$i++;
}
# last edge in the reverse direction
print $filepita "edge(1, " .$i. "): 0.5.\n";
print $fileprism "edge1(1, " .$i. ").\n";

# add N random chords while preserving acyclicity 
# ensured by making chords go from lower numbered nodes to higher ones
$i = 1;
while ($i < $n) {
        # 'r' ranges between 1 and n-i
	my $r = 1 + int(rand($n-$i));
	my $end = $i + $r;
        # we don't want to repeat the edges in the cycle
        if ($end == $i + 1) {
            $i++;
            next;
        }
	my $r1 = rand();
        # add chords with probability, this ensures O(N) chords
	if ($r1 > 0.5) {
		print $filepita "edge(".$i.", ".$end."): 0.5.\n";
                print $fileprism "edge1(".$i.", ".$end.").\n";
	}
	$i++;
}
close($filepita);
