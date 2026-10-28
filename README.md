# flac-tool
 
Tool to make FLAC audio files compatible with the IW engine.  

# How

* Download [flac](https://ftp.osuosl.org/pub/xiph/releases/flac/) and encode any **WAV** sound you want into **FLAC** making sure you have this flag set: `--blocksize=1024`
* Convert the encoded **FLAC** with flac-tool: `flac-tool <path to flac>`

# Flags

| Name | Shortname | Description | 
| --- | --- | --- |
| --ignore-blocksize | -i | Does not check if the blocksize is `1024`, conversion will succeed but the sound is not guaranteed to work correctly |
| --output | -o | Specifies the output path |

# IW Engine FLAC requirements
* Must have a [**Metadata Block of type APPLICATION**](https://xiph.org/flac/format.html#metadata_block_application) with the following data:

  | Size/Type | Description | Value |
  | --- | --- | --- |
  | 4 byte char array | Application ID | "psiz" |
  | 4 byte unsigned int | Application Data | Size of the frames section (from the start of the first frame to the end of the file) |
  
  **This does not include the 4 byte header that describes the metadata block type and length ([more info on that can be found here](https://xiph.org/flac/format.html#metadata_block_header))**
  
* Must have a [**Metadata Block of type SEEKTABLE**](https://xiph.org/flac/format.html#metadata_block_seektable), its contents don't matter it just has to be there.  
  This is usually already present in all flac files, but if it isn't it is sufficient to insert the following bytes before the last metadata block:  
  `03 00 00 00`
  
