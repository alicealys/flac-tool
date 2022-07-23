# flac-tool
 
Tool to make .flac audio files compatible with the IW engine

# How

* Download [flac](https://ftp.osuosl.org/pub/xiph/releases/flac/) and encode any .wav/.mp3/etc... file you want into .flac making sure you have this flag set: `--blocksize=1024`
* Convert the encoded .flac with flac-tool like this: `flac-tool <path to flac>`
