#!/usr/bin/perl

##------------------------------------------------------------------------------
## Modified from sdp_ping.pl
##------------------------------------------------------------------------------


use strict;
use warnings;

use SpiNN::SCP;


my $debug = 4;		# Debug level (3 or 4)

my $spin;		# SpiNNaker handle
my $port;		# SpiNNaker app. port

sub process_args
{
    die "usage: ./test_supv <hostname> <chipX> <chipY> <CPU> <port>\n" . 
	"where:\n-port=1 for printReport\n-port=2 for testing FR communication with stub\n" .
        "-port=3 for...\n" unless
	$#ARGV == 4 &&	$ARGV[1] =~ /^\d+$/ &&	$ARGV[2] =~ /^\d+$/ &&	$ARGV[3] =~ /^\d+$/ &&	$ARGV[4] =~ /^\d+$/;

    $spin = SpiNN::SCP->new (target => $ARGV[0]);
    die "Failed to connect to $ARGV[0]\n" unless $spin;

    $spin->addr ($ARGV[1], $ARGV[2], $ARGV[3]);
    $port = $ARGV[4];
}

sub main
{
    process_args ();
    my $pad = pack "V4", 0, 0, 0, 0;
	$spin->send_sdp ($pad, port => $port, debug => $debug);
}

main ();

#------------------------------------------------------------------------------
