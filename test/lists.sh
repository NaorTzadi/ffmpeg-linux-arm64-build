#!/bin/sh
F=$1; O=$2; mkdir -p $O
for k in encoders decoders filters muxers demuxers protocols bsfs hwaccels pix_fmts codecs devices; do
  $F -hide_banner -$k 2>/dev/null > $O/$k.txt
done
$F -hide_banner -version > $O/version.txt 2>&1
