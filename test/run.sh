#!/bin/sh
F=/ff/ffmpeg
P=/ff/ffprobe
T=$(mktemp -d)
pass=0
fail=0
report() { if [ "$1" = 0 ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: $2"; fi; }
run() { name=$1; shift; "$@" >"$T/log" 2>&1; rc=$?; [ $rc -ne 0 ] && tail -3 "$T/log"; report $rc "$name"; }

. /etc/os-release 2>/dev/null
echo "== $PRETTY_NAME ($(uname -m)) glibc $(ldd --version 2>&1 | head -1 | grep -oE '[0-9]+\.[0-9]+$')"
run "ffmpeg starts" $F -hide_banner -version
run "ffprobe starts" $P -hide_banner -version

$F -hide_banner -loglevel error -f lavfi -i testsrc2=size=321x241:rate=25:duration=2 -f lavfi -i sine=frequency=440:duration=2 \
   -c:v ffv1 -c:a pcm_s16le -shortest "$T/src.mkv" || echo "FAIL: source"
$F -hide_banner -loglevel error -f lavfi -i testsrc2=size=320x240:duration=1 -frames:v 1 "$T/src.png" || echo "FAIL: still"
$F -hide_banner -loglevel error -f lavfi -i sine=frequency=440:duration=2 "$T/src.wav" || echo "FAIL: audio"

V="-map 0:v:0 -map 0:a:0? -vf crop=trunc(iw/2)*2:trunc(ih/2)*2:0:0"
video() { out=$1; shift; run "video -> $out" $F -hide_banner -loglevel error -y -i "$T/src.mkv" "$@" "$T/out.$out"; run "probe $out" $P -v error "$T/out.$out"; }
video mp4 $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart
video mov $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart
video mkv $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k
video flv $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k
video ts $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k
video 3gp $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a aac -b:a 192k -ac 2 -movflags +faststart
video webm -map 0:v:0 -map 0:a:0? -c:v libvpx-vp9 -crf 31 -b:v 0 -row-mt 1 -pix_fmt yuv420p -c:a libopus -b:a 192k
video avi -map 0:v:0 -map 0:a:0? -c:v mpeg4 -q:v 4 -pix_fmt yuv420p -c:a libmp3lame -b:a 192k
video mpg -map 0:v:0 -map 0:a:0? -c:v mpeg2video -b:v 5M -pix_fmt yuv420p -c:a mp2 -b:a 192k
video vob -map 0:v:0 -map 0:a:0? -c:v mpeg2video -b:v 5M -pix_fmt yuv420p -c:a ac3 -b:a 192k -ar 48000 -f vob
video wmv $V -c:v wmv2 -b:v 5M -pix_fmt yuv420p -c:a wmav2 -b:a 192k
video ogv -map 0:v:0 -map 0:a:0? -c:v libvpx -b:v 2M -c:a libopus -f ogg
video m2ts $V -c:v libopenh264 -b:v 5M -pix_fmt yuv420p -c:a ac3 -f mpegts
video av1.mkv $V -c:v libaom-av1 -cpu-used 8 -b:v 1M -pix_fmt yuv420p -an
video svt.mkv $V -c:v libsvtav1 -preset 12 -pix_fmt yuv420p -an
video gif -an -filter_complex "[0:v]fps=10,scale='min(480,iw)':-1:flags=lanczos,split[a][b];[b]palettegen=stats_mode=diff[p];[a][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle"
video webp -map 0:v:0 -frames:v 1 -update 1 -c:v libwebp -quality 90
video png -map 0:v:0 -frames:v 1 -update 1
video jpg -map 0:v:0 -frames:v 1 -update 1

audio() { out=$1; shift; run "audio -> $out" $F -hide_banner -loglevel error -y -i "$T/src.wav" "$@" "$T/a.$out"; run "probe $out" $P -v error "$T/a.$out"; }
audio mp3 -c:a libmp3lame -b:a 192k
audio flac
audio ogg -c:a libopus
audio opus -c:a libopus
audio m4a -c:a aac
audio aac -c:a aac
audio ac3 -c:a ac3
audio mp2 -c:a mp2
audio aiff
audio wma -c:a wmav2
audio mka -c:a libopus
audio amr -c:a libopencore_amrnb -ar 8000 -ac 1 -b:a 12.2k
audio wb.amr -c:a libvo_amrwbenc -ar 16000 -ac 1
audio spx.ogg -c:a libspeex -ar 16000
audio caf
audio wv -c:a wavpack

run "decode h264 back" $F -hide_banner -loglevel error -i "$T/out.mp4" -f null -
run "decode vp9 back" $F -hide_banner -loglevel error -i "$T/out.webm" -f null -
run "decode av1 back" $F -hide_banner -loglevel error -i "$T/out.av1.mkv" -f null -
run "zimg zscale" $F -hide_banner -loglevel error -i "$T/src.png" -vf zscale=w=160:h=120 -f null -

graceful() {
    name=$1; shift
    "$@" >"$T/log" 2>&1
    rc=$?
    case $rc in
        132|134|135|136|137|139) echo "FAIL: $name crashed (exit $rc)"; tail -2 "$T/log"; fail=$((fail+1)) ;;
        *) pass=$((pass+1)); echo "ok: $name -> $(grep -v '^ *$' "$T/log" | tail -1)" ;;
    esac
}
graceful "vaapi without hardware" $F -hide_banner -init_hw_device vaapi=va -f lavfi -i testsrc -frames 1 -f null -
graceful "qsv encoder without hardware" $F -hide_banner -f lavfi -i testsrc -frames 1 -c:v h264_qsv -f null -
graceful "nvenc encoder without hardware" $F -hide_banner -f lavfi -i testsrc -frames 1 -c:v h264_nvenc -f null -
graceful "v4l2request without hardware" $F -hide_banner -init_hw_device v4l2request -f lavfi -i testsrc -frames 1 -f null -
graceful "vulkan without hardware" $F -hide_banner -init_hw_device vulkan -f lavfi -i testsrc -frames 1 -f null -
graceful "pulse without server" $F -hide_banner -f pulse -i default -t 0.1 -f null -
graceful "alsa without device" $F -hide_banner -f alsa -i default -t 0.1 -f null -
graceful "x11grab without display" $F -hide_banner -f x11grab -i :0 -t 0.1 -f null -
graceful "kmsgrab without device" $F -hide_banner -f kmsgrab -i - -t 0.1 -f null -

echo "RESULT: $pass passed, $fail failed"
rm -rf "$T"
[ $fail -eq 0 ]
