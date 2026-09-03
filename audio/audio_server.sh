
mkfifo /tmp/audio.pipe
./fcntl &
# while true; do
    pacat --latency-msec=50 --format=s16le --rate=44100 --channels=1 < /tmp/audio.pipe
# done
