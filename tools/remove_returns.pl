#!/usr/bin/env perl
use strict;
use warnings;

# Remove 'return <expr>' lines from void @test functions
# Keeps the expression if it has side effects (function calls, assignments)

my $in_test = 0;
my $brace_depth = 0;

while (<>) {
    chomp;
    
    # Detect @test function
    if (/^\@test/) {
        $in_test = 1;
        print "$_\n";
        next;
    }
    
    if ($in_test && /^func\s+\w+\s*\(.*?\)\s*:\s*void/) {
        $brace_depth = 0;
        print "$_\n";
        next;
    }
    
    if ($in_test) {
        # Track brace depth
        for my $ch (split //, $_) {
            $brace_depth++ if $ch eq '{';
            $brace_depth-- if $ch eq '}';
        }
        
        # Remove return statements at top level of function
        if ($brace_depth == 1 && /^\s*return\s+(.*?)\s*$/) {
            my $expr = $1;
            # If it's a simple constant, just remove the line
            if ($expr =~ /^\d+$/ || $expr =~ /^0x[0-9a-fA-F]+$/) {
                # Skip - don't print
                next;
            }
            # If it has function calls or assignments, keep as statement
            elsif ($expr =~ /\(/ || $expr =~ /=/) {
                my $indent = $_;
                $indent =~ s/\S.*//;
                print "${indent}${expr}\n";
                next;
            }
            # Variable or expression - convert to assertEquals
            else {
                my $indent = $_;
                $indent =~ s/\S.*//;
                print "${indent}assertEquals(0, ${expr}, \"${ARGV}:\")\n";
                next;
            }
        }
        
        # Exit test function when brace depth returns to 0
        if ($brace_depth <= 0 && /^\s*\}/) {
            $in_test = 0;
        }
    }
    
    print "$_\n";
}
