# I.R.Choir local correction to JUCE 8.0.6 FlacReader sample scaling.
# Left-shifting negative signed samples is undefined in C++17. Scaling through
# int64 preserves the signed PCM value without signed-shift UB. Valid FLAC
# sample depths keep the result within int32 range. No sanitizer is disabled.
set(ir_flac_source "${juce_SOURCE_DIR}/modules/juce_audio_formats/codecs/juce_FlacAudioFormat.cpp")
file(READ "${ir_flac_source}" ir_flac_content)
set(ir_flac_before "dest[j] = src[j] << bitsToShift;")
set(ir_flac_after "dest[j] = static_cast<int> (static_cast<int64> (src[j]) * (int64 { 1 } << bitsToShift)); // I.R.Choir: defined signed PCM scaling")
string(FIND "${ir_flac_content}" "${ir_flac_before}" ir_flac_match)
if(NOT ir_flac_match EQUAL -1)
    string(REPLACE "${ir_flac_before}" "${ir_flac_after}" ir_flac_content "${ir_flac_content}")
    file(WRITE "${ir_flac_source}" "${ir_flac_content}")
else()
    string(FIND "${ir_flac_content}" "${ir_flac_after}" ir_flac_applied)
    if(ir_flac_applied EQUAL -1)
        message(FATAL_ERROR "Pinned JUCE FLAC source differs; review sample scaling patch")
    endif()
endif()
