#!/usr/bin/perl -w
#
# Authors:
#   Andreas Simon (2000)
#
# Copyright:
#   Andreas Simon (2000)
#
# Last change:
#   $Date$ by $Author$
#   $Revision$
#
# This file is part of Mozart, an implementation
# of Oz 3:
#   http://www.mozart-oz.org
#
# See the file "LICENSE" or
#   http://www.mozart-oz.org/LICENSE.html
# for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL
# WARRANTIES.
#

use Getopt::Long;

sub remove_comments {
    s/\/\*[^\/]*\*\///g;
}

# transforms the C enumeration names
# GDK_OVERLAP_RECTANGLE_PART gets ConstOverlapRectanglePart
sub c2oz($) {
  my ($name) = @_;

  $name = lc $name;
  $name =~ s/^[a-z]+_//s; # remove prefix
  my @substrings = split /_/, $name;
  foreach my $string (@substrings) {
    $string = ucfirst $string;
  }
  $name = join '', @substrings;
  
  return 'Const' . $name;
}

sub enum_to_hash($$) {
    # @_[0] is the name of the enumeration
    # @_[1] is the body of a enumeration
    # output is a hash of the key/value pairs

    my($enum_name, $body) = @_;
    my $i;
    my $name;
    my $value;
    my @values;
    my %hash;

    $hash{"enum"} = $enum_name;

    $body =~ s/[ \t\r\f]//g;   # remove spaces
    $body =~ s/,$//gm;         # remove colons
    $body =~ s/^\n//mg;        # remove blank lines

    $i = 0;
    @values = split /\n/, $body;
    foreach $name (@values) {
        next if $name =~ m/\#/g; # we have no enumeration
	if($name =~ m/=/g) {
	    $value = $name;
	    $value =~ s/^(\w+)=\s*//g; # cut name, leave only value
	    # compute the right value
	    if ($value =~ m/1<<(\d+)/) {                    # 1 << 4
		$value = 2 ** $1;
	    } elsif ($value =~ m/\'(.)\'/) {                # character 'x'
		$value = ord $1;
	    } elsif ($value =~ m/[\s\(]*(\w+)\s*\|\s*(\w+)/s) {    # value | other_value
		$value = $hash{$1} | $hash{$2};
	    } elsif ($value =~ m/(\d+)/) {                  # numeral
                $value = $1;
                $i = $value;
            } elsif ($value =~ m/(\w+)/) {                  # other_value
                $value = c2oz($1);
            }
	} else {
	    $value = $i ;
	}
	$name  =~ s/=.*$//g;  # remove asignments
	$name  =~ s/[ \t]//g; # remove unnessesary spaces

	$hash{$name} = $value;
	$i++;
    }
    return \%hash;
}

sub declarations(\%) {
    my ($hash)    = @_;
    my $oz_phrase = "\% enum " . $$hash{enum} . "\n";
    my $name;

    foreach $name (keys %$hash) {
	# just discard the values
	$oz_phrase = $oz_phrase . c2oz($name) . "\n" if $name !~ m/enum/;
    }
    return "$oz_phrase\n";
}

sub definitions(\%) {
    my ($hash)    = @_;
    my $oz_phrase = "\% enum " . $$hash{enum} . "\n";
    my $name;

    foreach $name (keys %$hash) {
	$oz_phrase = $oz_phrase . c2oz($name) . " = $$hash{$name}\n" if $name !~ m/enum/;
    }
    return "$oz_phrase\n";
}

sub usage() {
    print <<EOF;
usage: $0 OPTION HEADER_FILE ...

Generate constants definitions or declarations for the GTK+ binding for Oz

  --declarations    generate the constants declarations for the Oz functor
  --definitions     generate the constants definitions for the Oz functor
EOF

    exit 0;
}

my ($opt_defs, $opt_decs);
&GetOptions("declarations" => \$opt_decs,
	    "definitions"  => \$opt_defs);

@headers = @ARGV;

usage() unless @headers != 0;
usage() unless $opt_defs or $opt_decs;

print<<EOF;
%
%   THIS FILE IS AUTOMATICALLY GENERATED. PLEASE DO NOT EDIT!
%

EOF

foreach $header (@headers) {
    open(HEADER, $header) || warn "Could not open header file '$header'. $!\n";
    $/ = ";"; # field separator for .h files is semicolon
    while (<HEADER>) {
	&remove_comments;
	if (/(typedef(\s)+enum(\s)*\{)((\s*[^\n]*\n)+)\}([ \t\r\f]*)(\w*)/) { # select enums
	    $key_values = enum_to_hash($7, $4);
	    print definitions(%$key_values)  if $opt_defs;
	    print declarations(%$key_values) if $opt_decs;
	}
    }
    close HEADER;
}
